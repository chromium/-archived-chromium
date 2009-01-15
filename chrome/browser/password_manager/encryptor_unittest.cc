// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/encryptor.h"

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

