// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/encryptor.h"

#include "base/logging.h"
#include "base/string_util.h"

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
  // This doesn't actually encrypt, we need to work on the Encryptor API.
  // http://code.google.com/p/chromium/issues/detail?id=8205
  NOTIMPLEMENTED();

  // this does a copy
  ciphertext->assign(plaintext.data(), plaintext.length());
  return true;
}

bool Encryptor::DecryptString(const std::string& ciphertext,
                              std::string* plaintext) {
  // This doesn't actually decrypt, we need to work on the Encryptor API.
  // http://code.google.com/p/chromium/issues/detail?id=8205
  NOTIMPLEMENTED();

  plaintext->assign(ciphertext.data(), ciphertext.length());
  return true;
}
