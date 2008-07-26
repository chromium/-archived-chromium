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
// A class for encrypting/decrypting strings

#ifndef CHROME_BROWSER_ENCRYPTOR_H__
#define CHROME_BROWSER_ENCRYPTOR_H__

#include <string>

#include "base/values.h"

class Encryptor {
public:
  // Encrypt a wstring. The output (second argument) is
  // really an array of bytes, but we're passing it back
  // as a std::string
  static bool EncryptWideString(const std::wstring& plaintext,
                                std::string* ciphertext);

  // Decrypt an array of bytes obtained with EnctryptWideString
  // back into a wstring. Note that the input (first argument)
  // is a std::string, so you need to first get your (binary)
  // data into a string.
  static bool DecryptWideString(const std::string& ciphertext,
                                std::wstring* plaintext);

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

#endif CHROME_BROWSER_ENCRYPTOR_H__
