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
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/unzip.h"
#if defined(OS_WIN)
#include "chrome/common/win_util.h"
#endif

// ExtensionsService

const char* ExtensionsService::kInstallDirectoryName = "Extensions";
const char* ExtensionsService::kCurrentVersionFileName = "Current Version";
const char* ExtensionsServiceBackend::kTempExtensionName = "TEMP_INSTALL";
// Chromium Extension magic number
static const char kExtensionFileMagic[] = "Cr24";

struct ExtensionHeader {
  char magic[sizeof(kExtensionFileMagic) - 1];
  uint32 version;
  size_t header_size;
  size_t manifest_size;
};

const size_t kZipHashBytes = 32;  // SHA-256
const size_t kZipHashHexBytes = kZipHashBytes * 2;  // Hex string is 2x size.

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
  // TODO(aa): This message loop should probably come from a backend
  // interface, similar to how the message loop for the frontend comes
  // from the frontend interface.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(backend_.get(),
          &ExtensionsServiceBackend::LoadExtensionsFromDirectory,
          install_directory_,
          scoped_refptr<ExtensionsServiceFrontendInterface>(this)));
  // TODO(aa): Load extensions from other registered directories.

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

  // Tell UserScriptMaster about any scripts in the loaded extensions.
  for (ExtensionList::iterator extension = extensions_.begin();
       extension != extensions_.end(); ++extension) {
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

void ExtensionsService::OnExtensionLoadError(const std::string& error) {
  // TODO(aa): Print the error message out somewhere better. I think we are
  // going to need some sort of 'extension inspector'.
  LOG(WARNING) << error;
#if defined(OS_WIN)
  win_util::MessageBox(NULL, UTF8ToWide(error),
      L"Extension load error", MB_OK | MB_SETFOREGROUND);
#endif
}

void ExtensionsService::OnExtensionInstallError(const std::string& error) {
  // TODO(erikkay): Print the error message out somewhere better.
  LOG(WARNING) << error;
#if defined(OS_WIN)
  win_util::MessageBox(NULL, UTF8ToWide(error),
      L"Extension load error", MB_OK | MB_SETFOREGROUND);
#endif
}

void ExtensionsService::OnExtensionInstalled(FilePath path) {
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_INSTALLED,
      NotificationService::AllSources(),
      Details<FilePath>(&path));

  // Immediately try to load the extension.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(backend_.get(),
          &ExtensionsServiceBackend::LoadSingleExtension,
          path,
          scoped_refptr<ExtensionsServiceFrontendInterface>(this)));
}


// ExtensionsServicesBackend

bool ExtensionsServiceBackend::LoadExtensionsFromDirectory(
    const FilePath& path_in,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  FilePath path = path_in;
  if (!file_util::AbsolutePath(&path))
    NOTREACHED();

  // Find all child directories in the install directory and load their
  // manifests. Post errors and results to the frontend.
  scoped_ptr<ExtensionList> extensions(new ExtensionList);
  file_util::FileEnumerator enumerator(path,
                                       false,  // not recursive
                                       file_util::FileEnumerator::DIRECTORIES);
  for (FilePath child_path = enumerator.Next(); !child_path.value().empty();
       child_path = enumerator.Next()) {
    std::string version_str;
    if (!ReadCurrentVersion(child_path, &version_str)) {
      ReportExtensionLoadError(frontend.get(), child_path, StringPrintf(
          "Could not read '%s' file.", 
          ExtensionsService::kCurrentVersionFileName));
      continue;
    }

    child_path = child_path.AppendASCII(version_str);
    Extension* extension = LoadExtension(child_path, frontend);
    if (extension)
      extensions->push_back(extension);
  }

  ReportExtensionsLoaded(frontend.get(), extensions.release());
  return true;
}

bool ExtensionsServiceBackend::LoadSingleExtension(
    const FilePath& path_in,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  FilePath path = path_in;
  if (!file_util::AbsolutePath(&path))
    NOTREACHED();
  Extension* extension = LoadExtension(path, frontend);
  if (extension) {
    ExtensionList* extensions = new ExtensionList;
    extensions->push_back(extension);
    ReportExtensionsLoaded(frontend.get(), extensions);
    return true;
  }
  return false;
}

