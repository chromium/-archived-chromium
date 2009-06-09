// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_creator.h"

#include <vector>
#include <string>

#include "base/crypto/rsa_private_key.h"
#include "base/crypto/signature_creator.h"
#include "base/file_util.h"
#include "base/scoped_handle.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/zip.h"
#include "net/base/base64.h"

namespace {
  const int kRSAKeySize = 1024;
};

DictionaryValue* ExtensionCreator::InitializeInput(
    const FilePath& extension_dir,
    const FilePath& private_key_path,
    const FilePath& private_key_output_path) {
  // Validate input |extension_dir|.
  if (extension_dir.value().empty() ||
      !file_util::DirectoryExists(extension_dir)) {
    error_message_ = "Input directory must exist.";
    return false;
  }

  // Validate input |private_key| (if provided).
  if (!private_key_path.value().empty() &&
      !file_util::PathExists(private_key_path)) {
    error_message_ = "Input value for private key must be a valid path.";
    return false;
  }

  // If an |output_private_key| path is given, make sure it doesn't over-write
  // an existing private key.
  if (private_key_path.value().empty() &&
      !private_key_output_path.value().empty() &&
      file_util::PathExists(private_key_output_path)) {
      error_message_ = "Private key exists next to input directory. Try using "
          "--pack-extension-key";
      return false;
  }

  // Read the manifest.
  FilePath manifest_path = extension_dir.AppendASCII(
      Extension::kManifestFilename);
  if (!file_util::PathExists(manifest_path)) {
    error_message_ = "Extension must contain '";
    error_message_.append(Extension::kManifestFilename);
    error_message_.append("'.");
    return false;
  }

  JSONFileValueSerializer serializer(manifest_path);
  std::string serialization_error;
  Value* input_manifest = (serializer.Deserialize(&serialization_error));
  if (!input_manifest) {
    error_message_ = "Invalid manifest.json: ";
    error_message_.append(serialization_error);
    return false;
  }

  if (!input_manifest->IsType(Value::TYPE_DICTIONARY)) {
    error_message_ = "Invalid manifest.json";
    return false;
  }

  return static_cast<DictionaryValue*>(input_manifest);
}

base::RSAPrivateKey* ExtensionCreator::ReadInputKey(const FilePath&
    private_key_path) {
  if (!file_util::PathExists(private_key_path)) {
    error_message_ = "Input value for private key must exist.";
    return false;
  }

  std::string private_key_contents;
  if (!file_util::ReadFileToString(private_key_path,
      &private_key_contents)) {
    error_message_ = "Failed to read private key.";
    return false;
  }

  std::string private_key_bytes;
  if (!Extension::ParsePEMKeyBytes(private_key_contents,
       &private_key_bytes)) {
    error_message_ = "Invalid private key.";
    return false;
  }

  return base::RSAPrivateKey::CreateFromPrivateKeyInfo(
      std::vector<uint8>(private_key_bytes.begin(), private_key_bytes.end()));
}

base::RSAPrivateKey* ExtensionCreator::GenerateKey(const FilePath&
    output_private_key_path) {
  scoped_ptr<base::RSAPrivateKey> key_pair(
      base::RSAPrivateKey::Create(kRSAKeySize));
  if (!key_pair.get()) {
    error_message_ = "Yikes! Failed to generate random RSA private key.";
    return NULL;
  }

  std::vector<uint8> private_key_vector;
  if (!key_pair->ExportPrivateKey(&private_key_vector)) {
    error_message_ = "Failed to export private key.";
    return NULL;
  }
  std::string private_key_bytes(private_key_vector.begin(),
      private_key_vector.end());

  std::string private_key;
  if (!Extension::ProducePEM(private_key_bytes, &private_key)) {
    error_message_ = "Failed to output private key.";
    return NULL;
  }
  std::string pem_output;
  if (!Extension::FormatPEMForFileOutput(private_key, &pem_output,
       false)) {
    error_message_ = "Failed to output private key.";
    return NULL;
  }

  if (!output_private_key_path.empty()) {
    if (-1 == file_util::WriteFile(output_private_key_path,
        pem_output.c_str(), pem_output.size())) {
      error_message_ = "Failed to write private key.";
      return NULL;
    }
  }

  return key_pair.release();
}

