// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_service.h"

#include "base/file_util.h"
#include "base/scoped_handle.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/third_party/nss/blapi.h"
#include "base/third_party/nss/sha256.h"
#include "base/thread.h"
#include "base/values.h"
#include "net/base/file_stream.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/unzip.h"

#if defined(OS_WIN)
#include "base/registry.h"
#endif

// ExtensionsService

const char* ExtensionsService::kInstallDirectoryName = "Extensions";
const char* ExtensionsService::kCurrentVersionFileName = "Current Version";
const char* ExtensionsServiceBackend::kTempExtensionName = "TEMP_INSTALL";

namespace {
// Chromium Extension magic number
const char kExtensionFileMagic[] = "Cr24";

struct ExtensionHeader {
  char magic[sizeof(kExtensionFileMagic) - 1];
  uint32 version;
  size_t header_size;
  size_t manifest_size;
};

const size_t kZipHashBytes = 32;  // SHA-256
const size_t kZipHashHexBytes = kZipHashBytes * 2;  // Hex string is 2x size.

#if defined(OS_WIN)

// Registry key where registry defined extension installers live.
const wchar_t kRegistryExtensions[] = L"Software\\Google\\Chrome\\Extensions";

// Registry value of of that key that defines the path to the .crx file.
const wchar_t kRegistryExtensionPath[] = L"path";

// Registry value of that key that defines the current version of the .crx file.
const wchar_t kRegistryExtensionVersion[] = L"version";

#endif

// A marker file to indicate that an extension was installed from an external
// source.
const char kExternalInstallFile[] = "EXTERNAL_INSTALL";

// The version of the extension package that this code understands.
const uint32 kExpectedVersion = 1;
}

ExtensionsService::ExtensionsService(Profile* profile,
                                     UserScriptMaster* user_script_master)
    : message_loop_(MessageLoop::current()),
      install_directory_(profile->GetPath().AppendASCII(kInstallDirectoryName)),
      backend_(new ExtensionsServiceBackend(install_directory_)),
      profile_(profile),
      user_script_master_(user_script_master) {
}

ExtensionsService::~ExtensionsService() {
  for (ExtensionList::iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    delete *iter;
  }
}

bool ExtensionsService::Init() {
#if defined(OS_WIN)
  // TODO(port): ExtensionsServiceBackend::CheckForExternalUpdates depends on
  // the Windows registry.
  // TODO(erikkay): Should we monitor the registry during run as well?
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(backend_.get(),
          &ExtensionsServiceBackend::CheckForExternalUpdates,
          scoped_refptr<ExtensionsServiceFrontendInterface>(this)));
#endif

  // TODO(aa): This message loop should probably come from a backend
  // interface, similar to how the message loop for the frontend comes
  // from the frontend interface.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(backend_.get(),
          &ExtensionsServiceBackend::LoadExtensionsFromInstallDirectory,
          scoped_refptr<ExtensionsServiceFrontendInterface>(this)));

  return true;
}

MessageLoop* ExtensionsService::GetMessageLoop() {
  return message_loop_;
}

void ExtensionsService::InstallExtension(const FilePath& extension_path) {
  // TODO(aa): This message loop should probably come from a backend
  // interface, similar to how the message loop for the frontend comes
  // from the frontend interface.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(backend_.get(),
          &ExtensionsServiceBackend::InstallExtension,
          extension_path,
          scoped_refptr<ExtensionsServiceFrontendInterface>(this)));
}

void ExtensionsService::LoadExtension(const FilePath& extension_path) {
  // TODO(aa): This message loop should probably come from a backend
  // interface, similar to how the message loop for the frontend comes
  // from the frontend interface.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(backend_.get(),
          &ExtensionsServiceBackend::LoadSingleExtension,
          extension_path,
          scoped_refptr<ExtensionsServiceFrontendInterface>(this)));
}

