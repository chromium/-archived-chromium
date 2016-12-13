// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/rsa_private_key.h"

#include <iostream>
#include <list>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"


// This file manually encodes and decodes RSA private keys using PrivateKeyInfo
// from PKCS #8 and RSAPrivateKey from PKCS #1. These structures are:
//
// PrivateKeyInfo ::= SEQUENCE {
//   version Version,
//   privateKeyAlgorithm PrivateKeyAlgorithmIdentifier,
//   privateKey PrivateKey,
//   attributes [0] IMPLICIT Attributes OPTIONAL
// }
//
// RSAPrivateKey ::= SEQUENCE {
//   version Version,
//   modulus INTEGER,
//   publicExponent INTEGER,
//   privateExponent INTEGER,
//   prime1 INTEGER,
//   prime2 INTEGER,
//   exponent1 INTEGER,
//   exponent2 INTEGER,
//   coefficient INTEGER
// }


namespace {

// ASN.1 encoding of the AlgorithmIdentifier from PKCS #8.
const uint8 kRsaAlgorithmIdentifier[] = {
  0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01,
  0x05, 0x00
};

// ASN.1 tags for some types we use.
const uint8 kSequenceTag = 0x30;
const uint8 kIntegerTag = 0x02;
const uint8 kNullTag = 0x05;
const uint8 kOctetStringTag = 0x04;

// Helper function to prepend an array of bytes into a list, reversing their
// order. This is needed because ASN.1 integers are big-endian, while CryptoAPI
// uses little-endian.
static void PrependBytesInReverseOrder(uint8* val, int num_bytes,
                                       std::list<uint8>* data) {
  for (int i = 0; i < num_bytes; ++i)
    data->push_front(val[i]);
}

// Helper to prepend an ASN.1 length field.
static void PrependLength(size_t size, std::list<uint8>* data) {
  // The high bit is used to indicate whether additional octets are needed to
  // represent the length.
  if (size < 0x80) {
    data->push_front(static_cast<uint8>(size));
  } else {
    uint8 num_bytes = 0;
    while (size > 0) {
      data->push_front(static_cast<uint8>(size & 0xFF));
      size >>= 8;
      num_bytes++;
    }
    CHECK(num_bytes <= 4);
    data->push_front(0x80 | num_bytes);
  }
}

// Helper to prepend an ASN.1 type header.
static void PrependTypeHeaderAndLength(uint8 type, uint32 length,
                                       std::list<uint8>* output) {
  PrependLength(length, output);
  output->push_front(type);
}

// Helper to prepend an ASN.1 integer.
static void PrependInteger(uint8* val, int num_bytes, std::list<uint8>* data) {
  // Skip trailing null bytes off the MSB end, which is the tail since the input
  // is little endian.
  while (num_bytes > 1 && val[num_bytes - 1] == 0x00)
    num_bytes--;

  PrependBytesInReverseOrder(val, num_bytes, data);

  // If the MSB is set, we need to add an extra null byte, otherwise the integer
  // could be interpreted as negative.
  if ((val[num_bytes - 1] & 0x80) != 0) {
    data->push_front(0x00);
    num_bytes++;
  }

  PrependTypeHeaderAndLength(kIntegerTag, num_bytes, data);
}

// Helper for error handling during key import.
#define READ_ASSERT(truth) \
  if (!(truth)) { \
    NOTREACHED(); \
    return false; \
  }

// Read an ASN.1 length field. This also checks that the length does not extend
// beyond |end|.
static bool ReadLength(uint8** pos, uint8* end, uint32* result) {
  READ_ASSERT(*pos < end);
  int length = 0;

  // If the MSB is not set, the length is just the byte itself.
  if (!(**pos & 0x80)) {
    length = **pos;
    (*pos)++;
  } else {
    // Otherwise, the lower 7 indicate the length of the length.
    int length_of_length = **pos & 0x7F;
    READ_ASSERT(length_of_length <= 4);
    (*pos)++;
    READ_ASSERT(*pos + length_of_length < end);

    length = 0;
    for (int i = 0; i < length_of_length; ++i) {
      length <<= 8;
      length |= **pos;
      (*pos)++;
    }
  }

  READ_ASSERT(*pos + length <= end);
  if (result) *result = length;
  return true;
}

// Read an ASN.1 type header and its length.
static bool ReadTypeHeaderAndLength(uint8** pos, uint8* end,
                                    uint8 expected_tag, uint32* length) {
  READ_ASSERT(*pos < end);
  READ_ASSERT(**pos == expected_tag);
  (*pos)++;

  return ReadLength(pos, end, length);
}

// Read an ASN.1 sequence declaration. This consumes the type header and length
// field, but not the contents of the sequence.
static bool ReadSequence(uint8** pos, uint8* end) {
  return ReadTypeHeaderAndLength(pos, end, kSequenceTag, NULL);
}

// Read the RSA AlgorithmIdentifier.
static bool ReadAlgorithmIdentifier(uint8** pos, uint8* end) {
  READ_ASSERT(*pos + sizeof(kRsaAlgorithmIdentifier) < end);
  READ_ASSERT(memcmp(*pos, kRsaAlgorithmIdentifier,
                     sizeof(kRsaAlgorithmIdentifier)) == 0);
  (*pos) += sizeof(kRsaAlgorithmIdentifier);
  return true;
}

// Read one of the two version fields in PrivateKeyInfo.
static bool ReadVersion(uint8** pos, uint8* end) {
  uint32 length = 0;
  if (!ReadTypeHeaderAndLength(pos, end, kIntegerTag, &length))
    return false;

  // The version should be zero.
  for (uint32 i = 0; i < length; ++i) {
    READ_ASSERT(**pos == 0x00);
    (*pos)++;
  }

  return true;
}

// Read an ASN.1 integer.
static bool ReadInteger(uint8** pos, uint8* end, std::vector<uint8>* out) {
  uint32 length = 0;
  if (!ReadTypeHeaderAndLength(pos, end, kIntegerTag, &length))
    return false;

  // Read the bytes out in reverse order because of endianness.
  for (uint32 i = length - 1; i > 0; --i)
    out->push_back(*(*pos + i));

  // The last byte can be zero to force positiveness. We can ignore this.
  if (**pos != 0x00)
    out->push_back(**pos);

  (*pos) += length;
  return true;
}

static bool ReadIntegerWithExpectedSize(uint8** pos, uint8* end,
                                        int expected_size,
                                        std::vector<uint8>* out) {
  if (!ReadInteger(pos, end, out))
    return false;

  if (out->size() == expected_size + 1) {
    READ_ASSERT(out->back() == 0x00);
    out->pop_back();
  } else {
    READ_ASSERT(out->size() <= expected_size);
  }

  // Pad out any missing bytes with null.
  for (size_t i = out->size(); i < expected_size; ++i)
    out->push_back(0x00);

  return true;
}

}  // namespace


