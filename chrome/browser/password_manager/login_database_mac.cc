// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/login_database_mac.h"

std::string LoginDatabaseMac::EncryptedString(const std::wstring& plain_text)
    const {
  return std::string();
}

std::wstring LoginDatabaseMac::DecryptedString(const std::string& cipher_text)
    const {
  return std::wstring();
}
