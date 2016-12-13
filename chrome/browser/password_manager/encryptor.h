// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// A class for encrypting/decrypting strings

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ENCRYPTOR_H__
#define CHROME_BROWSER_PASSWORD_MANAGER_ENCRYPTOR_H__

#include <string>

#include "base/values.h"
#include "base/string16.h"

class Encryptor {
 public:
  // Encrypt a string16. The output (second argument) is
  // really an array of bytes, but we're passing it back
  // as a std::string
  static bool EncryptString16(const string16& plaintext,
                              std::string* ciphertext);

  // Decrypt an array of bytes obtained with EncryptString16
  // back into a string16. Note that the input (first argument)
  // is a std::string, so you need to first get your (binary)
  // data into a string.
  static bool DecryptString16(const std::string& ciphertext,
                              string16* plaintext);

  // Encrypt a string.
  static bool EncryptString(const std::string& plaintext,
                            std::string* ciphertext);

  // Decrypt an array of bytes obtained with EnctryptString
  // back into a string. Note that the input (first argument)
  // is a std::string, so you need to first get your (binary)
  // data into a string.
  static bool DecryptString(const std::string& ciphertext,
                            std::string* plaintext);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Encryptor);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ENCRYPTOR_H__
