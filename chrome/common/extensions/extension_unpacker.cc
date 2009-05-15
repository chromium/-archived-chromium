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
}

// The extension file format is a header, followed by the manifest, followed
// by the zip file.  The header is a magic number, a version, the size of the
// header, and the size of the manifest.  These ints are 4 byte little endian.
DictionaryValue* ExtensionUnpacker::ReadManifest() {
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

bool ExtensionUnpacker::Run() {
  LOG(INFO) << "Installing extension " << extension_path_.value();

  // Read and verify the extension.
  scoped_ptr<DictionaryValue> manifest(ReadManifest());
  if (!manifest.get()) {
    // ReadManifest has already reported the extension error.
    return false;
  }
  Extension extension;
  std::string error;
  if (!extension.InitFromValue(*manifest,
                               true,  // require ID
                               &error)) {
    SetError("Invalid extension manifest.");
    return false;
  }

  // ID is required for installed extensions.
  if (extension.id().empty()) {
    SetError("Required value 'id' is missing.");
    return false;
  }

  // <profile>/Extensions/INSTALL_TEMP/<version>
  std::string version = extension.VersionString();
  FilePath temp_install =
      extension_path_.DirName().AppendASCII(kTempExtensionName);
  if (!file_util::CreateDirectory(temp_install)) {
    SetError("Couldn't create directory for unzipping.");
    return false;
  }

  if (!Unzip(extension_path_, temp_install, NULL)) {
    SetError("Couldn't unzip extension.");
    return false;
  }

  return true;
}

void ExtensionUnpacker::SetError(const std::string &error) {
  error_message_ = error;
}
