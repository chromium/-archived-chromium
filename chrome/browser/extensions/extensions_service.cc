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
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/unzip.h"

#if defined(OS_WIN)
#include "base/registry.h"
#include "chrome/common/win_util.h"
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
}


ExtensionsService::ExtensionsService(const FilePath& profile_directory,
                                     UserScriptMaster* user_script_master)
    : message_loop_(MessageLoop::current()),
      backend_(new ExtensionsServiceBackend),
      install_directory_(profile_directory.AppendASCII(kInstallDirectoryName)),
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
          install_directory_,
          scoped_refptr<ExtensionsServiceFrontendInterface>(this)));
#endif

  // TODO(aa): This message loop should probably come from a backend
  // interface, similar to how the message loop for the frontend comes
  // from the frontend interface.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(backend_.get(),
          &ExtensionsServiceBackend::LoadExtensionsFromDirectory,
          install_directory_,
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
          install_directory_,
          true,  // alert_on_error
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

void ExtensionsService::OnExtensionsLoadedFromDirectory(
    ExtensionList* new_extensions) {
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

  // Tell UserScriptMaster to kick off the first scan.
  user_script_master_->StartScan();

  NotificationService::current()->Notify(
      NotificationType::EXTENSIONS_LOADED,
      NotificationService::AllSources(),
      Details<ExtensionList>(new_extensions));

  delete new_extensions;
}

void ExtensionsService::OnExtensionLoadError(bool alert_on_error,
                                             const std::string& error) {
  // TODO(aa): Print the error message out somewhere better. I think we are
  // going to need some sort of 'extension inspector'.
  LOG(WARNING) << error;
  if (alert_on_error) {
#if defined(OS_WIN)
    win_util::MessageBox(NULL, UTF8ToWide(error),
        L"Extension load error", MB_OK | MB_SETFOREGROUND);
#endif
  }
}

void ExtensionsService::OnExtensionInstallError(bool alert_on_error,
                                                const std::string& error) {
  // TODO(erikkay): Print the error message out somewhere better.
  LOG(WARNING) << error;
  if (alert_on_error) {
#if defined(OS_WIN)
    win_util::MessageBox(NULL, UTF8ToWide(error),
        L"Extension load error", MB_OK | MB_SETFOREGROUND);
#endif
  }
}

void ExtensionsService::OnExtensionInstalled(FilePath path, bool update) {
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_INSTALLED,
      NotificationService::AllSources(),
      Details<FilePath>(&path));

  // TODO(erikkay): Update UI if appropriate.
}


// ExtensionsServicesBackend