void ExtensionsService::OnExtensionsLoaded(ExtensionList* new_extensions) {
  extensions_.insert(extensions_.end(), new_extensions->begin(),
                     new_extensions->end());

  // TODO: Fix race here.  A page could need a user script on startup, before
  // the user script is loaded.  We need to freeze the renderer in that case.
  // TODO(mpcomplete): We also need to force a renderer to refresh its cache of
  // the plugin list when we inject user scripts, since it could have a stale
  // version by the time extensions are loaded.

  for (ExtensionList::iterator extension = extensions_.begin();
       extension != extensions_.end(); ++extension) {
    // Tell NPAPI about any plugins in the loaded extensions.
    if (!(*extension)->plugins_dir().empty()) {
      PluginService::GetInstance()->AddExtraPluginDir(
          (*extension)->plugins_dir());
    }

    // Tell UserScriptMaster about any scripts in the loaded extensions.
    const UserScriptList& scripts = (*extension)->content_scripts();
    for (UserScriptList::const_iterator script = scripts.begin();
         script != scripts.end(); ++script) {
      user_script_master_->AddLoneScript(*script);
    }
  }

  // Since user scripts may have changed, tell UserScriptMaster to kick off
  // a scan.
  user_script_master_->StartScan();

  NotificationService::current()->Notify(
      NotificationType::EXTENSIONS_LOADED,
      NotificationService::AllSources(),
      Details<ExtensionList>(new_extensions));

  delete new_extensions;
}

void ExtensionsService::OnExtensionInstalled(FilePath path, bool update) {
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_INSTALLED,
      NotificationService::AllSources(),
      Details<FilePath>(&path));

  // TODO(erikkay): Update UI if appropriate.
}


// ExtensionsServicesBackend

void ExtensionsServiceBackend::LoadExtensionsFromInstallDirectory(
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  frontend_ = frontend;
  alert_on_error_ = false;

#if defined(OS_WIN)
  // On POSIX, AbsolutePath() calls realpath() which returns NULL if
  // it does not exist.  Instead we absolute-ify after creation in
  // case that is needed.
  // TODO(port): does this really need to happen before
  // CreateDirectory() on Windows?
  if (!file_util::AbsolutePath(&install_directory_))
    NOTREACHED();
#endif

  scoped_ptr<ExtensionList> extensions(new ExtensionList);

  // Create the <Profile>/Extensions directory if it doesn't exist.
  if (!file_util::DirectoryExists(install_directory_)) {
    file_util::CreateDirectory(install_directory_);
    LOG(INFO) << "Created Extensions directory.  No extensions to install.";
    ReportExtensionsLoaded(extensions.release());
    return;
  }

#if !defined(OS_WIN)
  if (!file_util::AbsolutePath(&install_directory_))
    NOTREACHED();
#endif

  LOG(INFO) << "Loading installed extensions...";

  // Find all child directories in the install directory and load their
  // manifests. Post errors and results to the frontend.
  file_util::FileEnumerator enumerator(install_directory_,
                                       false,  // not recursive
                                       file_util::FileEnumerator::DIRECTORIES);
  FilePath extension_path;
  for (extension_path = enumerator.Next(); !extension_path.value().empty();
       extension_path = enumerator.Next()) {
    std::string extension_id = WideToASCII(
        extension_path.BaseName().ToWStringHack());
    if (CheckExternalUninstall(extension_path, extension_id)) {
      // TODO(erikkay): Possibly defer this operation to avoid slowing initial
      // load of extensions.
      UninstallExtension(extension_path);

      // No error needs to be reported.  The extension effectively doesn't
      // exist.
      continue;
    }

    Extension* extension = LoadExtensionCurrentVersion(extension_path);
    if (extension)
      extensions->push_back(extension);
  }

  LOG(INFO) << "Done.";
  ReportExtensionsLoaded(extensions.release());
}

void ExtensionsServiceBackend::LoadSingleExtension(
    const FilePath& path_in,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  frontend_ = frontend;

  // Explicit UI loads are always noisy.
  alert_on_error_ = true;

  FilePath extension_path = path_in;
  if (!file_util::AbsolutePath(&extension_path))
    NOTREACHED();

  LOG(INFO) << "Loading single extension from " <<
      WideToASCII(extension_path.BaseName().ToWStringHack());

  Extension* extension = LoadExtension(extension_path);
  if (extension) {
    if (extension->id().empty()) {
      // Generate an ID
      static int counter = 0;
      std::string id = StringPrintf("%x", counter);
      ++counter;

      // pad the string out to 40 chars with zeroes.
      id.insert(0, 40 - id.length(), '0');
      extension->set_id(id);
    }
    ExtensionList* extensions = new ExtensionList;
    extensions->push_back(extension);
    ReportExtensionsLoaded(extensions);
  }
}

