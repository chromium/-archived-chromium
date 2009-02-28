// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/des.h"

#if defined(OS_LINUX)
#include <nss.h>
#include <pk11pub.h>
#elif defined(OS_MACOSX)
#include <CommonCrypto/CommonCryptor.h>
#elif defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#endif

#include "base/logging.h"
#if defined(OS_LINUX)
#include "base/nss_init.h"
#endif

// The Mac and Windows (CryptoAPI) versions of DESEncrypt are our own code.
// DESSetKeyParity, DESMakeKey, and the Linux (NSS) version of DESEncrypt are
// based on mozilla/security/manager/ssl/src/nsNTLMAuthModule.cpp,
// CVS rev. 1.14.

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Set odd parity bit (in least significant bit position).
static uint8 DESSetKeyParity(uint8 x) {
  if ((((x >> 7) ^ (x >> 6) ^ (x >> 5) ^
        (x >> 4) ^ (x >> 3) ^ (x >> 2) ^
        (x >> 1)) & 0x01) == 0) {
    x |= 0x01;
  } else {
    x &= 0xfe;
  }
  return x;
}

namespace net {

void DESMakeKey(const uint8* raw, uint8* key) {
  key[0] = DESSetKeyParity(raw[0]);
  key[1] = DESSetKeyParity((raw[0] << 7) | (raw[1] >> 1));
  key[2] = DESSetKeyParity((raw[1] << 6) | (raw[2] >> 2));
  key[3] = DESSetKeyParity((raw[2] << 5) | (raw[3] >> 3));
  key[4] = DESSetKeyParity((raw[3] << 4) | (raw[4] >> 4));
  key[5] = DESSetKeyParity((raw[4] << 3) | (raw[5] >> 5));
  key[6] = DESSetKeyParity((raw[5] << 2) | (raw[6] >> 6));
  key[7] = DESSetKeyParity((raw[6] << 1));
}

#if defined(OS_LINUX)

void DESEncrypt(const uint8* key, const uint8* src, uint8* hash) {
  CK_MECHANISM_TYPE cipher_mech = CKM_DES_ECB;
  PK11SlotInfo* slot = NULL;
  PK11SymKey* symkey = NULL;
  PK11Context* ctxt = NULL;
  SECItem key_item;
  SECItem* param = NULL;
  SECStatus rv;
  unsigned int n;

  base::EnsureNSSInit();

  slot = PK11_GetBestSlot(cipher_mech, NULL);
  if (!slot)
    goto done;

  key_item.data = const_cast<uint8*>(key);
  key_item.len = 8;
  symkey = PK11_ImportSymKey(slot, cipher_mech,
                             PK11_OriginUnwrap, CKA_ENCRYPT,
                             &key_item, NULL);
  if (!symkey)
    goto done;

  // No initialization vector required.
  param = PK11_ParamFromIV(cipher_mech, NULL);
  if (!param)
    goto done;

  ctxt = PK11_CreateContextBySymKey(cipher_mech, CKA_ENCRYPT,
                                    symkey, param);
  if (!ctxt)
    goto done;

  rv = PK11_CipherOp(ctxt, hash, reinterpret_cast<int*>(&n), 8,
                     const_cast<uint8*>(src), 8);
  if (rv != SECSuccess)
    goto done;

  // TODO(wtc): Should this be PK11_Finalize?
  rv = PK11_DigestFinal(ctxt, hash+8, &n, 0);
  if (rv != SECSuccess)
    goto done;

 done:
  if (ctxt)
    PK11_DestroyContext(ctxt, PR_TRUE);
  if (symkey)
    PK11_FreeSymKey(symkey);
  if (param)
    SECITEM_FreeItem(param, PR_TRUE);
  if (slot)
    PK11_FreeSlot(slot);
}

#elif defined(OS_MACOSX)

void DESEncrypt(const uint8* key, const uint8* src, uint8* hash) {
  CCCryptorStatus status;
  size_t data_out_moved = 0;
  status = CCCrypt(kCCEncrypt, kCCAlgorithmDES, kCCOptionECBMode,
                   key, 8, NULL, src, 8, hash, 8, &data_out_moved);
  DCHECK(status == kCCSuccess);
  DCHECK(data_out_moved == 8);
}

#elif defined(OS_WIN)

void DESEncrypt(const uint8* key, const uint8* src, uint8* hash) {
  HCRYPTPROV provider;
  HCRYPTKEY hkey = NULL;

  if (!CryptAcquireContext(&provider, NULL, NULL,
                           PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
    provider = NULL;
    goto done;
  }

  // Import the DES key.
  // This code doesn't work on Win2k because PLAINTEXTKEYBLOB is not supported
  // on Windows 2000.  PLAINTEXTKEYBLOB allows the import of an unencrypted
  // key.  For Win2k support, a cubmbersome exponent-of-one key procedure must
  // be used:
  //     http://support.microsoft.com/kb/228786/en-us

  struct KeyBlob {
    BLOBHEADER header;
    DWORD key_size;
    BYTE key_data[8];
  };
  KeyBlob key_blob;
  key_blob.header.bType = PLAINTEXTKEYBLOB;
  key_blob.header.bVersion = CUR_BLOB_VERSION;
  key_blob.header.reserved = 0;
  key_blob.header.aiKeyAlg = CALG_DES;
  key_blob.key_size = 8;  // 64 bits
  memcpy(key_blob.key_data, key, 8);

  if (!CryptImportKey(provider, reinterpret_cast<BYTE*>(&key_blob),
                      sizeof(key_blob), 0, 0, &hkey)) {
    hkey = NULL;
    goto done;
  }

  // Destroy the copy of the key.
  SecureZeroMemory(key_blob.key_data, sizeof(key_blob.key_data));

  // No initialization vector required.
  DWORD cipher_mode = CRYPT_MODE_ECB;
  if (!CryptSetKeyParam(hkey, KP_MODE,
                        reinterpret_cast<BYTE*>(&cipher_mode), 0))
    goto done;

  // CryptoAPI requires us to copy the plaintext to the output buffer first.
  CopyMemory(hash, src, 8);
  // Pass a 'Final' of FALSE, otherwise CryptEncrypt appends one additional
  // block of padding to the data.
  DWORD hash_len = 8;
  if (!CryptEncrypt(hkey, NULL, FALSE, 0, hash, &hash_len, 8))
    goto done;

 done:
  if (hkey)
    CryptDestroyKey(hkey);
  if (provider)
    CryptReleaseContext(provider, 0);
}

#endif

}  // namespace net
