// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "net/http/des.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// This test vector comes from the NSS FIPS power-up self-test.
TEST(DESTest, KnownAnswerTest1) {
  // DES known key (56-bits).
  static const uint8 des_known_key[] = { "ANSI DES" };

  // DES known plaintext (64-bits).
  static const uint8 des_ecb_known_plaintext[] = { "Netscape" };

  // DES known ciphertext (64-bits).
  static const uint8 des_ecb_known_ciphertext[] = {
    0x26, 0x14, 0xe9, 0xc3, 0x28, 0x80, 0x50, 0xb0
  };

  uint8 ciphertext[8];
  memset(ciphertext, 0xaf, sizeof(ciphertext));

  DESEncrypt(des_known_key, des_ecb_known_plaintext, ciphertext);
  EXPECT_EQ(0, memcmp(ciphertext, des_ecb_known_ciphertext, 8));
}

// This test vector comes from NIST Special Publication 800-17, Modes of
// Operation Validation System (MOVS): Requirements and Procedures, Appendix
// A, page 124.
TEST(DESTest, KnownAnswerTest2) {
  static const uint8 key[] = {
    0x10, 0x31, 0x6e, 0x02, 0x8c, 0x8f, 0x3b, 0x4a
  };
  static const uint8 plaintext[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  static const uint8 known_ciphertext[] = {
    0x82, 0xdc, 0xba, 0xfb, 0xde, 0xab, 0x66, 0x02
  };
  uint8 ciphertext[8];
  memset(ciphertext, 0xaf, sizeof(ciphertext));

  DESEncrypt(key, plaintext, ciphertext);
  EXPECT_EQ(0, memcmp(ciphertext, known_ciphertext, 8));
}

}  // namespace net
