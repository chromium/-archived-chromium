// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hmac.h"
#include "base/logging.h"

HMAC::HMAC(HashAlgorithm hash_alg, const unsigned char* key, int key_length)
    : hash_alg_(hash_alg),
      provider_(NULL),
      hash_(NULL),
      hkey_(NULL) {
  if (!CryptAcquireContext(&provider_, NULL, NULL,
                           PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    provider_ = NULL;
  ImportKey(key, key_length);
}

HMAC::~HMAC() {
  if (hkey_)
    CryptDestroyKey(hkey_);
  if (hash_)
    CryptDestroyHash(hash_);
  if (provider_)
    CryptReleaseContext(provider_, 0);
}

bool HMAC::Sign(const std::string& data,
                unsigned char* digest,
                int digest_length) {
  if (!provider_ || !hkey_)
    return false;

  switch (hash_alg_) {
    case SHA1:
      return SignWithSHA1(data, digest, digest_length);
    default:
      NOTREACHED();
      return false;
  }
}

void HMAC::ImportKey(const unsigned char* key, int key_length) {
  if (key_length > kMaxKeySize) {
    NOTREACHED();
    return;
  }

  struct {
    BLOBHEADER header;
    DWORD key_size;
    BYTE key_data[kMaxKeySize];
  } key_blob;
  key_blob.header.bType = PLAINTEXTKEYBLOB;
  key_blob.header.bVersion = CUR_BLOB_VERSION;
  key_blob.header.reserved = 0;
  key_blob.header.aiKeyAlg = CALG_RC2;
  key_blob.key_size = key_length;
  memcpy(key_blob.key_data, key, key_length);

  if (!CryptImportKey(provider_,
                      reinterpret_cast<const BYTE *>(&key_blob),
                      sizeof(key_blob), 0, 0, &hkey_))
    hkey_ = NULL;

  // Destroy the copy of the key.
  SecureZeroMemory(key_blob.key_data, key_length);
}

bool HMAC::SignWithSHA1(const std::string& data,
                        unsigned char* digest,
                        int digest_length) {
  DCHECK(provider_);
  DCHECK(hkey_);

  if (!CryptCreateHash(provider_, CALG_HMAC, hkey_, 0, &hash_))
    return false;

  HMAC_INFO hmac_info;
  memset(&hmac_info, 0, sizeof(hmac_info));
  hmac_info.HashAlgid = CALG_SHA1;
  if (!CryptSetHashParam(hash_, HP_HMAC_INFO,
                         reinterpret_cast<BYTE*>(&hmac_info), 0))
    return false;

  if (!CryptHashData(hash_,
                     reinterpret_cast<const BYTE*>(data.data()),
                     static_cast<DWORD>(data.size()), 0))
    return false;

  DWORD sha1_size = digest_length;
  if (!CryptGetHashParam(hash_, HP_HASHVAL, digest, &sha1_size, 0))
    return false;

  return true;
}

