// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility class for calculating the HMAC for a given message. We currently
// only support SHA1 for the hash algorithm, but this can be extended easily.

#ifndef BASE_HMAC_H_
#define BASE_HMAC_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#endif

#include <string>

#include "base/basictypes.h"

namespace base {

class HMAC {
 public:
  // The set of supported hash functions. Extend as required.
  enum HashAlgorithm {
    SHA1
  };

  HMAC(HashAlgorithm hash_alg, const unsigned char* key, int key_length);
  ~HMAC();

  // Calculates the HMAC for the message in |data| using the algorithm and key
  // supplied to the constructor.  The HMAC is returned in |digest|, which
  // has |digest_length| bytes of storage available.
  bool Sign(const std::string& data, unsigned char* digest, int digest_length);

 private:
#if defined(OS_POSIX)
  HashAlgorithm hash_alg_;
  std::string key_;
#elif defined(OS_WIN)
  // Import the key so that we don't have to store it ourself.
  void ImportKey(const unsigned char* key, int key_length);

  // Returns the SHA1 hash of 'data' and 'key' in 'digest'. If there was any
  // error in the calculation, this method returns false, otherwise true.
  bool SignWithSHA1(const std::string& data,
                    unsigned char* digest,
                    int digest_length);

  // The hash algorithm to use.
  HashAlgorithm hash_alg_;

  // Windows Crypt API resources.
  HCRYPTPROV provider_;
  HCRYPTHASH hash_;
  HCRYPTKEY hkey_;
#endif  // OS_WIN

  DISALLOW_COPY_AND_ASSIGN(HMAC);
};

}  // namespace base

#endif  // BASE_HMAC_H_
