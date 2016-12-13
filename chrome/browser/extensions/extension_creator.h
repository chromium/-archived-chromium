// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_CREATOR_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_CREATOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace base {
class RSAPrivateKey;
}

class FilePath;

// This class create an installable extension (.crx file) given an input
// directory that contains a valid manifest.json and the extension's resources
// contained within that directory. The output .crx file is always signed with a
// private key that is either provided in |private_key_path| or is internal
// generated randomly (and optionally written to |output_private_key_path|.
class ExtensionCreator {
 public:
  ExtensionCreator() {}

  bool Run(const FilePath& extension_dir,
           const FilePath& crx_path,
           const FilePath& private_key_path,
           const FilePath& private_key_output_path);

  // Returns the error message that will be present if Run(...) returned false.
  std::string error_message() { return error_message_; }

 private:
  // Verifies input directory's existance. |extension_dir| is the source
  // directory that should contain all the extension resources.
  // |private_key_path| is the optional path to an existing private key to sign
  // the extension. If not provided, a random key will be created (in which case
  // it is written to |private_key_output_path| -- if provided).
  bool InitializeInput(const FilePath& extension_dir,
                       const FilePath& private_key_path,
                       const FilePath& private_key_output_path);

  // Reads private key from |private_key_path|.
  base::RSAPrivateKey* ReadInputKey(const FilePath& private_key_path);

  // Generates a key pair and writes the private key to |private_key_path|
  // if provided.
  base::RSAPrivateKey* GenerateKey(const FilePath& private_key_path);

  // Creates temporary zip file for the extension.
  bool CreateZip(const FilePath& extension_dir, FilePath* zip_path);

  // Signs the temporary zip and returns the signature.
  bool SignZip(const FilePath& zip_path,
               base::RSAPrivateKey* private_key,
               std::vector<uint8>* signature);

  // Export installable .crx to |crx_path|.
  bool WriteCRX(const FilePath& zip_path,
                base::RSAPrivateKey* private_key,
                const std::vector<uint8>& signature,
                const FilePath& crx_path);

  // Holds a message for any error that is raised during Run(...).
  std::string error_message_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionCreator);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_CREATOR_H_
