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