Extension* ExtensionsServiceBackend::LoadExtensionCurrentVersion(
    const FilePath& extension_path) {
  std::string version_str;
  if (!ReadCurrentVersion(extension_path, &version_str)) {
    ReportExtensionLoadError(extension_path,
        StringPrintf("Could not read '%s' file.",
            ExtensionsService::kCurrentVersionFileName));
    return NULL;
  }

  LOG(INFO) << "  " <<
      WideToASCII(extension_path.BaseName().ToWStringHack()) <<
      " version: " << version_str;

  return LoadExtension(extension_path.AppendASCII(version_str));
}

Extension* ExtensionsServiceBackend::LoadExtension(
    const FilePath& extension_path) {
  FilePath manifest_path =
      extension_path.AppendASCII(Extension::kManifestFilename);
  if (!file_util::PathExists(manifest_path)) {
    ReportExtensionLoadError(extension_path, Extension::kInvalidManifestError);
    return NULL;
  }

  JSONFileValueSerializer serializer(manifest_path.ToWStringHack());
  std::string error;
  scoped_ptr<Value> root(serializer.Deserialize(&error));
  if (!root.get()) {
    ReportExtensionLoadError(extension_path, error);
    return NULL;
  }

  if (!root->IsType(Value::TYPE_DICTIONARY)) {
    ReportExtensionLoadError(extension_path, Extension::kInvalidManifestError);
    return NULL;
  }

  scoped_ptr<Extension> extension(new Extension(extension_path));
  if (!extension->InitFromValue(*static_cast<DictionaryValue*>(root.get()),
                                &error)) {
    ReportExtensionLoadError(extension_path, error);
    return NULL;
  }

  // Validate that claimed resources actually exist.
  for (size_t i = 0; i < extension->content_scripts().size(); ++i) {
    const UserScript& script = extension->content_scripts()[i];

    for (size_t j = 0; j < script.js_scripts().size(); j++) {
      const FilePath& path = script.js_scripts()[j].path();
      if (!file_util::PathExists(path)) {
        ReportExtensionLoadError(extension_path,
          StringPrintf("Could not load '%s' for content script.",
          WideToUTF8(path.ToWStringHack()).c_str()));
        return NULL;
      }
    }

    for (size_t j = 0; j < script.css_scripts().size(); j++) {
      const FilePath& path = script.css_scripts()[j].path();
      if (!file_util::PathExists(path)) {
        ReportExtensionLoadError(extension_path,
          StringPrintf("Could not load '%s' for content script.",
          WideToUTF8(path.ToWStringHack()).c_str()));
        return NULL;
      }
    }
  }

  return extension.release();
}

void ExtensionsServiceBackend::ReportExtensionLoadError(
    const FilePath& extension_path, const std::string &error) {
  // TODO(port): note that this isn't guaranteed to work properly on Linux.
  std::string path_str = WideToASCII(extension_path.ToWStringHack());
  std::string message = StringPrintf("Could not load extension from '%s'. %s",
                                     path_str.c_str(), error.c_str());
  ExtensionErrorReporter::GetInstance()->ReportError(message, alert_on_error_);
}

void ExtensionsServiceBackend::ReportExtensionsLoaded(
    ExtensionList* extensions) {
  frontend_->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_,
      &ExtensionsServiceFrontendInterface::OnExtensionsLoaded,
      extensions));
}