bool ExtensionCreator::CreateAndSignZip(const FilePath& extension_dir,
                                        base::RSAPrivateKey *key_pair,
                                        FilePath* zip_path,
                                        std::string* signature) {
  file_util::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_"), zip_path);
  *zip_path = zip_path->Append(FILE_PATH_LITERAL("extension.zip"));

  if (!Zip(extension_dir, *zip_path)) {
    error_message_ = "Failed to create temporary zip file during packaging.";
    return false;
  }

  scoped_ptr<base::SignatureCreator> signature_creator(
      base::SignatureCreator::Create(key_pair));
  ScopedStdioHandle zip_handle(file_util::OpenFile(*zip_path, "rb"));
  uint8 buffer[1 << 16];
  int bytes_read = -1;
  while ((bytes_read = fread(buffer, 1, sizeof(buffer),
       zip_handle.get())) > 0) {
    if (!signature_creator->Update(buffer, bytes_read)) {
      error_message_ = "Error while signing extension.";
      return false;
    }
  }
  zip_handle.Close();

  std::vector<uint8> signature_vector;
  signature_creator->Final(&signature_vector);
  std::string signature_bytes(signature_vector.begin(), signature_vector.end());
  bool result = net::Base64Encode(signature_bytes, signature);
  DCHECK(result);
  return true;
}

bool ExtensionCreator::PrepareManifestForExport(base::RSAPrivateKey *key_pair,
                                                const std::string& signature,
                                                DictionaryValue* manifest) {
  std::vector<uint8> public_key_vector;
  if (!key_pair->ExportPublicKey(&public_key_vector)) {
    error_message_ = "Failed to export public key.";
    return false;
  }

  std::string public_key_bytes(public_key_vector.begin(),
      public_key_vector.end());
  std::string public_key;
  if (!net::Base64Encode(public_key_bytes, &public_key)) {
    error_message_ = "Error while signing extension.";
    return false;
  }

  manifest->SetString(Extension::kSignatureKey, signature);
  manifest->SetString(Extension::kPublicKeyKey, public_key);

  return true;
}

bool ExtensionCreator::WriteCRX(const FilePath& crx_path,
                                DictionaryValue* manifest,
    const FilePath& zip_path) {
  std::string manifest_string;
  JSONStringValueSerializer serializer(&manifest_string);
  if (!serializer.Serialize(*manifest)) {
    error_message_ = "Failed to write crx.";
    return false;
  }

  if (file_util::PathExists(crx_path))
    file_util::Delete(crx_path, false);
  ScopedStdioHandle crx_handle(file_util::OpenFile(crx_path, "wb"));

  ExtensionsService::ExtensionHeader header;
  memcpy(&header.magic, ExtensionsService::kExtensionFileMagic,
      sizeof(ExtensionsService::kExtensionFileMagic));
  header.version = 1;  // kExpectedVersion
  header.header_size = sizeof(ExtensionsService::ExtensionHeader);
  header.manifest_size = manifest_string.size();

  fwrite(&header, sizeof(ExtensionsService::ExtensionHeader), 1,
      crx_handle.get());
  fwrite(manifest_string.c_str(), sizeof(char), manifest_string.size(),
      crx_handle.get());

  uint8 buffer[1 << 16];
  int bytes_read = -1;
  ScopedStdioHandle zip_handle(file_util::OpenFile(zip_path, "rb"));
  while ((bytes_read = fread(buffer, 1, sizeof(buffer),
      zip_handle.get())) > 0) {
    fwrite(buffer, sizeof(char), bytes_read, crx_handle.get());
  }

  return true;
}

bool ExtensionCreator::Run(const FilePath& extension_dir,
                           const FilePath& crx_path,
                           const FilePath& private_key_path,
                           const FilePath& output_private_key_path) {
  // Check input diretory and read manifest.
  scoped_ptr<DictionaryValue> manifest(InitializeInput(extension_dir,
      private_key_path, output_private_key_path));
  if (!manifest.get())
    return false;

  // Initialize Key Pair
  scoped_ptr<base::RSAPrivateKey> key_pair;
  if (!private_key_path.value().empty())
    key_pair.reset(ReadInputKey(private_key_path));
  else
    key_pair.reset(GenerateKey(output_private_key_path));
  if (!key_pair.get())
    return false;

  // Zip up the extension.
  FilePath zip_path;
  std::string signature;
  if (!CreateAndSignZip(extension_dir, key_pair.get(), &zip_path, &signature))
    return false;

  if (!PrepareManifestForExport(key_pair.get(), signature, manifest.get()))
    return false;

  // Write the final crx out to disk.
  if (!WriteCRX(crx_path, manifest.get(), zip_path))
    return false;

  return true;
}
