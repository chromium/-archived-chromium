/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <windows.h>
#include <objbase.h>
#include <stdlib.h>
#include <stdio.h>
#include "nbguidgen/win/md5.h"

// This tool generates name-based GUIDs.

// The quoted comments refer to various sections of RFC 4122, which explains
// inter alia how to generate a name-based GUID.

void GenerateNameBasedGUID(const unsigned char *namespace_guid_bytes,
                           const char *name, unsigned char *guid_bytes) {
  // "Compute the hash of the name space ID concatenated with the name."
  int concatenated_input_len = 16 + static_cast<int>(strlen(name));
  unsigned char *concatenated_inputs =
    static_cast<unsigned char *>(malloc(concatenated_input_len));
  memcpy(concatenated_inputs, namespace_guid_bytes, 16);
  memcpy(&concatenated_inputs[16], name, strlen(name));

  md5_t md5;
  MD5Init(&md5);
  MD5Process(&md5, concatenated_inputs, concatenated_input_len);
  free(concatenated_inputs);
  MD5Finish(&md5, guid_bytes);

  // "Set the four most significant bits (bits 12 through 15) of the
  // time_hi_and_version field to the appropriate 4-bit version number
  // from Section 4.1.3."
  //
  //    Msb0  Msb1  Msb2  Msb3   Version  Description
  //     0     0     1     1        3     The name-based version
  //                                      specified in this document
  //                                      that uses MD5 hashing.
  guid_bytes[6] &= 0x0f;
  guid_bytes[6] |= 0x30;

  // "Set the two most significant bits (bits 6 and 7) of the
  // clock_seq_hi_and_reserved to zero and one, respectively."
  guid_bytes[8] &= 0x3f;
  guid_bytes[8] |= static_cast<unsigned char>(0x80);
}

bool ConvertStringToSerializedGUID(const char *s, unsigned char *g) {
  GUID namespace_guid;
  wchar_t namespace_guid_wide[38 + 1];

  if (strlen(s) > 38) {
    return false;
  }
  if (strlen(s) == 38) {
    mbstowcs(namespace_guid_wide, s, INT_MAX);
  }

  // Undocumented feature: accept GUIDs that are missing braces.
  if (strlen(s) == 36 && s[0] != '{') {
    namespace_guid_wide[0] = L'{';
    mbstowcs(namespace_guid_wide + 1, s, INT_MAX);
    wcscat(namespace_guid_wide, L"}");
  }
  if (FAILED(CLSIDFromString(namespace_guid_wide, &namespace_guid))) {
    return false;
  }

  int ngi = 0;
  g[ngi++] = static_cast<unsigned char>(namespace_guid.Data1 >> 24);
  g[ngi++] = static_cast<unsigned char>(namespace_guid.Data1 >> 16);
  g[ngi++] = static_cast<unsigned char>(namespace_guid.Data1 >> 8);
  g[ngi++] = static_cast<unsigned char>(namespace_guid.Data1);

  g[ngi++] = static_cast<unsigned char>(namespace_guid.Data2 >> 8);
  g[ngi++] = static_cast<unsigned char>(namespace_guid.Data2);

  g[ngi++] = static_cast<unsigned char>(namespace_guid.Data3 >> 8);
  g[ngi++] = static_cast<unsigned char>(namespace_guid.Data3);

  memcpy(&g[ngi], namespace_guid.Data4, sizeof(namespace_guid.Data4));

  return true;
}

// Permanent unit test
//
// Unfortunately, the single concrete example of such a GUID in the RFC is
// incorrect. Authorities on the web suggest that the correct output for
// (6ba7b810-9dad-11d1-80b4-00c04fd430c8, "www.widgets.com") is
// 3d813cbb-47fb-32ba-91df-831e1593ac29, and this program passes that test.
static bool run_unit_test() {
  unsigned char hash[16] = { 0 };
  const unsigned char known_hash[16] = { 0x3d, 0x81, 0x3c, 0xbb,
    0x47, 0xfb,
    0x32, 0xba,
    0x91, 0xdf, 0x83, 0x1e, 0x15, 0x93, 0xac, 0x29 };
  const unsigned char namespace_DNS_GUID[16] = {0x6b, 0xa7, 0xb8, 0x10,
    0x9d, 0xad,
    0x11, 0xd1,
    0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8};
  GenerateNameBasedGUID(namespace_DNS_GUID, "www.widgets.com", hash);

  return (0 == memcmp(hash, known_hash, 16));
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "This tool generates name-based GUIDs as described in "
      "RFC 4122.\r\nUsage: gguidgen namespace-guid name\r\n");
    return 1;
  }

  if (!run_unit_test()) {
    fprintf(stderr, "This program is broken.\r\n");
    return 1;
  }

  unsigned char namespace_guid_as_bytes[16] = { 0 };
  if (!ConvertStringToSerializedGUID(argv[1], namespace_guid_as_bytes)) {
    fprintf(stderr, "Namespace must be a GUID of the form "
      "{00000000-0000-0000-0000-000000000000}.\r\n");
    return 1;
  }

  unsigned char hash[16] = { 0 };
  GenerateNameBasedGUID(namespace_guid_as_bytes, argv[2], hash);

  printf(
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    hash[0], hash[1], hash[2], hash[3],
    hash[4], hash[5],
    hash[6], hash[7],
    hash[8], hash[9],
    hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);

  return 0;
}
