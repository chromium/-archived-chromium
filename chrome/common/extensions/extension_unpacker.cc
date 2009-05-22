// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_unpacker.h"

#include "base/file_util.h"
#include "base/scoped_handle.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/third_party/nss/blapi.h"
#include "base/third_party/nss/sha256.h"
#include "base/thread.h"
#include "base/values.h"
#include "net/base/file_stream.h"
// TODO(mpcomplete): move to common
#include "chrome/browser/extensions/extension.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/unzip.h"
#include "chrome/common/url_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/image_decoder.h"

namespace {
const char kCurrentVersionFileName[] = "Current Version";

// The name of a temporary directory to install an extension into for
// validation before finalizing install.
const char kTempExtensionName[] = "TEMP_INSTALL";

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
}  // namespace

static SkBitmap DecodeImage(const FilePath& path) {
  // Read the file from disk.
  std::string file_contents;
  if (!file_util::PathExists(path) ||
      !file_util::ReadFileToString(path, &file_contents)) {
    return SkBitmap();
  }

  // Decode the image using WebKit's image decoder.
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(file_contents.data());
  webkit_glue::ImageDecoder decoder;
  return decoder.Decode(data, file_contents.length());
}

static bool PathContainsParentDirectory(const FilePath& path) {
  const FilePath::StringType kSeparators(FilePath::kSeparators);
  const FilePath::StringType kParentDirectory(FilePath::kParentDirectory);
  const size_t npos = FilePath::StringType::npos;
  const FilePath::StringType& value = path.value();

  for (size_t i = 0; i < value.length(); ) {
    i = value.find(kParentDirectory, i);
    if (i != npos) {
      if ((i == 0 || kSeparators.find(value[i-1]) == npos) &&
          (i+1 < value.length() || kSeparators.find(value[i+1]) == npos)) {
        return true;
      }
      ++i;
    }
  }

  return false;
}

// The extension file format is a header, followed by the manifest, followed
// by the zip file.  The header is a magic number, a version, the size of the
// header, and the size of the manifest.  These ints are 4 byte little endian.
DictionaryValue* ExtensionUnpacker::ReadPackageHeader() {
  ScopedStdioHandle file(file_util::OpenFile(extension_path_, "rb"));
  if (!file.get()) {
    SetError("no such extension file");
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
    SetError("invalid extension header");
    return NULL;
  }
  if (strncmp(kExtensionFileMagic, header.magic, sizeof(header.magic))) {
    SetError("bad magic number");
    return NULL;
  }
  if (header.version != kExpectedVersion) {
    SetError("bad version number");
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
    SetError(error);
    return NULL;
  }
  if (!val->IsType(Value::TYPE_DICTIONARY)) {
    SetError("manifest isn't a JSON dictionary");
    return NULL;
  }
  DictionaryValue* manifest = static_cast<DictionaryValue*>(val.get());

  std::string zip_hash;
  if (!manifest->GetString(Extension::kZipHashKey, &zip_hash)) {
    SetError("missing zip_hash key");
    return NULL;
  }
  if (zip_hash.size() != kZipHashHexBytes) {
    SetError("invalid zip_hash key");
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
    SetError("invalid zip_hash key");
    return NULL;
  }
  if (zip_hash_bytes.size() != kZipHashBytes) {
    SetError("invalid zip_hash key");
    return NULL;
  }
  for (size_t i = 0; i < kZipHashBytes; ++i) {
    if (zip_hash_bytes[i] != hash[i]) {
      SetError("zip_hash key didn't match zip hash");
      return NULL;
    }
  }

  // TODO(erikkay): The manifest will also contain a signature of the hash
  // (or perhaps the whole manifest) for authentication purposes.

  // The caller owns val (now cast to manifest).
  val.release();
  return manifest;
}

DictionaryValue* ExtensionUnpacker::ReadManifest() {
  FilePath manifest_path =
      temp_install_dir_.AppendASCII(Extension::kManifestFilename);
  if (!file_util::PathExists(manifest_path)) {
    SetError(Extension::kInvalidManifestError);
    return NULL;
  }

  JSONFileValueSerializer serializer(manifest_path);
  std::string error;
  scoped_ptr<Value> root(serializer.Deserialize(&error));
  if (!root.get()) {
    SetError(error);
    return NULL;
  }

  if (!root->IsType(Value::TYPE_DICTIONARY)) {
    SetError(Extension::kInvalidManifestError);
    return NULL;
  }

  return static_cast<DictionaryValue*>(root.release());
}

bool ExtensionUnpacker::Run() {
  LOG(INFO) << "Installing extension " << extension_path_.value();

  // Read and verify the extension.
  scoped_ptr<DictionaryValue> header_manifest(ReadPackageHeader());
  if (!header_manifest.get()) {
    // ReadPackageHeader has already reported the extension error.
    return false;
  }

  // TODO(mpcomplete): it looks like this isn't actually necessary.  We don't
  // use header_extension, and we check that the unzipped manifest is valid.
  Extension header_extension;
  std::string error;
  if (!header_extension.InitFromValue(*header_manifest,
                                      true,  // require ID
                                      &error)) {
    SetError(error);
    return false;
  }

  // <profile>/Extensions/INSTALL_TEMP/<version>
  temp_install_dir_ =
      extension_path_.DirName().AppendASCII(kTempExtensionName);
  if (!file_util::CreateDirectory(temp_install_dir_)) {
    SetError("Couldn't create directory for unzipping.");
    return false;
  }

  if (!Unzip(extension_path_, temp_install_dir_, NULL)) {
    SetError("Couldn't unzip extension.");
    return false;
  }

  // Parse the manifest.
  parsed_manifest_.reset(ReadManifest());
  if (!parsed_manifest_.get())
    return false;  // Error was already reported.

  // Re-read the actual manifest into our extension struct.
  Extension extension;
  if (!extension.InitFromValue(*parsed_manifest_,
                               true,  // require ID
                               &error)) {
    SetError(error);
    return false;
  }

  // Decode any images that the browser needs to display.
  DictionaryValue* images = extension.GetThemeImages();
  if (images) {
    for (DictionaryValue::key_iterator it = images->begin_keys();
         it != images->end_keys(); ++it) {
      std::wstring val;
      if (images->GetString(*it, &val)) {
        if (!AddDecodedImage(FilePath::FromWStringHack(val)))
          return false;  // Error was already reported.
      }
    }
  }

  for (PageActionMap::const_iterator it = extension.page_actions().begin();
       it != extension.page_actions().end(); ++it) {
    if (!AddDecodedImage(it->second->icon_path()))
      return false;  // Error was already reported.
  }

  return true;
}

bool ExtensionUnpacker::AddDecodedImage(const FilePath& path) {
  // Make sure it's not referencing a file outside the extension's subdir.
  if (path.IsAbsolute() || PathContainsParentDirectory(path)) {
    SetError("Path names must not be absolute or contain '..'.");
    return false;
  }

  SkBitmap image_bitmap = DecodeImage(temp_install_dir_.Append(path));
  if (image_bitmap.isNull()) {
    SetError("Could not decode theme image.");
    return false;
  }

  decoded_images_.push_back(MakeTuple(image_bitmap, path));
  return true;
}

void ExtensionUnpacker::SetError(const std::string &error) {
  error_message_ = error;
}
