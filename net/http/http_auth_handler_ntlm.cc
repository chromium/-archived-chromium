// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_ntlm.h"

#include <stdlib.h>
// For gethostname
#if defined(OS_POSIX)
#include <unistd.h>
#elif defined(OS_WIN)
#include <winsock2.h>
#endif

#include "base/md5.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "net/base/base64.h"
#include "net/base/net_errors.h"
#include "net/http/des.h"
#include "net/http/md4.h"

namespace net {

// Based on mozilla/security/manager/ssl/src/nsNTLMAuthModule.cpp,
// CVS rev. 1.14.
//
// TODO(wtc):
// - The IS_BIG_ENDIAN code is not tested.
// - Enable the logging code or just delete it.
// - Delete or comment out the LM code, which hasn't been tested and isn't
//   being used.

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

// Discover the endianness by testing processor architecture.
#if defined(ARCH_CPU_X86) || defined(ARCH_CPU_X86_64)
#define IS_LITTLE_ENDIAN 1
#undef  IS_BIG_ENDIAN
#else
#error "Unknown endianness"
#endif

#define NTLM_LOG(x) ((void) 0)

//-----------------------------------------------------------------------------
// This file contains a cross-platform NTLM authentication implementation. It
// is based on documentation from: http://davenport.sourceforge.net/ntlm.html
//-----------------------------------------------------------------------------

enum {
  NTLM_NegotiateUnicode             = 0x00000001,
  NTLM_NegotiateOEM                 = 0x00000002,
  NTLM_RequestTarget                = 0x00000004,
  NTLM_Unknown1                     = 0x00000008,
  NTLM_NegotiateSign                = 0x00000010,
  NTLM_NegotiateSeal                = 0x00000020,
  NTLM_NegotiateDatagramStyle       = 0x00000040,
  NTLM_NegotiateLanManagerKey       = 0x00000080,
  NTLM_NegotiateNetware             = 0x00000100,
  NTLM_NegotiateNTLMKey             = 0x00000200,
  NTLM_Unknown2                     = 0x00000400,
  NTLM_Unknown3                     = 0x00000800,
  NTLM_NegotiateDomainSupplied      = 0x00001000,
  NTLM_NegotiateWorkstationSupplied = 0x00002000,
  NTLM_NegotiateLocalCall           = 0x00004000,
  NTLM_NegotiateAlwaysSign          = 0x00008000,
  NTLM_TargetTypeDomain             = 0x00010000,
  NTLM_TargetTypeServer             = 0x00020000,
  NTLM_TargetTypeShare              = 0x00040000,
  NTLM_NegotiateNTLM2Key            = 0x00080000,
  NTLM_RequestInitResponse          = 0x00100000,
  NTLM_RequestAcceptResponse        = 0x00200000,
  NTLM_RequestNonNTSessionKey       = 0x00400000,
  NTLM_NegotiateTargetInfo          = 0x00800000,
  NTLM_Unknown4                     = 0x01000000,
  NTLM_Unknown5                     = 0x02000000,
  NTLM_Unknown6                     = 0x04000000,
  NTLM_Unknown7                     = 0x08000000,
  NTLM_Unknown8                     = 0x10000000,
  NTLM_Negotiate128                 = 0x20000000,
  NTLM_NegotiateKeyExchange         = 0x40000000,
  NTLM_Negotiate56                  = 0x80000000
};

// We send these flags with our type 1 message.
enum {
  NTLM_TYPE1_FLAGS =
      NTLM_NegotiateUnicode |
      NTLM_NegotiateOEM |
      NTLM_RequestTarget |
      NTLM_NegotiateNTLMKey |
      NTLM_NegotiateAlwaysSign |
      NTLM_NegotiateNTLM2Key
};

static const char NTLM_SIGNATURE[] = "NTLMSSP";
static const char NTLM_TYPE1_MARKER[] = { 0x01, 0x00, 0x00, 0x00 };
static const char NTLM_TYPE2_MARKER[] = { 0x02, 0x00, 0x00, 0x00 };
static const char NTLM_TYPE3_MARKER[] = { 0x03, 0x00, 0x00, 0x00 };

enum {
  NTLM_TYPE1_HEADER_LEN = 32,
  NTLM_TYPE2_HEADER_LEN = 32,
  NTLM_TYPE3_HEADER_LEN = 64,

