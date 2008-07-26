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

#include "chrome/browser/encryptor.h"

#include <windows.h>
#include <wincrypt.h>
#include "base/string_util.h"

#pragma comment(lib, "crypt32.lib")

bool Encryptor::EncryptWideString(const std::wstring& plaintext,
                                  std::string* ciphertext) {
  return EncryptString(WideToUTF8(plaintext), ciphertext);
}

bool Encryptor::DecryptWideString(const std::string& ciphertext,
                                  std::wstring* plaintext){
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8))
    return false;

  *plaintext = UTF8ToWide(utf8);
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
                              std::string* plaintext){
  DATA_BLOB input;
  input.pbData = const_cast<BYTE*>(
    reinterpret_cast<const BYTE*>(ciphertext.data()));
  input.cbData = static_cast<DWORD>(ciphertext.length());

  DATA_BLOB output;
  BOOL result = CryptUnprotectData(&input, NULL, NULL, NULL, NULL,
                                   0, &output);
  if(!result)
    return false;

  plaintext->assign(reinterpret_cast<char*>(output.pbData), output.cbData);
  LocalFree(output.pbData);
  return true;
}