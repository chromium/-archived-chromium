// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hmac.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"

namespace base {

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
  // This code doesn't work on Win2k because PLAINTEXTKEYBLOB and
  // CRYPT_IPSEC_HMAC_KEY are not supported on Windows 2000.  PLAINTEXTKEYBLOB
  // allows the import of an unencrypted key.  For Win2k support, a cubmbersome
  // exponent-of-one key procedure must be used:
  //     http://support.microsoft.com/kb/228786/en-us
  // CRYPT_IPSEC_HMAC_KEY allows keys longer than 16 bytes.

  struct KeyBlob {
    BLOBHEADER header;
    DWORD key_size;
    BYTE key_data[1];
  };
  size_t key_blob_size = std::max(offsetof(KeyBlob, key_data) + key_length,
                                  sizeof(KeyBlob));
  std::vector<BYTE> key_blob_storage = std::vector<BYTE>(key_blob_size);
  KeyBlob* key_blob = reinterpret_cast<KeyBlob*>(&key_blob_storage[0]);
  key_blob->header.bType = PLAINTEXTKEYBLOB;
  key_blob->header.bVersion = CUR_BLOB_VERSION;
  key_blob->header.reserved = 0;
  key_blob->header.aiKeyAlg = CALG_RC2;
  key_blob->key_size = key_length;
  memcpy(key_blob->key_data, key, key_length);

  if (!CryptImportKey(provider_, &key_blob_storage[0], key_blob_storage.size(),
                      0, CRYPT_IPSEC_HMAC_KEY, &hkey_)) {
    hkey_ = NULL;
  }

  // Destroy the copy of the key.
  SecureZeroMemory(key_blob->key_data, key_length);
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

}  // namespace base