  LM_HASH_LEN = 16,
  LM_RESP_LEN = 24,

  NTLM_HASH_LEN = 16,
  NTLM_RESP_LEN = 24
};

//-----------------------------------------------------------------------------

// The return value of this function controls whether or not the LM hash will
// be included in response to a NTLM challenge.
//
// In Mozilla, this function returns the value of the boolean preference
// "network.ntlm.send-lm-response".  By default, the preference is disabled
// since servers should almost never need the LM hash, and the LM hash is what
// makes NTLM authentication less secure.  See
// https://bugzilla.mozilla.org/show_bug.cgi?id=250691 for further details.
//
// We just return a hardcoded false.
static bool SendLM() {
  return false;
}

//-----------------------------------------------------------------------------

#define LogFlags(x) ((void) 0)
#define LogBuf(a, b, c) ((void) 0)
#define LogToken(a, b, c) ((void) 0)

//-----------------------------------------------------------------------------

// Byte order swapping.
#define SWAP16(x) ((((x) & 0xff) << 8) | (((x) >> 8) & 0xff))
#define SWAP32(x) ((SWAP16((x) & 0xffff) << 16) | (SWAP16((x) >> 16)))

static void* WriteBytes(void* buf, const void* data, uint32 data_len) {
  memcpy(buf, data, data_len);
  return static_cast<char*>(buf) + data_len;
}

static void* WriteDWORD(void* buf, uint32 dword) {
#ifdef IS_BIG_ENDIAN
  // NTLM uses little endian on the wire.
  dword = SWAP32(dword);
#endif
  return WriteBytes(buf, &dword, sizeof(dword));
}

static void* WriteSecBuf(void* buf, uint16 length, uint32 offset) {
#ifdef IS_BIG_ENDIAN
  length = SWAP16(length);
  offset = SWAP32(offset);
#endif
  buf = WriteBytes(buf, &length, sizeof(length));
  buf = WriteBytes(buf, &length, sizeof(length));
  buf = WriteBytes(buf, &offset, sizeof(offset));
  return buf;
}

#ifdef IS_BIG_ENDIAN
/**
 * WriteUnicodeLE copies a unicode string from one buffer to another.  The
 * resulting unicode string is in little-endian format.  The input string is
 * assumed to be in the native endianness of the local machine.  It is safe
 * to pass the same buffer as both input and output, which is a handy way to
 * convert the unicode buffer to little-endian on big-endian platforms.
 */
static void* WriteUnicodeLE(void* buf, const char16* str, uint32 str_len) {
  // Convert input string from BE to LE.
  uint8* cursor = static_cast<uint8*>(buf);
  const uint8* input  = reinterpret_cast<const uint8*>(str);
  for (uint32 i = 0; i < str_len; ++i, input += 2, cursor += 2) {
    // Allow for the case where |buf == str|.
    uint8 temp = input[0];
    cursor[0] = input[1];
    cursor[1] = temp;
  }
  return buf;
}
#endif

static uint16 ReadUint16(const uint8*& buf) {
  uint16 x = (static_cast<uint16>(buf[0]))      |
             (static_cast<uint16>(buf[1]) << 8);
  buf += sizeof(x);
  return x;
}

static uint32 ReadUint32(const uint8*& buf) {
  uint32 x = (static_cast<uint32>(buf[0]))       |
             (static_cast<uint32>(buf[1]) << 8)  |
             (static_cast<uint32>(buf[2]) << 16) |
             (static_cast<uint32>(buf[3]) << 24);
  buf += sizeof(x);
  return x;
}

//-----------------------------------------------------------------------------

static void ZapBuf(void* buf, size_t buf_len) {
  memset(buf, 0, buf_len);
}

// TODO(wtc): Can we implement ZapString as
// s.replace(0, s.size(), s.size(), '\0)?
static void ZapString(std::string* s) {
  ZapBuf(&(*s)[0], s->length());
}

static void ZapString(string16* s) {
  ZapBuf(&(*s)[0], s->length() * 2);
}

// LM_Hash computes the LM hash of the given password.
//
// param password
//       unicode password.
// param hash
//       16-byte result buffer
//
// Note: This function is not being used because our SendLM() function always
// returns false.
static void LM_Hash(const string16& password, uint8* hash) {
  static const uint8 LM_MAGIC[] = "KGS!@#$%";

  // Convert password to OEM character set.  We'll just use the native
  // filesystem charset.
  std::string passbuf = base::SysWideToNativeMB(UTF16ToWide(password));
  StringToUpperASCII(&passbuf);
  passbuf.resize(14, '\0');

  uint8 k1[8], k2[8];
  DESMakeKey(reinterpret_cast<const uint8*>(passbuf.data())    , k1);
  DESMakeKey(reinterpret_cast<const uint8*>(passbuf.data()) + 7, k2);
  ZapString(&passbuf);

  // Use password keys to hash LM magic string twice.
  DESEncrypt(k1, LM_MAGIC, hash);
  DESEncrypt(k2, LM_MAGIC, hash + 8);
}

// NTLM_Hash computes the NTLM hash of the given password.
//
// param password
//       null-terminated unicode password.
// param hash
//       16-byte result buffer
static void NTLM_Hash(const string16& password, uint8* hash) {
#ifdef IS_BIG_ENDIAN
  uint32 len = password.length();
  uint8* passbuf;

  passbuf = static_cast<uint8*>(malloc(len * 2));
  WriteUnicodeLE(passbuf, password.data(), len);
  weak_crypto::MD4Sum(passbuf, len * 2, hash);

  ZapBuf(passbuf, len * 2);
  free(passbuf);
#else
  weak_crypto::MD4Sum(reinterpret_cast<const uint8*>(password.data()),
                      password.length() * 2, hash);
#endif
}

//-----------------------------------------------------------------------------

// LM_Response generates the LM response given a 16-byte password hash and the
// challenge from the Type-2 message.
//
// param hash
//       16-byte password hash
// param challenge
//       8-byte challenge from Type-2 message
// param response
//       24-byte buffer to contain the LM response upon return
static void LM_Response(const uint8* hash,
                        const uint8* challenge,
                        uint8* response) {
  uint8 keybytes[21], k1[8], k2[8], k3[8];

  memcpy(keybytes, hash, 16);
  ZapBuf(keybytes + 16, 5);

  DESMakeKey(keybytes     , k1);
  DESMakeKey(keybytes +  7, k2);
  DESMakeKey(keybytes + 14, k3);

  DESEncrypt(k1, challenge, response);
  DESEncrypt(k2, challenge, response + 8);
  DESEncrypt(k3, challenge, response + 16);
}

//-----------------------------------------------------------------------------

// Returns OK or a network error code.
static int GenerateType1Msg(void** out_buf, uint32* out_len) {
  //
  // Verify that buf_len is sufficient.
  //
  *out_len = NTLM_TYPE1_HEADER_LEN;
  *out_buf = malloc(*out_len);
  if (!*out_buf)
    return ERR_OUT_OF_MEMORY;

  //
  // Write out type 1 message.
  //
  void* cursor = *out_buf;

  // 0 : signature
  cursor = WriteBytes(cursor, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE));

