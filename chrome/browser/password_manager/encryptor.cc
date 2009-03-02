// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/encryptor.h"

#include <windows.h>
#include <wincrypt.h>
#include "base/string_util.h"

#pragma comment(lib, "crypt32.lib")

bool Encryptor::EncryptString16(const string16& plaintext,
                                std::string* ciphertext) {
  return EncryptString(UTF16ToUTF8(plaintext), ciphertext);
}

bool Encryptor::DecryptString16(const std::string& ciphertext,
                                string16* plaintext) {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8))
    return false;

  *plaintext = UTF8ToUTF16(utf8);
  return true;
}

bool Encryptor::EncryptString(const std::string& plaintext,
                              std::string* ciphertext) {
  DATA_BLOB input;
  input.pbData = const_cast<BYTE*>(
    reinterpret_cast<const BYTE*>(plaintext.data()));
  input.cbData = static_cast<DWORD>(plaintext.length());

  DATA_BLOB output;
  BOOL result = CryptProtectData(&input, L"", NULL, NULL, NULL,
                                 0, &output);
  if (!result)
    return false;

  // this does a copy
  ciphertext->assign(reinterpret_cast<std::string::value_type*>(output.pbData),
                     output.cbData);

  LocalFree(output.pbData);
  return true;
}

bool Encryptor::DecryptString(const std::string& ciphertext,
                              std::string* plaintext) {
  DATA_BLOB input;
  input.pbData = const_cast<BYTE*>(
    reinterpret_cast<const BYTE*>(ciphertext.data()));
  input.cbData = static_cast<DWORD>(ciphertext.length());

  DATA_BLOB output;
  BOOL result = CryptUnprotectData(&input, NULL, NULL, NULL, NULL,
                                   0, &output);
  if (!result)
    return false;

  plaintext->assign(reinterpret_cast<char*>(output.pbData), output.cbData);
  LocalFree(output.pbData);
  return true;
}