Extension* ExtensionsServiceBackend::LoadExtension(
    const FilePath& path,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  FilePath manifest_path =
      path.AppendASCII(Extension::kManifestFilename);
  if (!file_util::PathExists(manifest_path)) {
    ReportExtensionLoadError(frontend.get(), path,
                             Extension::kInvalidManifestError);
    return NULL;
  }

  JSONFileValueSerializer serializer(manifest_path.ToWStringHack());
  std::string error;
  scoped_ptr<Value> root(serializer.Deserialize(&error));
  if (!root.get()) {
    ReportExtensionLoadError(frontend.get(), path,
                             error);
    return NULL;
  }

  if (!root->IsType(Value::TYPE_DICTIONARY)) {
    ReportExtensionLoadError(frontend.get(), path,
                             Extension::kInvalidManifestError);
    return NULL;
  }

  scoped_ptr<Extension> extension(new Extension(path));
  if (!extension->InitFromValue(*static_cast<DictionaryValue*>(root.get()),
                                &error)) {
    ReportExtensionLoadError(frontend.get(), path, error);
    return NULL;
  }

  // Validate that claimed resources actually exist.
  for (UserScriptList::const_iterator iter =
       extension->content_scripts().begin();
       iter != extension->content_scripts().end(); ++iter) {
    if (!file_util::PathExists(iter->path())) {
      ReportExtensionLoadError(frontend.get(), path, StringPrintf(
          "Could not load content script '%s'.",
          WideToUTF8(iter->path().ToWStringHack()).c_str()));
      return NULL;
    }
  }

  return extension.release();
}

void ExtensionsServiceBackend::ReportExtensionLoadError(
    ExtensionsServiceFrontendInterface *frontend, const FilePath& path,
    const std::string &error) {
  // TODO(erikkay): note that this isn't guaranteed to work properly on Linux.
  std::string path_str = WideToASCII(path.ToWStringHack());
  std::string message = StringPrintf("Could not load extension from '%s'. %s",
                                     path_str.c_str(), error.c_str());
  frontend->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
      frontend, &ExtensionsServiceFrontendInterface::OnExtensionLoadError,
      message));
}

void ExtensionsServiceBackend::ReportExtensionsLoaded(
    ExtensionsServiceFrontendInterface *frontend, ExtensionList* extensions) {
  frontend->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
      frontend,
      &ExtensionsServiceFrontendInterface::OnExtensionsLoadedFromDirectory,
      extensions));
}