  // 8 : marker
  cursor = WriteBytes(cursor, NTLM_TYPE1_MARKER, sizeof(NTLM_TYPE1_MARKER));

  // 12 : flags
  cursor = WriteDWORD(cursor, NTLM_TYPE1_FLAGS);

  //
  // NOTE: It is common for the domain and workstation fields to be empty.
  //       This is true of Win2k clients, and my guess is that there is
  //       little utility to sending these strings before the charset has
  //       been negotiated.  We follow suite -- anyways, it doesn't hurt
  //       to save some bytes on the wire ;-)
  //

  // 16 : supplied domain security buffer (empty)
  cursor = WriteSecBuf(cursor, 0, 0);

  // 24 : supplied workstation security buffer (empty)
  cursor = WriteSecBuf(cursor, 0, 0);

  return OK;
}

struct Type2Msg {
  uint32      flags;         // NTLM_Xxx bitwise combination
  uint8       challenge[8];  // 8 byte challenge
  const void* target;        // target string (type depends on flags)
  uint32      target_len;    // target length in bytes
};

// Returns OK or a network error code.
// TODO(wtc): This function returns ERR_UNEXPECTED when the input message is
// invalid.  We should return a better error code.
static int ParseType2Msg(const void* in_buf, uint32 in_len, Type2Msg* msg) {
  // Make sure in_buf is long enough to contain a meaningful type2 msg.
  //
  // 0  NTLMSSP Signature
  // 8  NTLM Message Type
  // 12 Target Name
  // 20 Flags
  // 24 Challenge
  // 32 end of header, start of optional data blocks
  //
  if (in_len < NTLM_TYPE2_HEADER_LEN)
    return ERR_UNEXPECTED;

  const uint8* cursor = (const uint8*) in_buf;

  // verify NTLMSSP signature
  if (memcmp(cursor, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE)) != 0)
    return ERR_UNEXPECTED;
  cursor += sizeof(NTLM_SIGNATURE);

  // verify Type-2 marker
  if (memcmp(cursor, NTLM_TYPE2_MARKER, sizeof(NTLM_TYPE2_MARKER)) != 0)
    return ERR_UNEXPECTED;
  cursor += sizeof(NTLM_TYPE2_MARKER);

  // read target name security buffer
  msg->target_len = ReadUint16(cursor);
  ReadUint16(cursor);  // discard next 16-bit value
  uint32 offset = ReadUint32(cursor);  // get offset from in_buf
  msg->target = ((const uint8*) in_buf) + offset;

  // read flags
  msg->flags = ReadUint32(cursor);

  // read challenge
  memcpy(msg->challenge, cursor, sizeof(msg->challenge));
  cursor += sizeof(msg->challenge);

  NTLM_LOG(("NTLM type 2 message:\n"));
  LogBuf("target", (const uint8*) msg->target, msg->target_len);
  LogBuf("flags", (const uint8*) &msg->flags, 4);
  LogFlags(msg->flags);
  LogBuf("challenge", msg->challenge, sizeof(msg->challenge));

  // We currently do not implement LMv2/NTLMv2 or NTLM2 responses,
  // so we can ignore target information.  We may want to enable
  // support for these alternate mechanisms in the future.
  return OK;
}