void ExtensionsServiceBackend::LoadExtensionsFromDirectory(
    const FilePath& path_in,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  FilePath path = path_in;
  frontend_ = frontend;
  alert_on_error_ = false;

  if (!file_util::AbsolutePath(&path))
    NOTREACHED();

  scoped_ptr<ExtensionList> extensions(new ExtensionList);

  // Create the <Profile>/Extensions directory if it doesn't exist.
  if (!file_util::DirectoryExists(path)) {
    file_util::CreateDirectory(path);
    LOG(INFO) << "Created Extensions directory.  No extensions to install.";
    ReportExtensionsLoaded(extensions.release());
    return;
  }

  LOG(INFO) << "Loading installed extensions...";

  // Find all child directories in the install directory and load their
  // manifests. Post errors and results to the frontend.
  file_util::FileEnumerator enumerator(path,
                                       false,  // not recursive
                                       file_util::FileEnumerator::DIRECTORIES);
  for (extension_path_ = enumerator.Next(); !extension_path_.value().empty();
       extension_path_ = enumerator.Next()) {
    Extension* extension = LoadExtensionCurrentVersion();
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

  extension_path_ = path_in;
  if (!file_util::AbsolutePath(&extension_path_))
    NOTREACHED();

  LOG(INFO) << "Loading single extension from " <<
      WideToASCII(extension_path_.BaseName().ToWStringHack());

  Extension* extension = LoadExtension();
  if (extension) {
    ExtensionList* extensions = new ExtensionList;
    extensions->push_back(extension);
    ReportExtensionsLoaded(extensions);
  }
}

Extension* ExtensionsServiceBackend::LoadExtensionCurrentVersion() {
  std::string version_str;
  if (!ReadCurrentVersion(extension_path_, &version_str)) {
    ReportExtensionLoadError(StringPrintf("Could not read '%s' file.",
        ExtensionsService::kCurrentVersionFileName));
    return NULL;
  }

  LOG(INFO) << "  " <<
      WideToASCII(extension_path_.BaseName().ToWStringHack()) <<
      " version: " << version_str;

  extension_path_ = extension_path_.AppendASCII(version_str);
  return LoadExtension();
}

Extension* ExtensionsServiceBackend::LoadExtension() {
  FilePath manifest_path =
      extension_path_.AppendASCII(Extension::kManifestFilename);
  if (!file_util::PathExists(manifest_path)) {
    ReportExtensionLoadError(Extension::kInvalidManifestError);
    return NULL;
  }

  JSONFileValueSerializer serializer(manifest_path.ToWStringHack());
  std::string error;
  scoped_ptr<Value> root(serializer.Deserialize(&error));
  if (!root.get()) {
    ReportExtensionLoadError(error);
    return NULL;
  }

  if (!root->IsType(Value::TYPE_DICTIONARY)) {
    ReportExtensionLoadError(Extension::kInvalidManifestError);
    return NULL;
  }

  scoped_ptr<Extension> extension(new Extension(extension_path_));
  if (!extension->InitFromValue(*static_cast<DictionaryValue*>(root.get()),
                                &error)) {
    ReportExtensionLoadError(error);
    return NULL;
  }

  if (CheckExternalUninstall(extension_path_, extension->id())) {

    // TODO(erikkay): Possibly defer this operation to avoid slowing initial
    // load of extensions.
    UninstallExtension(extension_path_);

    // No error needs to be reported.  The extension effectively doesn't exist.
    return NULL;
  }

  // Validate that claimed resources actually exist.
  for (UserScriptList::const_iterator iter =
       extension->content_scripts().begin();
       iter != extension->content_scripts().end(); ++iter) {
    if (!file_util::PathExists(iter->path())) {
      ReportExtensionLoadError(
          StringPrintf("Could not load content script '%s'.",
                       WideToUTF8(iter->path().ToWStringHack()).c_str()));
      return NULL;
    }
  }

  return extension.release();
}

void ExtensionsServiceBackend::ReportExtensionLoadError(
    const std::string &error) {

  // TODO(port): note that this isn't guaranteed to work properly on Linux.
  std::string path_str = WideToASCII(extension_path_.ToWStringHack());
  std::string message = StringPrintf("Could not load extension from '%s'. %s",
                                     path_str.c_str(), error.c_str());
  frontend_->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_, &ExtensionsServiceFrontendInterface::OnExtensionLoadError,
      alert_on_error_, message));
}

void ExtensionsServiceBackend::ReportExtensionsLoaded(
    ExtensionList* extensions) {
  frontend_->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_,
      &ExtensionsServiceFrontendInterface::OnExtensionsLoadedFromDirectory,
      extensions));
}