// The extension file format is a header, followed by the manifest, followed
// by the zip file.  The header is a magic number, a version, the size of the
// header, and the size of the manifest.  These ints are 4 byte little endian.
DictionaryValue* ExtensionsServiceBackend::ReadManifest(
    const FilePath& extension_path) {
  ScopedStdioHandle file(file_util::OpenFile(extension_path, "rb"));
  if (!file.get()) {
    ReportExtensionInstallError(extension_path, "no such extension file");
    return NULL;
  }

  // Read and verify the header.
  ExtensionHeader header;
  size_t len;

  // TODO(erikkay): Yuck.  I'm not a big fan of this kind of code, but it
  // appears that we don't have any endian/alignment aware serialization
  // code in the code base.  So for now, this assumes that we're running
  // on a little endian machine with 4 byte alignment.
  len = fread(&header, 1, sizeof(ExtensionHeader), file.get());
  if (len < sizeof(ExtensionHeader)) {
    ReportExtensionInstallError(extension_path, "invalid extension header");
    return NULL;
  }
  if (strncmp(kExtensionFileMagic, header.magic, sizeof(header.magic))) {
    ReportExtensionInstallError(extension_path, "bad magic number");
    return NULL;
  }
  if (header.version != kExpectedVersion) {
    ReportExtensionInstallError(extension_path, "bad version number");
    return NULL;
  }
  if (header.header_size > sizeof(ExtensionHeader))
    fseek(file.get(), header.header_size - sizeof(ExtensionHeader), SEEK_CUR);

  char buf[1 << 16];
  std::string manifest_str;
  size_t read_size = std::min(sizeof(buf), header.manifest_size);
  size_t remainder = header.manifest_size;
  while ((len = fread(buf, 1, read_size, file.get())) > 0) {
    manifest_str.append(buf, len);
    if (len <= remainder)
      break;
    remainder -= len;
    read_size = std::min(sizeof(buf), remainder);
  }

  // Verify the JSON
  JSONStringValueSerializer json(manifest_str);
  std::string error;
  scoped_ptr<Value> val(json.Deserialize(&error));
  if (!val.get()) {
    ReportExtensionInstallError(extension_path, error);
    return NULL;
  }
  if (!val->IsType(Value::TYPE_DICTIONARY)) {
    ReportExtensionInstallError(extension_path,
                                "manifest isn't a JSON dictionary");
    return NULL;
  }
  DictionaryValue* manifest = static_cast<DictionaryValue*>(val.get());

  // Check the version before proceeding.  Although we verify the version
  // again later, checking it here allows us to skip some potentially expensive
  // work.
  std::string id;
  if (!manifest->GetString(Extension::kIdKey, &id)) {
    ReportExtensionInstallError(extension_path, "missing id key");
    return NULL;
  }
  FilePath dest_dir = install_directory_.AppendASCII(id.c_str());
  if (file_util::PathExists(dest_dir)) {
    std::string version;
    if (!manifest->GetString(Extension::kVersionKey, &version)) {
      ReportExtensionInstallError(extension_path, "missing version key");
      return NULL;
    }
    std::string current_version;
    if (ReadCurrentVersion(dest_dir, &current_version)) {
      if (!CheckCurrentVersion(version, current_version, dest_dir))
        return NULL;
    }
  }

  std::string zip_hash;
  if (!manifest->GetString(Extension::kZipHashKey, &zip_hash)) {
    ReportExtensionInstallError(extension_path, "missing zip_hash key");
    return NULL;
  }
  if (zip_hash.size() != kZipHashHexBytes) {
    ReportExtensionInstallError(extension_path, "invalid zip_hash key");
    return NULL;
  }

  // Read the rest of the zip file and compute a hash to compare against
  // what the manifest claims.  Compute the hash incrementally since the
  // zip file could be large.
  const unsigned char* ubuf = reinterpret_cast<const unsigned char*>(buf);
  SHA256Context ctx;
  SHA256_Begin(&ctx);
  while ((len = fread(buf, 1, sizeof(buf), file.get())) > 0)
    SHA256_Update(&ctx, ubuf, len);
  uint8 hash[32];
  SHA256_End(&ctx, hash, NULL, sizeof(hash));

  std::vector<uint8> zip_hash_bytes;
  if (!HexStringToBytes(zip_hash, &zip_hash_bytes)) {
    ReportExtensionInstallError(extension_path, "invalid zip_hash key");
    return NULL;
  }
  if (zip_hash_bytes.size() != kZipHashBytes) {
    ReportExtensionInstallError(extension_path, "invalid zip_hash key");
    return NULL;
  }
  for (size_t i = 0; i < kZipHashBytes; ++i) {
    if (zip_hash_bytes[i] != hash[i]) {
      ReportExtensionInstallError(extension_path,
                                  "zip_hash key didn't match zip hash");
      return NULL;
    }
  }

  // TODO(erikkay): The manifest will also contain a signature of the hash
  // (or perhaps the whole manifest) for authentication purposes.

  // The caller owns val (now cast to manifest).
  val.release();
  return manifest;
}

