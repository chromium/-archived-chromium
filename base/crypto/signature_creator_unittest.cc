// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/crypto/signature_creator.h"
#include "base/crypto/signature_verifier.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SignatureCreatorTest, BasicTest) {
  // Do a verify round trip.
  scoped_ptr<base::RSAPrivateKey> key_original(
      base::RSAPrivateKey::Create(1024));
  ASSERT_TRUE(key_original.get());

  std::vector<uint8> key_info;
  key_original->ExportPrivateKey(&key_info);
  scoped_ptr<base::RSAPrivateKey> key(
      base::RSAPrivateKey::CreateFromPrivateKeyInfo(key_info));

  scoped_ptr<base::SignatureCreator> signer(
      base::SignatureCreator::Create(key.get()));
  ASSERT_TRUE(signer.get());

  std::string data("Hello, World!");
  ASSERT_TRUE(signer->Update(reinterpret_cast<const uint8*>(data.c_str()),
                             data.size()));

  std::vector<uint8> signature;
  ASSERT_TRUE(signer->Final(&signature));

  std::vector<uint8> public_key_info;
  ASSERT_TRUE(key_original->ExportPublicKey(&public_key_info));

  // This is the algorithm ID for SHA-1 with RSA encryption.
  // TODO(aa): Factor this out into some shared location.
  const uint8 kSHA1WithRSAAlgorithmID[] = {
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00
  };
  base::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(
      kSHA1WithRSAAlgorithmID, sizeof(kSHA1WithRSAAlgorithmID),
      &signature.front(), signature.size(),
      &public_key_info.front(), public_key_info.size()));

  verifier.VerifyUpdate(reinterpret_cast<const uint8*>(data.c_str()),
                        data.size());
  ASSERT_TRUE(verifier.VerifyFinal());
}
