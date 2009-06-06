// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/signature_creator.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"

namespace base {

// static
SignatureCreator* SignatureCreator::Create(RSAPrivateKey* key) {
  scoped_ptr<SignatureCreator> result(new SignatureCreator);
  result->key_ = key;

  if (!CryptCreateHash(key->provider(), CALG_SHA1, 0, 0,
                       &result->hash_object_)) {
    NOTREACHED();
    return NULL;
  }

  return result.release();
}

SignatureCreator::~SignatureCreator() {
  if (hash_object_) {
    if (!CryptDestroyHash(hash_object_))
      NOTREACHED();

    hash_object_ = 0;
  }
}

bool SignatureCreator::Update(const uint8* data_part, int data_part_len) {
  if (!CryptHashData(hash_object_, data_part, data_part_len, 0)) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool SignatureCreator::Final(std::vector<uint8>* signature) {
  DWORD signature_length = 0;
  if (!CryptSignHash(hash_object_, AT_SIGNATURE, NULL, 0, NULL,
                     &signature_length)) {
    return false;
  }

  std::vector<uint8> temp;
  temp.resize(signature_length);
  if (!CryptSignHash(hash_object_, AT_SIGNATURE, NULL, 0, &temp.front(),
                     &signature_length)) {
    return false;
  }

  temp.resize(signature_length);
  for (size_t i = temp.size(); i > 0; --i)
    signature->push_back(temp[i - 1]);

  hash_object_ = 0;
  return true;
}

}  // namespace base