// Returns OK or a network error code.
static int GenerateType3Msg(const string16& domain,
                            const string16& username,
                            const string16& password,
                            const void*     in_buf,
                            uint32          in_len,
                            void**          out_buf,
                            uint32*         out_len) {
  // in_buf contains Type-2 msg (the challenge) from server.

  int rv;
  Type2Msg msg;

  rv = ParseType2Msg(in_buf, in_len, &msg);
  if (rv != OK)
    return rv;

  bool unicode = (msg.flags & NTLM_NegotiateUnicode) != 0;

  // Temporary buffers for unicode strings
#ifdef IS_BIG_ENDIAN
  string16 ucs_domain_buf, ucs_user_buf;
#endif
  string16 ucs_host_buf;
  // Temporary buffers for oem strings
  std::string oem_domain_buf, oem_user_buf, oem_host_buf;
  // Pointers and lengths for the string buffers; encoding is unicode if
  // the "negotiate unicode" flag was set in the Type-2 message.
  const void* domain_ptr;
  const void* user_ptr;
  const void* host_ptr;
  uint32 domain_len, user_len, host_len;

  //
  // Get domain name.
  //
  if (unicode) {
#ifdef IS_BIG_ENDIAN
    ucs_domain_buf = domain;
    domain_ptr = ucs_domain_buf.data();
    domain_len = ucs_domain_buf.length() * 2;
    WriteUnicodeLE(const_cast<void*>(domain_ptr), (const char16*) domain_ptr,
                   ucs_domain_buf.length());
#else
    domain_ptr = domain.data();
    domain_len = domain.length() * 2;
#endif
  } else {
    oem_domain_buf = base::SysWideToNativeMB(UTF16ToWide(domain));
    domain_ptr = oem_domain_buf.data();
    domain_len = oem_domain_buf.length();
  }

  //
  // Get user name.
  //
  if (unicode) {
#ifdef IS_BIG_ENDIAN
    ucs_user_buf = username;
    user_ptr = ucs_user_buf.data();
    user_len = ucs_user_buf.length() * 2;
    WriteUnicodeLE(const_cast<void*>(user_ptr), (const char16*) user_ptr,
                   ucs_user_buf.length());
#else
    user_ptr = username.data();
    user_len = username.length() * 2;
#endif
  } else {
    oem_user_buf = base::SysWideToNativeMB(UTF16ToWide(username));
    user_ptr = oem_user_buf.data();
    user_len = oem_user_buf.length();
  }

  //
  // Get workstation name (use local machine's hostname).
  //
  char host_buf[256];  // Host names are limited to 255 bytes.
  if (gethostname(host_buf, sizeof(host_buf)) != 0)
    return ERR_UNEXPECTED;
  host_len = strlen(host_buf);
  if (unicode) {
    // hostname is ASCII, so we can do a simple zero-pad expansion:
    ucs_host_buf.assign(host_buf, host_buf + host_len);
    host_ptr = ucs_host_buf.data();
    host_len = ucs_host_buf.length() * 2;
#ifdef IS_BIG_ENDIAN
    WriteUnicodeLE(const_cast<void*>(host_ptr), (const char16*) host_ptr,
                   ucs_host_buf.length());
#endif
  } else {
    host_ptr = host_buf;
  }

  //
  // Now that we have generated all of the strings, we can allocate out_buf.
  //
  *out_len = NTLM_TYPE3_HEADER_LEN + host_len + domain_len + user_len +
             LM_RESP_LEN + NTLM_RESP_LEN;
  *out_buf = malloc(*out_len);
  if (!*out_buf)
    return ERR_OUT_OF_MEMORY;

  //
  // Next, we compute the LM and NTLM responses.
  //
  uint8 lm_resp[LM_RESP_LEN];
  uint8 ntlm_resp[NTLM_RESP_LEN];
  uint8 ntlm_hash[NTLM_HASH_LEN];
  if (msg.flags & NTLM_NegotiateNTLM2Key) {
    // compute NTLM2 session response
    MD5Digest session_hash;
    uint8 temp[16];

    // TODO(wtc): Add a function that generates random bytes so we can say:
    //   GenerateRandom(lm_resp, 8);
    for (int i = 0; i < 8; ++i)
      lm_resp[i] = base::RandInt(0, 255);
    memset(lm_resp + 8, 0, LM_RESP_LEN - 8);

    memcpy(temp, msg.challenge, 8);
    memcpy(temp + 8, lm_resp, 8);
    MD5Sum(temp, 16, &session_hash);

    NTLM_Hash(password, ntlm_hash);
    LM_Response(ntlm_hash, session_hash.a, ntlm_resp);
  } else {
    NTLM_Hash(password, ntlm_hash);
    LM_Response(ntlm_hash, msg.challenge, ntlm_resp);

    if (SendLM()) {
      uint8 lm_hash[LM_HASH_LEN];
      LM_Hash(password, lm_hash);
      LM_Response(lm_hash, msg.challenge, lm_resp);
    } else {
      // According to http://davenport.sourceforge.net/ntlm.html#ntlmVersion2,
      // the correct way to not send the LM hash is to send the NTLM hash twice
      // in both the LM and NTLM response fields.
      LM_Response(ntlm_hash, msg.challenge, lm_resp);
    }
  }

  //
  // Finally, we assemble the Type-3 msg :-)
  //
  void* cursor = *out_buf;
  uint32 offset;

  // 0 : signature
  cursor = WriteBytes(cursor, NTLM_SIGNATURE, sizeof(NTLM_SIGNATURE));

  // 8 : marker
  cursor = WriteBytes(cursor, NTLM_TYPE3_MARKER, sizeof(NTLM_TYPE3_MARKER));

  // 12 : LM response sec buf
  offset = NTLM_TYPE3_HEADER_LEN + domain_len + user_len + host_len;
  cursor = WriteSecBuf(cursor, LM_RESP_LEN, offset);
  memcpy(static_cast<uint8*>(*out_buf) + offset, lm_resp, LM_RESP_LEN);

  // 20 : NTLM response sec buf
  offset += LM_RESP_LEN;
  cursor = WriteSecBuf(cursor, NTLM_RESP_LEN, offset);
  memcpy(static_cast<uint8*>(*out_buf) + offset, ntlm_resp, NTLM_RESP_LEN);

  // 28 : domain name sec buf
  offset = NTLM_TYPE3_HEADER_LEN;
  cursor = WriteSecBuf(cursor, domain_len, offset);
  memcpy(static_cast<uint8*>(*out_buf) + offset, domain_ptr, domain_len);

  // 36 : user name sec buf
  offset += domain_len;
  cursor = WriteSecBuf(cursor, user_len, offset);
  memcpy(static_cast<uint8*>(*out_buf) + offset, user_ptr, user_len);

  // 44 : workstation (host) name sec buf
  offset += user_len;
  cursor = WriteSecBuf(cursor, host_len, offset);
  memcpy(static_cast<uint8*>(*out_buf) + offset, host_ptr, host_len);

  // 52 : session key sec buf (not used)
  cursor = WriteSecBuf(cursor, 0, 0);

  // 60 : negotiated flags
  cursor = WriteDWORD(cursor, msg.flags & NTLM_TYPE1_FLAGS);

  return OK;
}