// The extension file format is a header, followed by the manifest, followed
// by the zip file.  The header is a magic number, a version, the size of the
// header, and the size of the manifest.  These ints are 4 byte little endian.
DictionaryValue* ExtensionsServiceBackend::ReadManifest() {
  ScopedStdioHandle file(file_util::OpenFile(extension_path_, "rb"));
  if (!file.get()) {
    ReportExtensionInstallError("no such extension file");
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
    ReportExtensionInstallError("invalid extension header");
    return NULL;
  }
  if (strncmp(kExtensionFileMagic, header.magic, sizeof(header.magic))) {
    ReportExtensionInstallError("bad magic number");
    return NULL;
  }
  if (header.version != Extension::kExpectedFormatVersion) {
    ReportExtensionInstallError("bad version number");
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
    ReportExtensionInstallError(error);
    return NULL;
  }
  if (!val->IsType(Value::TYPE_DICTIONARY)) {
    ReportExtensionInstallError("manifest isn't a JSON dictionary");
    return NULL;
  }
  DictionaryValue* manifest = static_cast<DictionaryValue*>(val.get());

  // Check the version before proceeding.  Although we verify the version
  // again later, checking it here allows us to skip some potentially expensive
  // work.
  std::string id;
  if (!manifest->GetString(Extension::kIdKey, &id)) {
    ReportExtensionInstallError("missing id key");
    return NULL;
  }
  FilePath dest_dir = install_directory_.AppendASCII(id.c_str());
  if (file_util::PathExists(dest_dir)) {
    std::string version;
    if (!manifest->GetString(Extension::kVersionKey, &version)) {
      ReportExtensionInstallError("missing version key");
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
    ReportExtensionInstallError("missing zip_hash key");
    return NULL;
  }
  if (zip_hash.size() != kZipHashHexBytes) {
    ReportExtensionInstallError("invalid zip_hash key");
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
    ReportExtensionInstallError("invalid zip_hash key");
    return NULL;
  }
  if (zip_hash_bytes.size() != kZipHashBytes) {
    ReportExtensionInstallError("invalid zip_hash key");
    return NULL;
  }
  for (size_t i = 0; i < kZipHashBytes; ++i) {
    if (zip_hash_bytes[i] != hash[i]) {
      ReportExtensionInstallError("zip_hash key didn't match zip hash");
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
      ReportExtensionInstallError(
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
      ReportExtensionInstallError(
          "Can't delete existing version directory.");
      return false;
    }
  } else {
    FilePath parent = dest_dir.DirName();
    if (!file_util::DirectoryExists(parent)) {
      if (!file_util::CreateDirectory(parent)) {
        ReportExtensionInstallError("Couldn't create extension directory.");
        return false;
      }
    }
  }
  if (!file_util::Move(source_dir, dest_dir)) {
    ReportExtensionInstallError("Couldn't move temporary directory.");
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
      ReportExtensionInstallError("Couldn't remove CurrentVersion_old file.");
      return false;
    }
  }
  if (file_util::PathExists(current_version)) {
    if (!file_util::Move(current_version, current_version_old)) {
      ReportExtensionInstallError("Couldn't move CurrentVersion file.");
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
    ReportExtensionInstallError("Couldn't create CurrentVersion file.");
    return false;
  }
  return true;
}

void ExtensionsServiceBackend::InstallExtension(
    const FilePath& extension_path,
    const FilePath& install_dir,
    bool alert_on_error,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  LOG(INFO) << "Installing extension " << extension_path.value();

  frontend_ = frontend;
  alert_on_error_ = alert_on_error;
  external_install_ = false;
  extension_path_ = extension_path;
  install_directory_ = install_dir;

  InstallOrUpdateExtension(std::string());
}

void ExtensionsServiceBackend::InstallOrUpdateExtension(
    const std::string& expected_id) {
  bool update = false;

  // Read and verify the extension.
  scoped_ptr<DictionaryValue> manifest(ReadManifest());
  if (!manifest.get()) {

    // ReadManifest has already reported the extension error.
    return;
  }
  DictionaryValue* dict = manifest.get();
  Extension extension;
  std::string error;
  if (!extension.InitFromValue(*dict, &error)) {
    ReportExtensionInstallError("Invalid extension manifest.");
    return;
  }

  // If an expected id was provided, make sure it matches.
  if (expected_id.length() && expected_id != extension.id()) {
    ReportExtensionInstallError(
        "ID in new extension manifest does not match expected ID.");
    return;
  }

  // <profile>/Extensions/<id>
  FilePath dest_dir = install_directory_.AppendASCII(extension.id());
  std::string version = extension.VersionString();
  std::string current_version;
  if (ReadCurrentVersion(dest_dir, &current_version)) {
    if (!CheckCurrentVersion(version, current_version, dest_dir))
      return;
    update = true;
  }

  // <profile>/Extensions/INSTALL_TEMP
  FilePath temp_dir = install_directory_.AppendASCII(kTempExtensionName);

  // Ensure we're starting with a clean slate.
  if (file_util::PathExists(temp_dir)) {
    if (!file_util::Delete(temp_dir, true)) {
      ReportExtensionInstallError(
          "Couldn't delete existing temporary directory.");
      return;
    }
  }
  ScopedTempDir scoped_temp;
  scoped_temp.Set(temp_dir);
  if (!scoped_temp.IsValid()) {
    ReportExtensionInstallError("Couldn't create temporary directory.");
    return;
  }

  // <profile>/Extensions/INSTALL_TEMP/<version>
  FilePath temp_version = temp_dir.AppendASCII(version);
  file_util::CreateDirectory(temp_version);
  if (!Unzip(extension_path_, temp_version, NULL)) {
    ReportExtensionInstallError("Couldn't unzip extension.");
    return;
  }

  // <profile>/Extensions/<dir_name>/<version>
  FilePath version_dir = dest_dir.AppendASCII(version);
  if (!InstallDirSafely(temp_version, version_dir))
    return;

  if (!SetCurrentVersion(dest_dir, version)) {
    if (!file_util::Delete(version_dir, true))
      LOG(WARNING) << "Can't remove " << dest_dir.value();
    return;
  }

  if (external_install_) {

    // To mark that this extension was installed from an external source,
    // create a zero-length file.  At load time, this is used to indicate
    // that the extension should be uninstalled.
    // TODO(erikkay): move this into per-extension config storage when
    // it appears.
    FilePath marker = version_dir.AppendASCII(kExternalInstallFile);
    file_util::WriteFile(marker, NULL, 0);
  }

  ReportExtensionInstalled(dest_dir, update);
}

void ExtensionsServiceBackend::ReportExtensionInstallError(
    const std::string &error) {

  // TODO(erikkay): note that this isn't guaranteed to work properly on Linux.
  std::string path_str = WideToASCII(extension_path_.ToWStringHack());
  std::string message =
      StringPrintf("Could not install extension from '%s'. %s",
                   path_str.c_str(), error.c_str());
  frontend_->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_, &ExtensionsServiceFrontendInterface::OnExtensionInstallError,
      alert_on_error_, message));
}

