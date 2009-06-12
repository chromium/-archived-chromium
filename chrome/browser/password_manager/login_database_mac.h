// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_LOGIN_DATABASE_MAC_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_LOGIN_DATABASE_MAC_H_

#include "chrome/browser/password_manager/login_database.h"

#include <string>

// A trivial subclass of LoginDatabase that nulls out passwords, so that we can
// use the rest of the database as a suplemental storage system to complement
// Keychain, providing storage of fields Keychain doesn't allow for.
class LoginDatabaseMac : public LoginDatabase {
 public:
  LoginDatabaseMac() {}
  virtual ~LoginDatabaseMac() {}

 protected:
  // Stub implementations that always return an empty string.
  virtual std::string EncryptedString(const std::wstring& plain_text) const;
  virtual std::wstring DecryptedString(const std::string& cipher_text) const;
};


#endif  // CHROME_BROWSER_PASSWORD_MANAGER_LOGIN_DATABASE_MAC_H_