//-----------------------------------------------------------------------------

class NTLMAuthModule {
 public:
  NTLMAuthModule() {}

  ~NTLMAuthModule();

  int Init(const string16& domain,
           const string16& username,
           const string16& password);

  int GetNextToken(const void* in_token,
                   uint32 in_token_len,
                   void** out_token,
                   uint32* out_token_len);

 private:
  string16 domain_;
  string16 username_;
  string16 password_;
};

NTLMAuthModule::~NTLMAuthModule() {
  ZapString(&password_);
}

int NTLMAuthModule::Init(const string16& domain,
                         const string16& username,
                         const string16& password) {
  domain_ = domain;
  username_ = username;
  password_ = password;
  return OK;
}

int NTLMAuthModule::GetNextToken(const void* in_token,
                                 uint32 in_token_len,
                                 void** out_token,
                                 uint32* out_token_len) {
  int rv;

  // If in_token is non-null, then assume it contains a type 2 message...
  if (in_token) {
    LogToken("in-token", in_token, in_token_len);
    rv = GenerateType3Msg(domain_, username_, password_, in_token,
                          in_token_len, out_token, out_token_len);
  } else {
    rv = GenerateType1Msg(out_token, out_token_len);
  }

  if (rv == OK)
    LogToken("out-token", *out_token, *out_token_len);

  return rv;
}