// The extension file format is a header, followed by the manifest, followed
// by the zip file.  The header is a magic number, a version, the size of the
// header, and the size of the manifest.  These ints are 4 byte little endian.
DictionaryValue* ExtensionsServiceBackend::ReadManifest(
      const FilePath& extension_path,
      scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  ScopedStdioHandle file(file_util::OpenFile(extension_path, "rb"));
  if (!file.get()) {
    ReportExtensionInstallError(frontend, extension_path,
                                "no such extension file");
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
    ReportExtensionInstallError(frontend, extension_path,
                                "invalid extension header");
    return NULL;
  }
  if (strncmp(kExtensionFileMagic, header.magic, sizeof(header.magic))) {
    ReportExtensionInstallError(frontend, extension_path,
                                "bad magic number");
    return NULL;
  }
  if (header.version != Extension::kExpectedFormatVersion) {
    ReportExtensionInstallError(frontend, extension_path,
                                "bad version number");
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
    ReportExtensionInstallError(frontend, extension_path, error);
    return NULL;
  }
  if (!val->IsType(Value::TYPE_DICTIONARY)) {
    ReportExtensionInstallError(frontend, extension_path,
                                "manifest isn't a JSON dictionary");
    return NULL;
  }
  DictionaryValue* manifest = static_cast<DictionaryValue*>(val.get());
  std::string zip_hash;
  if (!manifest->GetString(Extension::kZipHashKey, &zip_hash)) {
    ReportExtensionInstallError(frontend, extension_path,
                                "missing zip_hash key");
    return NULL;
  }
  if (zip_hash.size() != kZipHashHexBytes) {
    ReportExtensionInstallError(frontend, extension_path,
                                "invalid zip_hash key");
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
    ReportExtensionInstallError(frontend, extension_path,
                                "invalid zip_hash key");
    return NULL;
  }
  if (zip_hash_bytes.size() != kZipHashBytes) {
    ReportExtensionInstallError(frontend, extension_path,
                                "invalid zip_hash key");
    return NULL;
  }
  for (size_t i = 0; i < kZipHashBytes; ++i) {
    if (zip_hash_bytes[i] != hash[i]) {
      ReportExtensionInstallError(frontend, extension_path,
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

bool ExtensionsServiceBackend::ReadCurrentVersion(
    const FilePath& extension_path,
    std::string* version_string) {
  FilePath current_version =
      extension_path.AppendASCII(ExtensionsService::kCurrentVersionFileName);
  if (file_util::PathExists(current_version)) {
    if (file_util::ReadFileToString(current_version, version_string)) {
      TrimWhitespace(*version_string, TRIM_ALL, version_string);
      return true;
    }
  }
  return false;
}

bool ExtensionsServiceBackend::CheckCurrentVersion(
    const FilePath& extension_path,
    const std::string& version,
    const FilePath& dest_dir,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  std::string version_str;
  if (ReadCurrentVersion(dest_dir, &version_str)) {
    if (version_str == version) {
      FilePath version_dir = dest_dir.AppendASCII(version_str);
      if (file_util::PathExists(version_dir)) {
        ReportExtensionInstallError(frontend, extension_path,
            "Extension version already installed");
        return false;
      }
      // If the existing version_dir doesn't exist, then we'll return true
      // so that we attempt to repair the broken installation.
    } else {
      scoped_ptr<Version> cur_version(
          Version::GetVersionFromString(version_str));
      scoped_ptr<Version> new_version(
          Version::GetVersionFromString(version));
      if (cur_version->CompareTo(*new_version) >= 0) {
        ReportExtensionInstallError(frontend, extension_path,
            "More recent version of extension already installed");
        return false;
      }
    }
  }
  return true;
}

bool ExtensionsServiceBackend::UnzipExtension(const FilePath& extension_path,
    const FilePath& temp_dir,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  // <profile>/Extensions/INSTALL_TEMP/<version>
  if (!file_util::CreateDirectory(temp_dir)) {
    ReportExtensionInstallError(frontend, extension_path,
        "Couldn't create version directory.");
    return false;
  }
  if (!Unzip(extension_path, temp_dir, NULL)) {
    // Remove what we just installed.
    file_util::Delete(temp_dir, true);
    ReportExtensionInstallError(frontend, extension_path,
        "Couldn't unzip extension.");
    return false;
  }
  return true;
}

bool ExtensionsServiceBackend::InstallDirSafely(
    const FilePath& extension_path,
    const FilePath& source_dir, 
    const FilePath& dest_dir,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {

  if (file_util::PathExists(dest_dir)) {
    // By the time we get here, it should be safe to assume that this directory
    // is not currently in use (it's not the current active version).
    if (!file_util::Delete(dest_dir, true)) {
      ReportExtensionInstallError(frontend, extension_path,
          "Can't delete existing version directory.");
      return false;
    }
  } else {
    FilePath parent = dest_dir.DirName();
    if (!file_util::DirectoryExists(parent)) {
      if (!file_util::CreateDirectory(parent)) {
        ReportExtensionInstallError(frontend, extension_path,
            "Couldn't create extension directory.");
        return false;
      }
    }
  }
  if (!file_util::Move(source_dir, dest_dir)) {
    ReportExtensionInstallError(frontend, extension_path,
        "Couldn't move temporary directory.");
    return false;
  }

  return true;
}

bool ExtensionsServiceBackend::SetCurrentVersion(
    const FilePath& extension_path,
    const FilePath& dest_dir,
    std::string version,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  // Write out the new CurrentVersion file.
  // <profile>/Extension/<name>/CurrentVersion
  FilePath current_version =
      dest_dir.AppendASCII(ExtensionsService::kCurrentVersionFileName);
  FilePath current_version_old =
    current_version.InsertBeforeExtension(FILE_PATH_LITERAL("_old"));
  if (file_util::PathExists(current_version_old)) {
    if (!file_util::Delete(current_version_old, false)) {
      ReportExtensionInstallError(frontend, extension_path,
          "Couldn't remove CurrentVersion_old file.");
      return false;
    }
  }
  if (file_util::PathExists(current_version)) {
    if (!file_util::Move(current_version, current_version_old)) {
      ReportExtensionInstallError(frontend, extension_path,
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
    ReportExtensionInstallError(frontend, extension_path, 
        "Couldn't create CurrentVersion file.");
    return false;
  }
  return true;
}

bool ExtensionsServiceBackend::InstallExtension(
    const FilePath& extension_path,
    const FilePath& install_dir,
    scoped_refptr<ExtensionsServiceFrontendInterface> frontend) {
  LOG(INFO) << "Installing extension " << extension_path.value();

  // <profile>/Extensions/INSTALL_TEMP
  FilePath temp_dir = install_dir.AppendASCII(kTempExtensionName);
  // Ensure we're starting with a clean slate.
  if (file_util::PathExists(temp_dir)) {
    if (!file_util::Delete(temp_dir, true)) {
      ReportExtensionInstallError(frontend, extension_path,
          "Couldn't delete existing temporary directory.");
      return false;
    }
  }
  ScopedTempDir scoped_temp;
  scoped_temp.Set(temp_dir);
  if (!scoped_temp.IsValid()) {
    ReportExtensionInstallError(frontend, extension_path,
        "Couldn't create temporary directory.");
    return false;
  }

  // Read and verify the extension.
  scoped_ptr<DictionaryValue> manifest(ReadManifest(extension_path, frontend));
  if (!manifest.get()) {
    // ReadManifest has already reported the extension error.
    return false;
  }
  DictionaryValue* dict = manifest.get();
  Extension extension;
  std::string error;
  if (!extension.InitFromValue(*dict, &error)) {
    ReportExtensionInstallError(frontend, extension_path,
        "Invalid extension manifest.");
    return false;
  }

  // <profile>/Extensions/<id>
  FilePath dest_dir = install_dir.AppendASCII(extension.id());
  std::string version = extension.VersionString();
  if (!CheckCurrentVersion(extension_path, version, dest_dir, frontend))
    return false;

  // <profile>/Extensions/INSTALL_TEMP/<version>
  FilePath temp_version = temp_dir.AppendASCII(version);
  if (!UnzipExtension(extension_path, temp_version, frontend))
    return false;

  // <profile>/Extensions/<dir_name>/<version>
  FilePath version_dir = dest_dir.AppendASCII(version);
  if (!InstallDirSafely(extension_path, temp_version, version_dir, frontend))
    return false;

  if (!SetCurrentVersion(extension_path, dest_dir, version, frontend)) {
    if (!file_util::Delete(version_dir, true))
      LOG(WARNING) << "Can't remove " << dest_dir.value();
    return false;
  }

  ReportExtensionInstalled(frontend, dest_dir);
  return true;
}

void ExtensionsServiceBackend::ReportExtensionInstallError(
    ExtensionsServiceFrontendInterface *frontend, const FilePath& path,
    const std::string &error) {
  // TODO(erikkay): note that this isn't guaranteed to work properly on Linux.
  std::string path_str = WideToASCII(path.ToWStringHack());
  std::string message =
      StringPrintf("Could not install extension from '%s'. %s",
                   path_str.c_str(), error.c_str());
  frontend->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
      frontend, &ExtensionsServiceFrontendInterface::OnExtensionInstallError,
      message));
}

void ExtensionsServiceBackend::ReportExtensionInstalled(
    ExtensionsServiceFrontendInterface *frontend, FilePath path) {
  frontend->GetMessageLoop()->PostTask(FROM_HERE, NewRunnableMethod(
      frontend,
      &ExtensionsServiceFrontendInterface::OnExtensionInstalled,
      path));
}