bool ExtensionsServiceBackend::ReadCurrentVersion(const FilePath& dir,
                                                  std::string* version_string) {
  FilePath current_version =
      dir.AppendASCII(ExtensionsService::kCurrentVersionFileName);
  if (file_util::PathExists(current_version)) {
    if (file_util::ReadFileToString(current_version, version_string)) {
      TrimWhitespace(*version_string, TRIM_ALL, version_string);
      return true;
    }
  }
  return false;
}

bool ExtensionsServiceBackend::CheckCurrentVersion(
    const std::string& new_version_str,
    const std::string& current_version_str,
    const FilePath& dest_dir) {
  scoped_ptr<Version> current_version(
      Version::GetVersionFromString(current_version_str));
  scoped_ptr<Version> new_version(
      Version::GetVersionFromString(new_version_str));
  if (current_version->CompareTo(*new_version) >= 0) {
    // Verify that the directory actually exists.  If it doesn't we'll return
    // true so that the install code will repair the broken installation.
    // TODO(erikkay): A further step would be to verify that the extension
    // has actually loaded successfully.
    FilePath version_dir = dest_dir.AppendASCII(current_version_str);
    if (file_util::PathExists(version_dir)) {
      ReportExtensionInstallError(dest_dir,
          "Existing version is already up to date.");
      return false;
    }
  }
  return true;
}

bool ExtensionsServiceBackend::InstallDirSafely(const FilePath& source_dir,
                                                const FilePath& dest_dir) {

  if (file_util::PathExists(dest_dir)) {
    // By the time we get here, it should be safe to assume that this directory
    // is not currently in use (it's not the current active version).
    if (!file_util::Delete(dest_dir, true)) {
      ReportExtensionInstallError(source_dir,
          "Can't delete existing version directory.");
      return false;
    }
  } else {
    FilePath parent = dest_dir.DirName();
    if (!file_util::DirectoryExists(parent)) {
      if (!file_util::CreateDirectory(parent)) {
        ReportExtensionInstallError(source_dir,
                                    "Couldn't create extension directory.");
        return false;
      }
    }
  }
  if (!file_util::Move(source_dir, dest_dir)) {
    ReportExtensionInstallError(source_dir,
                                "Couldn't move temporary directory.");
    return false;
  }

  return true;
}

bool ExtensionsServiceBackend::SetCurrentVersion(const FilePath& dest_dir,
                                                 std::string version) {
  // Write out the new CurrentVersion file.
  // <profile>/Extension/<name>/CurrentVersion
  FilePath current_version =
      dest_dir.AppendASCII(ExtensionsService::kCurrentVersionFileName);
  FilePath current_version_old =
    current_version.InsertBeforeExtension(FILE_PATH_LITERAL("_old"));
  if (file_util::PathExists(current_version_old)) {
    if (!file_util::Delete(current_version_old, false)) {
      ReportExtensionInstallError(dest_dir,
                                  "Couldn't remove CurrentVersion_old file.");
      return false;
    }
  }
  if (file_util::PathExists(current_version)) {
    if (!file_util::Move(current_version, current_version_old)) {
      ReportExtensionInstallError(dest_dir,
                                  "Couldn't move CurrentVersion file.");
      return false;
    }
  }
  net::FileStream stream;
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;
  if (stream.Open(current_version, flags) != 0)
    return false;
  if (stream.Write(version.c_str(), version.size(), NULL) < 0) {

    // Restore the old CurrentVersion.
    if (file_util::PathExists(current_version_old)) {
      if (!file_util::Move(current_version_old, current_version)) {
        LOG(WARNING) << "couldn't restore " << current_version_old.value() <<
            " to " << current_version.value();

        // TODO(erikkay): This is an ugly state to be in.  Try harder?
      }
    }
    ReportExtensionInstallError(dest_dir,
                                "Couldn't create CurrentVersion file.");
    return false;
  }
  return true;
}

void ExtensionsServiceBackend::InstallExtension(
    const FilePath& extension_path,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  LOG(INFO) << "Installing extension " << extension_path.value();

  frontend_ = frontend;
  alert_on_error_ = false;

  bool was_update = false;
  FilePath destination_path;
  if (InstallOrUpdateExtension(extension_path,
                               std::string(),  // no expected id
                               &destination_path, &was_update))
    ReportExtensionInstalled(destination_path.DirName(), was_update);
}

