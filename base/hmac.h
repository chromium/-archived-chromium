// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility class for calculating the HMAC for a given message. We currently
// only support SHA1 for the hash algorithm, but this can be extended easily.

#ifndef BASE_HMAC_H__
#define BASE_HMAC_H__

#include <windows.h>
#include <wincrypt.h>

#include <string>

#include "base/basictypes.h"

class HMAC {
 public:
  // The set of supported hash functions. Extend as required.
  enum HashAlgorithm {
    SHA1
  };

  HMAC(HashAlgorithm hash_alg, const unsigned char* key, int key_length);
  ~HMAC();

  // Returns the HMAC in 'digest' for the message in 'data' and the key
  // specified in the contructor.
  bool Sign(const std::string& data, unsigned char* digest, int digest_length);

 private:
  // Import the key so that we don't have to store it ourself.
  // TODO(paulg): Bug: http://b/1084719, 'ImportKey' will not currently work on
  //              Windows 2000 since it requires special handling for importing
  //              keys. See this link for details:
  // http://www.derkeiler.com/Newsgroups/microsoft.public.platformsdk.security/2004-06/0270.html
  void ImportKey(const unsigned char* key, int key_length);

  // Returns the SHA1 hash of 'data' and 'key' in 'digest'. If there was any
  // error in the calculation, this method returns false, otherwise true.
  bool SignWithSHA1(const std::string& data,
                    unsigned char* digest,
                    int digest_length);

  // Required for the SHA1 key_blob struct. We limit this to 16 bytes since
  // Windows 2000 doesn't support keys larger than that.
  static const int kMaxKeySize = 16;

  // The hash algorithm to use.
  HashAlgorithm hash_alg_;

  // Windows Crypt API resources.
  HCRYPTPROV provider_;
  HCRYPTHASH hash_;
  HCRYPTKEY hkey_;

  DISALLOW_EVIL_CONSTRUCTORS(HMAC);
};


#endif  // BASE_HMAC_H__

