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