void ExtensionsServiceBackend::ReportExtensionInstalled(
    FilePath path, bool update) {
  frontend_->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_,
      &ExtensionsServiceFrontendInterface::OnExtensionInstalled,
      path,
      update));

  // After it's installed, load it right away with the same settings.
  extension_path_ = path;
  LoadExtensionCurrentVersion();
}

// Some extensions will autoupdate themselves externally from Chrome.  These
// are typically part of some larger client application package.  To support
// these, the extension will register its location in the registry on Windows
// (TODO(port): what about on other platforms?) and this code will periodically
// check that location for a .crx file, which it will then install locally if
// a new version is available.
void ExtensionsServiceBackend::CheckForExternalUpdates(
    const FilePath& install_dir,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {

  // Note that this installation is intentionally silent (since it didn't
  // go through the front-end).  Extensions that are registered in this
  // way are effectively considered 'pre-bundled', and so implicitly
  // trusted.  In general, if something has HKLM or filesystem access,
  // they could install an extension manually themselves anyway.
  alert_on_error_ = false;
  frontend_ = frontend;
  external_install_ = true;
  install_directory_ = install_dir;

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
        extension_path_ = FilePath(extension_path);
        std::wstring extension_version;
        if (key.ReadValue(kRegistryExtensionVersion, &extension_version)) {
          if (ShouldInstall(id, WideToASCII(extension_version)))
            InstallOrUpdateExtension(id);
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