namespace base {

// static
RSAPrivateKey* RSAPrivateKey::Create(uint16 num_bits) {
  scoped_ptr<RSAPrivateKey> result(new RSAPrivateKey);
  if (!result->InitProvider())
    return NULL;

  DWORD flags = CRYPT_EXPORTABLE;

  // The size is encoded as the upper 16 bits of the flags. :: sigh ::.
  flags |= (num_bits << 16);
  if (!CryptGenKey(result->provider_, CALG_RSA_SIGN, flags, &result->key_))
    return NULL;

  return result.release();
}

// static
RSAPrivateKey* RSAPrivateKey::CreateFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  scoped_ptr<RSAPrivateKey> result(new RSAPrivateKey);
  if (!result->InitProvider())
    return NULL;

  uint8* src = const_cast<uint8*>(&input.front());
  uint8* end = src + input.size();
  int version = -1;
  std::vector<uint8> modulus;
  std::vector<uint8> public_exponent;
  std::vector<uint8> private_exponent;
  std::vector<uint8> prime1;
  std::vector<uint8> prime2;
  std::vector<uint8> exponent1;
  std::vector<uint8> exponent2;
  std::vector<uint8> coefficient;

  if (!ReadSequence(&src, end) ||
      !ReadVersion(&src, end) ||
      !ReadAlgorithmIdentifier(&src, end) ||
      !ReadTypeHeaderAndLength(&src, end, kOctetStringTag, NULL) ||
      !ReadSequence(&src, end) ||
      !ReadVersion(&src, end) ||
      !ReadInteger(&src, end, &modulus))
    return false;

