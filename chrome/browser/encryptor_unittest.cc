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
#include <string>

#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(EncryptorTest, WideEncryptionDecryption) {
  std::wstring plaintext;
  std::wstring result;
  std::string utf8_plaintext;
  std::string utf8_result;
  std::string ciphertext;

  // test borderline cases (empty strings)
  EXPECT_TRUE(Encryptor::EncryptWideString(plaintext, &ciphertext));
  EXPECT_TRUE(Encryptor::DecryptWideString(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // test a simple string
  plaintext = L"hello";
  EXPECT_TRUE(Encryptor::EncryptWideString(plaintext, &ciphertext));
  EXPECT_TRUE(Encryptor::DecryptWideString(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // test unicode
  std::wstring::value_type wchars[] = { 0xdbeb,0xdf1b,0x4e03,0x6708,0x8849,
                                        0x661f,0x671f,0x56db,0x597c,0x4e03,
                                        0x6708,0x56db,0x6708,0xe407,0xdbaf,
                                        0xdeb5,0x4ec5,0x544b,0x661f,0x671f,
                                        0x65e5,0x661f,0x671f,0x4e94,0xd8b1,
                                        0xdce1,0x7052,0x5095,0x7c0b,0xe586, 0};
  plaintext = wchars;
  utf8_plaintext = WideToUTF8(plaintext);
  EXPECT_EQ(plaintext, UTF8ToWide(utf8_plaintext));
  EXPECT_TRUE(Encryptor::EncryptWideString(plaintext, &ciphertext));
  EXPECT_TRUE(Encryptor::DecryptWideString(ciphertext, &result));
  EXPECT_EQ(plaintext, result);
  EXPECT_TRUE(Encryptor::DecryptString(ciphertext, &utf8_result));
  EXPECT_EQ(utf8_plaintext, WideToUTF8(result));

  EXPECT_TRUE(Encryptor::EncryptString(utf8_plaintext, &ciphertext));
  EXPECT_TRUE(Encryptor::DecryptWideString(ciphertext, &result));
  EXPECT_EQ(plaintext, result);
  EXPECT_TRUE(Encryptor::DecryptString(ciphertext, &utf8_result));
  EXPECT_EQ(utf8_plaintext, WideToUTF8(result));
}

TEST(EncryptorTest, EncryptionDecryption) {
  std::string plaintext;
  std::string result;
  std::string ciphertext;

  // test borderline cases (empty strings)
  EXPECT_TRUE(Encryptor::EncryptString(plaintext, &ciphertext));
  EXPECT_TRUE(Encryptor::DecryptString(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // test a simple string
  plaintext = "hello";
  EXPECT_TRUE(Encryptor::EncryptString(plaintext, &ciphertext));
  EXPECT_TRUE(Encryptor::DecryptString(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // Make sure it null terminates.
  plaintext.assign("hello", 3);
  EXPECT_TRUE(Encryptor::EncryptString(plaintext, &ciphertext));
  EXPECT_TRUE(Encryptor::DecryptString(ciphertext, &result));
  EXPECT_EQ(plaintext, "hel");
}

}  // namespace