// NTLM authentication is specified in "NTLM Over HTTP Protocol Specification"
// [MS-NTHT].

HttpAuthHandlerNTLM::HttpAuthHandlerNTLM()
    : ntlm_module_(new NTLMAuthModule) {
}

HttpAuthHandlerNTLM::~HttpAuthHandlerNTLM() {
}

bool HttpAuthHandlerNTLM::NeedsIdentity() {
  return !auth_data_.empty();
}

std::string HttpAuthHandlerNTLM::GenerateCredentials(
    const std::wstring& username,
    const std::wstring& password,
    const HttpRequestInfo* request,
    const ProxyInfo* proxy) {
  int rv;
  // TODO(wtc): See if we can use char* instead of void* for in_buf and
  // out_buf.  This change will need to propagate to GetNextToken,
  // GenerateType1Msg, and GenerateType3Msg, and perhaps further.
  const void* in_buf;
  void* out_buf;
  uint32 in_buf_len, out_buf_len;
  std::string decoded_auth_data;

  // |username| may be in the form "DOMAIN\user".  Parse it into the two
  // components.
  std::wstring domain;
  std::wstring user;
  size_t backslash_idx = username.find(L'\\');
  if (backslash_idx == std::wstring::npos) {
    user = username;
  } else {
    domain = username.substr(0, backslash_idx);
    user = username.substr(backslash_idx + 1);
  }
  rv = ntlm_module_->Init(WideToUTF16(domain), WideToUTF16(user),
                          WideToUTF16(password));

  // Initial challenge.
  if (auth_data_.empty()) {
    in_buf_len = 0;
    in_buf = NULL;
  } else {
    // Decode |auth_data_| into the input buffer.
    int len = auth_data_.length();

    // Strip off any padding.
    // (See https://bugzilla.mozilla.org/show_bug.cgi?id=230351.)
    //
    // Our base64 decoder requires that the length be a multiple of 4.
    while (len > 0 && len % 4 != 0 && auth_data_[len - 1] == '=')
      len--;
    auth_data_.erase(len);

    if (!Base64Decode(auth_data_, &decoded_auth_data))
      return std::string();  // Improper base64 encoding
    in_buf_len = decoded_auth_data.length();
    in_buf = decoded_auth_data.data();
  }

  rv = ntlm_module_->GetNextToken(in_buf, in_buf_len, &out_buf, &out_buf_len);
  if (rv != OK)
    return std::string();

  // Base64 encode data in output buffer and prepend "NTLM ".
  std::string encode_input(static_cast<char*>(out_buf), out_buf_len);
  std::string encode_output;
  bool ok = Base64Encode(encode_input, &encode_output);
  // OK, we are done with |out_buf|
  free(out_buf);
  if (!ok)
    return std::string();
  return std::string("NTLM ") + encode_output;
}

// The NTLM challenge header looks like:
//   WWW-Authenticate: NTLM auth-data
bool HttpAuthHandlerNTLM::ParseChallenge(
    std::string::const_iterator challenge_begin,
    std::string::const_iterator challenge_end) {
  scheme_ = "ntlm";
  score_ = 3;
  properties_ = ENCRYPTS_IDENTITY | IS_CONNECTION_BASED;
  auth_data_.clear();

  // Verify the challenge's auth-scheme.
  HttpAuth::ChallengeTokenizer challenge_tok(challenge_begin, challenge_end);
  if (!challenge_tok.valid() ||
      !LowerCaseEqualsASCII(challenge_tok.scheme(), "ntlm"))
    return false;

  // Extract the auth-data.  We can't use challenge_tok.GetNext() because
  // auth-data is base64-encoded and may contain '=' padding at the end,
  // which would be mistaken for a name=value pair.
  challenge_begin += 4;  // Skip over "NTLM".
  HttpUtil::TrimLWS(&challenge_begin, &challenge_end);

  auth_data_.assign(challenge_begin, challenge_end);

  return true;
}

}  // namespace net