  int mod_size = modulus.size();
  READ_ASSERT(mod_size % 2 == 0);
  int primes_size = mod_size / 2;

  if (!ReadIntegerWithExpectedSize(&src, end, 4, &public_exponent) ||
      !ReadIntegerWithExpectedSize(&src, end, mod_size, &private_exponent) ||
      !ReadIntegerWithExpectedSize(&src, end, primes_size, &prime1) ||
      !ReadIntegerWithExpectedSize(&src, end, primes_size, &prime2) ||
      !ReadIntegerWithExpectedSize(&src, end, primes_size, &exponent1) ||
      !ReadIntegerWithExpectedSize(&src, end, primes_size, &exponent2) ||
      !ReadIntegerWithExpectedSize(&src, end, primes_size, &coefficient))
    return false;

  READ_ASSERT(src == end);

  int blob_size = sizeof(PUBLICKEYSTRUC) + sizeof(RSAPUBKEY) + modulus.size() +
                  prime1.size() + prime2.size() +
                  exponent1.size() + exponent2.size() +
                  coefficient.size() + private_exponent.size();
  scoped_array<BYTE> blob(new BYTE[blob_size]);

  uint8* dest = blob.get();
  PUBLICKEYSTRUC* public_key_struc = reinterpret_cast<PUBLICKEYSTRUC*>(dest);
  public_key_struc->bType = PRIVATEKEYBLOB;
  public_key_struc->bVersion = 0x02;
  public_key_struc->reserved = 0;
  public_key_struc->aiKeyAlg = CALG_RSA_SIGN;
  dest += sizeof(PUBLICKEYSTRUC);

  RSAPUBKEY* rsa_pub_key = reinterpret_cast<RSAPUBKEY*>(dest);
  rsa_pub_key->magic = 0x32415352;
  rsa_pub_key->bitlen = modulus.size() * 8;
  int public_exponent_int = 0;
  for (size_t i = public_exponent.size(); i > 0; --i) {
    public_exponent_int <<= 8;
    public_exponent_int |= public_exponent[i - 1];
  }
  rsa_pub_key->pubexp = public_exponent_int;
  dest += sizeof(RSAPUBKEY);

  memcpy(dest, &modulus.front(), modulus.size());
  dest += modulus.size();
  memcpy(dest, &prime1.front(), prime1.size());
  dest += prime1.size();
  memcpy(dest, &prime2.front(), prime2.size());
  dest += prime2.size();
  memcpy(dest, &exponent1.front(), exponent1.size());
  dest += exponent1.size();
  memcpy(dest, &exponent2.front(), exponent2.size());
  dest += exponent2.size();
  memcpy(dest, &coefficient.front(), coefficient.size());
  dest += coefficient.size();
  memcpy(dest, &private_exponent.front(), private_exponent.size());
  dest += private_exponent.size();

  READ_ASSERT(dest == blob.get() + blob_size);
  if (!CryptImportKey(
      result->provider_, reinterpret_cast<uint8*>(public_key_struc), blob_size,
      NULL, CRYPT_EXPORTABLE, &result->key_)) {
    return NULL;
  }

  return result.release();
}

RSAPrivateKey::RSAPrivateKey() : provider_(NULL), key_(NULL) {}

RSAPrivateKey::~RSAPrivateKey() {
  if (key_) {
    if (!CryptDestroyKey(key_))
      NOTREACHED();
  }

  if (provider_) {
    if (!CryptReleaseContext(provider_, 0))
      NOTREACHED();
  }
}

