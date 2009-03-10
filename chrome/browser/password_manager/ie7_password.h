// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IE7_PASSWORD_H__
#define CHROME_BROWSER_IE7_PASSWORD_H__

#include <windows.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time.h"

// Contains the information read from the IE7/IE8 Storage2 key in the registry.
struct IE7PasswordInfo {
  // Hash of the url.
  std::wstring url_hash;

  // Encrypted data containing the username, password and some more undocumented
  // fields.
  std::vector<unsigned char> encrypted_data;

  // When the login was imported.
  base::Time date_created;
};

namespace ie7_password {

// Parses a data structure to find the password and the username.
bool GetUserPassFromData(const std::vector<unsigned char>& data,
                         std::wstring* username,
                         std::wstring* password);

// Decrypts the username and password for a given data vector using the url as
// the key.
bool DecryptPassword(const std::wstring& url,
                     const std::vector<unsigned char>& data,
                     std::wstring* username, std::wstring* password);

// Returns the hash of a url.
std::wstring GetUrlHash(const std::wstring& url);

}  // namespace ie7_password

#endif  // CHROME_BROWSER_IE7_PASSWORD_H__
