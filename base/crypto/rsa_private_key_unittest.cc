// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/crypto/rsa_private_key.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(RSAPrivateKeyUnitTest, InitRandomTest) {
  // Generate random private keys with two different sizes. Reimport, then
  // export them again. We should get back the same exact bytes.
  scoped_ptr<base::RSAPrivateKey> keypair1(base::RSAPrivateKey::Create(1024));
  scoped_ptr<base::RSAPrivateKey> keypair2(base::RSAPrivateKey::Create(2048));
  ASSERT_TRUE(keypair1.get());
  ASSERT_TRUE(keypair2.get());

  std::vector<uint8> privkey1;
  std::vector<uint8> privkey2;
  std::vector<uint8> pubkey1;
  std::vector<uint8> pubkey2;

  ASSERT_TRUE(keypair1->ExportPrivateKey(&privkey1));
  ASSERT_TRUE(keypair2->ExportPrivateKey(&privkey2));
  ASSERT_TRUE(keypair1->ExportPublicKey(&pubkey1));
  ASSERT_TRUE(keypair2->ExportPublicKey(&pubkey2));

  scoped_ptr<base::RSAPrivateKey> keypair3(
      base::RSAPrivateKey::CreateFromPrivateKeyInfo(privkey1));
  scoped_ptr<base::RSAPrivateKey> keypair4(
      base::RSAPrivateKey::CreateFromPrivateKeyInfo(privkey2));
  ASSERT_TRUE(keypair3.get());
  ASSERT_TRUE(keypair4.get());

  std::vector<uint8> privkey3;
  std::vector<uint8> privkey4;
  ASSERT_TRUE(keypair3->ExportPrivateKey(&privkey3));
  ASSERT_TRUE(keypair4->ExportPrivateKey(&privkey4));

  ASSERT_EQ(privkey1.size(), privkey3.size());
  ASSERT_EQ(privkey2.size(), privkey4.size());
  ASSERT_TRUE(0 == memcmp(&privkey1.front(), &privkey3.front(),
                          privkey1.size()));
  ASSERT_TRUE(0 == memcmp(&privkey2.front(), &privkey4.front(),
                          privkey2.size()));
}

// TODO(aa): Consider importing some private keys from other software.
