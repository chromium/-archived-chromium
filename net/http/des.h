// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_DES_H_
#define NET_HTTP_DES_H_

#include "base/basictypes.h"

namespace net {

//-----------------------------------------------------------------------------
// DES support code for NTLM authentication.
//
// TODO(wtc): Turn this into a C++ API and move it to the base module.

// Build a 64-bit DES key from a 56-bit raw key.
void DESMakeKey(const uint8* raw, uint8* key);

// Run the DES encryption algorithm in ECB mode on one block (8 bytes) of
// data.  |key| is a DES key (8 bytes), |src| is the input plaintext (8
// bytes), and |hash| is an 8-byte buffer receiving the output ciphertext.
void DESEncrypt(const uint8* key, const uint8* src, uint8* hash);

}  // namespace net

#endif  // NET_HTTP_DES_H_