bool RSAPrivateKey::InitProvider() {
  return FALSE != CryptAcquireContext(&provider_, NULL, NULL,
                                      PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
}

bool RSAPrivateKey::ExportPrivateKey(std::vector<uint8>* output) {
  // Export the key
  DWORD blob_length = 0;
  if (!CryptExportKey(key_, NULL, PRIVATEKEYBLOB, 0, NULL, &blob_length)) {
    NOTREACHED();
    return false;
  }

  scoped_array<uint8> blob(new uint8[blob_length]);
  if (!CryptExportKey(key_, NULL, PRIVATEKEYBLOB, 0, blob.get(),
                      &blob_length)) {
    NOTREACHED();
    return false;
  }

  uint8* pos = blob.get();
  PUBLICKEYSTRUC *publickey_struct = reinterpret_cast<PUBLICKEYSTRUC*>(pos);
  pos += sizeof(PUBLICKEYSTRUC);

  RSAPUBKEY *rsa_pub_key = reinterpret_cast<RSAPUBKEY*>(pos);
  pos += sizeof(RSAPUBKEY);

  int mod_size = rsa_pub_key->bitlen / 8;
  int primes_size = rsa_pub_key->bitlen / 16;

  uint8* modulus = pos;
  pos += mod_size;

  uint8* prime1 = pos;
  pos += primes_size;
  uint8* prime2 = pos;
  pos += primes_size;

  uint8* exponent1 = pos;
  pos += primes_size;
  uint8* exponent2 = pos;
  pos += primes_size;

  uint8* coefficient = pos;
  pos += primes_size;

  uint8* private_exponent = pos;
  pos += mod_size;

  CHECK((pos - blob_length) == reinterpret_cast<BYTE*>(publickey_struct));

  std::list<uint8> content;

  // Version (always zero)
  uint8 version = 0;

  // We build up the output in reverse order to prevent having to do copies to
  // figure out the length.
  PrependInteger(coefficient, primes_size, &content);
  PrependInteger(exponent2, primes_size, &content);
  PrependInteger(exponent1, primes_size, &content);
  PrependInteger(prime2, primes_size, &content);
  PrependInteger(prime1, primes_size, &content);
  PrependInteger(private_exponent, mod_size, &content);
  PrependInteger(reinterpret_cast<uint8*>(&rsa_pub_key->pubexp), 4, &content);
  PrependInteger(modulus, mod_size, &content);
  PrependInteger(&version, 1, &content);
  PrependTypeHeaderAndLength(kSequenceTag, content.size(), &content);
  PrependTypeHeaderAndLength(kOctetStringTag, content.size(), &content);

  // RSA algorithm OID
  for (size_t i = sizeof(kRsaAlgorithmIdentifier); i > 0; --i)
    content.push_front(kRsaAlgorithmIdentifier[i - 1]);

  PrependInteger(&version, 1, &content);
  PrependTypeHeaderAndLength(kSequenceTag, content.size(), &content);

  // Copy everying into the output.
  output->reserve(content.size());
  for (std::list<uint8>::iterator i = content.begin(); i != content.end(); ++i)
    output->push_back(*i);

  return true;
}

bool RSAPrivateKey::ExportPublicKey(std::vector<uint8>* output) {
  DWORD key_info_len;
  if (!CryptExportPublicKeyInfo(
      provider_, AT_SIGNATURE, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
      NULL, &key_info_len)) {
    NOTREACHED();
    return false;
  }

  scoped_array<uint8> key_info(new uint8[key_info_len]);
  if (!CryptExportPublicKeyInfo(
      provider_, AT_SIGNATURE, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
      reinterpret_cast<CERT_PUBLIC_KEY_INFO*>(key_info.get()), &key_info_len)) {
    NOTREACHED();
    return false;
  }

  DWORD encoded_length;
  if (!CryptEncodeObject(
      X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
      reinterpret_cast<CERT_PUBLIC_KEY_INFO*>(key_info.get()), NULL,
      &encoded_length)) {
    NOTREACHED();
    return false;
  }

  scoped_array<BYTE> encoded(new BYTE[encoded_length]);
  if (!CryptEncodeObject(
      X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
      reinterpret_cast<CERT_PUBLIC_KEY_INFO*>(key_info.get()), encoded.get(),
      &encoded_length)) {
    NOTREACHED();
    return false;
  }

  for (size_t i = 0; i < encoded_length; ++i)
    output->push_back(encoded[i]);

  return true;
}

}  // namespace base
