// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
  Time date_created;
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