bool ExtensionsServiceBackend::InstallOrUpdateExtension(
    const FilePath& source_file, const std::string& expected_id,
    FilePath* version_dir, bool* was_update) {
  if (was_update)
    *was_update = false;

  // Read and verify the extension.
  scoped_ptr<DictionaryValue> manifest(ReadManifest(source_file));
  if (!manifest.get()) {
    // ReadManifest has already reported the extension error.
    return false;
  }
  DictionaryValue* dict = manifest.get();
  Extension extension;
  std::string error;
  if (!extension.InitFromValue(*dict, &error)) {
    ReportExtensionInstallError(source_file,
                                "Invalid extension manifest.");
    return false;
  }

  // ID is required for installed extensions.
  if (extension.id().empty()) {
    ReportExtensionInstallError(source_file, "Required value 'id' is missing.");
    return false;
  }

  // If an expected id was provided, make sure it matches.
  if (expected_id.length() && expected_id != extension.id()) {
    ReportExtensionInstallError(source_file,
        "ID in new extension manifest does not match expected ID.");
    return false;
  }

  // <profile>/Extensions/<id>
  FilePath dest_dir = install_directory_.AppendASCII(extension.id());
  std::string version = extension.VersionString();
  std::string current_version;
  if (ReadCurrentVersion(dest_dir, &current_version)) {
    if (!CheckCurrentVersion(version, current_version, dest_dir))
      return false;
    if (was_update)
      *was_update = true;
  }

  // <profile>/Extensions/INSTALL_TEMP
  FilePath temp_dir = install_directory_.AppendASCII(kTempExtensionName);

  // Ensure we're starting with a clean slate.
  if (file_util::PathExists(temp_dir)) {
    if (!file_util::Delete(temp_dir, true)) {
      ReportExtensionInstallError(source_file,
          "Couldn't delete existing temporary directory.");
      return false;
    }
  }
  ScopedTempDir scoped_temp;
  scoped_temp.Set(temp_dir);
  if (!scoped_temp.IsValid()) {
    ReportExtensionInstallError(source_file,
                                "Couldn't create temporary directory.");
    return false;
  }

  // <profile>/Extensions/INSTALL_TEMP/<version>
  FilePath temp_version = temp_dir.AppendASCII(version);
  file_util::CreateDirectory(temp_version);
  if (!Unzip(source_file, temp_version, NULL)) {
    ReportExtensionInstallError(source_file, "Couldn't unzip extension.");
    return false;
  }

  // <profile>/Extensions/<dir_name>/<version>
  *version_dir = dest_dir.AppendASCII(version);
  if (!InstallDirSafely(temp_version, *version_dir))
    return false;

  if (!SetCurrentVersion(dest_dir, version)) {
    if (!file_util::Delete(*version_dir, true))
      LOG(WARNING) << "Can't remove " << dest_dir.value();
    return false;
  }

  return true;
}

void ExtensionsServiceBackend::ReportExtensionInstallError(
    const FilePath& extension_path,  const std::string &error) {

  // TODO(erikkay): note that this isn't guaranteed to work properly on Linux.
  std::string path_str = WideToASCII(extension_path.ToWStringHack());
  std::string message =
      StringPrintf("Could not install extension from '%s'. %s",
                   path_str.c_str(), error.c_str());
  ExtensionErrorReporter::GetInstance()->ReportError(message, alert_on_error_);
}

void ExtensionsServiceBackend::ReportExtensionInstalled(
    const FilePath& path, bool update) {
  frontend_->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_,
      &ExtensionsServiceFrontendInterface::OnExtensionInstalled,
      path,
      update));

  // After it's installed, load it right away with the same settings.
  LOG(INFO) << "Loading extension " << path.value();
  Extension* extension = LoadExtensionCurrentVersion(path);
  if (extension) {
    // Only one extension, but ReportExtensionsLoaded can handle multiple,
    // so we need to construct a list.
    scoped_ptr<ExtensionList> extensions(new ExtensionList);
    extensions->push_back(extension);
    LOG(INFO) << "Done.";
    // Hand off ownership of the loaded extensions to the frontend.
    ReportExtensionsLoaded(extensions.release());
  }
}

// Some extensions will autoupdate themselves externally from Chrome.  These
// are typically part of some larger client application package.  To support
// these, the extension will register its location in the registry on Windows
// (TODO(port): what about on other platforms?) and this code will periodically
// check that location for a .crx file, which it will then install locally if
// a new version is available.
void ExtensionsServiceBackend::CheckForExternalUpdates(
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {

  // Note that this installation is intentionally silent (since it didn't
  // go through the front-end).  Extensions that are registered in this
  // way are effectively considered 'pre-bundled', and so implicitly
  // trusted.  In general, if something has HKLM or filesystem access,
  // they could install an extension manually themselves anyway.
  alert_on_error_ = false;
  frontend_ = frontend;

#if defined(OS_WIN)
  HKEY reg_root = HKEY_LOCAL_MACHINE;
  RegistryKeyIterator iterator(reg_root, kRegistryExtensions);
  while (iterator.Valid()) {
    RegKey key;
    std::wstring key_path = kRegistryExtensions;
    key_path.append(L"\\");
    key_path.append(iterator.Name());
    if (key.Open(reg_root, key_path.c_str())) {
      std::wstring extension_path;
      if (key.ReadValue(kRegistryExtensionPath, &extension_path)) {
        std::string id = WideToASCII(iterator.Name());
        std::wstring extension_version;
        if (key.ReadValue(kRegistryExtensionVersion, &extension_version)) {
          if (ShouldInstall(id, WideToASCII(extension_version))) {
            FilePath version_dir;
            if (InstallOrUpdateExtension(FilePath(extension_path), id,
                                         &version_dir, NULL)) {
              // To mark that this extension was installed from an external
              // source, create a zero-length file.  At load time, this is used
              // to indicate that the extension should be uninstalled.
              // TODO(erikkay): move this into per-extension config storage when
              // it appears.
              FilePath marker = version_dir.AppendASCII(
                  kExternalInstallFile);
              file_util::WriteFile(marker, NULL, 0);
            }
          }
        } else {
          // TODO(erikkay): find a way to get this into about:extensions
          LOG(WARNING) << "Missing value " << kRegistryExtensionVersion <<
              " for key " << key_path;
        }
      } else {
        // TODO(erikkay): find a way to get this into about:extensions
        LOG(WARNING) << "Missing value " << kRegistryExtensionPath <<
            " for key " << key_path;
      }
    }
    ++iterator;
  }
#else
  NOTREACHED();
#endif
}

bool ExtensionsServiceBackend::CheckExternalUninstall(const FilePath& path,
                                                      const std::string& id) {
  FilePath external_file = path.AppendASCII(kExternalInstallFile);
  if (file_util::PathExists(external_file)) {
#if defined(OS_WIN)
    HKEY reg_root = HKEY_LOCAL_MACHINE;
    RegKey key;
    std::wstring key_path = kRegistryExtensions;
    key_path.append(L"\\");
    key_path.append(ASCIIToWide(id));

    // If the key doesn't exist, then we should uninstall.
    return !key.Open(reg_root, key_path.c_str());
#else
    NOTREACHED();
#endif
  }
  return false;
}

// Assumes that the extension isn't currently loaded or in use.
void ExtensionsServiceBackend::UninstallExtension(const FilePath& path) {
  FilePath parent = path.DirName();
  FilePath version =
      parent.AppendASCII(ExtensionsService::kCurrentVersionFileName);
  bool version_exists = file_util::PathExists(version);
  DCHECK(version_exists);
  if (!version_exists) {
    LOG(WARNING) << "Asked to uninstall bogus extension dir " << parent.value();
    return;
  }
  if (!file_util::Delete(parent, true)) {
    LOG(WARNING) << "Failed to delete " << parent.value();
  }
}

bool ExtensionsServiceBackend::ShouldInstall(const std::string& id,
                                             const std::string& version) {
  FilePath dir(install_directory_.AppendASCII(id.c_str()));
  std::string current_version;
  if (ReadCurrentVersion(dir, &current_version)) {
    return !CheckCurrentVersion(version, current_version, dir);
  }
  return true;
}
