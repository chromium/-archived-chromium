// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8statetable.h"

// This code is copied from google3/util/utf8/internal/utf8statetable.cc and was
// not modified (it generates a lot of lint warnings, but I decided not to fix
// them to simplify its maintenance).


// Return true if current Tbl pointer is within state0 range
// Note that unsigned compare checks both ends of range simultaneously
static inline bool InStateZero(const UTF8ScanObj* st, const uint8* Tbl) {
  const uint8* Tbl0 = &st->state_table[st->state0];
  return (static_cast<uint32>(Tbl - Tbl0) < st->state0_size);
}


// Look up property of one UTF-8 character and advance over it
// Return 0 if input length is zero
// Return 0 and advance one byte if input is ill-formed
uint8 UTF8GenericProperty(const UTF8PropObj* st,
                          const uint8** src,
                          int* srclen) {
  if (*srclen <= 0) {
    return 0;
  }

  const uint8* lsrc = *src;
  const uint8* Tbl_0 = &st->state_table[st->state0];
  const uint8* Tbl = Tbl_0;
  int e;
  int eshift = st->entry_shift;

  // Short series of tests faster than switch, optimizes 7-bit ASCII
  unsigned char c = lsrc[0];
  if (static_cast<signed char>(c) >= 0) {           // one byte
    e = Tbl[c];
    *src += 1;
    *srclen -= 1;
  } else if (((c & 0xe0) == 0xc0) && (*srclen >= 2)) {     // two bytes
    e = Tbl[c];
    Tbl = &Tbl_0[e << eshift];
    e = Tbl[lsrc[1]];
    *src += 2;
    *srclen -= 2;
  } else if (((c & 0xf0) == 0xe0) && (*srclen >= 3)) {     // three bytes
    e = Tbl[c];
    Tbl = &Tbl_0[e << eshift];
    e = Tbl[lsrc[1]];
    Tbl = &Tbl_0[e << eshift];
    e = Tbl[lsrc[2]];
    *src += 3;
    *srclen -= 3;
  }else if (((c & 0xf8) == 0xf0) && (*srclen >= 4)) {     // four bytes
    e = Tbl[c];
    Tbl = &Tbl_0[e << eshift];
    e = Tbl[lsrc[1]];
    Tbl = &Tbl_0[e << eshift];
    e = Tbl[lsrc[2]];
    Tbl = &Tbl_0[e << eshift];
    e = Tbl[lsrc[3]];
    *src += 4;
    *srclen -= 4;
  } else {                                                // Ill-formed
    e = 0;
    *src += 1;
    *srclen -= 1;
  }
  return e;
}

// BigOneByte versions are needed for tables > 240 states, but most
// won't need the TwoByte versions.
// Internally, to next-to-last offset is multiplied by 16 and the last
// offset is relative instead of absolute.
// Look up property of one UTF-8 character and advance over it
// Return 0 if input length is zero
// Return 0 and advance one byte if input is ill-formed
uint8 UTF8GenericPropertyBigOneByte(const UTF8PropObj* st,
                          const uint8** src,
                          int* srclen) {
  if (*srclen <= 0) {
    return 0;
  }

  const uint8* lsrc = *src;
  const uint8* Tbl_0 = &st->state_table[st->state0];
  const uint8* Tbl = Tbl_0;
  int e;
  int eshift = st->entry_shift;

  // Short series of tests faster than switch, optimizes 7-bit ASCII
  unsigned char c = lsrc[0];
  if (static_cast<signed char>(c) >= 0) {           // one byte
    e = Tbl[c];
    *src += 1;
    *srclen -= 1;
  } else if (((c & 0xe0) == 0xc0) && (*srclen >= 2)) {     // two bytes
    e = Tbl[c];
    Tbl = &Tbl_0[e << eshift];
    e = Tbl[lsrc[1]];
    *src += 2;
    *srclen -= 2;
  } else if (((c & 0xf0) == 0xe0) && (*srclen >= 3)) {     // three bytes
    e = Tbl[c];
    Tbl = &Tbl_0[e << (eshift + 4)];  // 16x the range
    e = (reinterpret_cast<const int8*>(Tbl))[lsrc[1]];
    Tbl = &Tbl[e << eshift];          // Relative +/-
    e = Tbl[lsrc[2]];
    *src += 3;
    *srclen -= 3;
  }else if (((c & 0xf8) == 0xf0) && (*srclen >= 4)) {     // four bytes
    e = Tbl[c];
    Tbl = &Tbl_0[e << eshift];
    e = Tbl[lsrc[1]];
    Tbl = &Tbl_0[e << (eshift + 4)];  // 16x the range
    e = (reinterpret_cast<const int8*>(Tbl))[lsrc[2]];
    Tbl = &Tbl[e << eshift];          // Relative +/-
    e = Tbl[lsrc[3]];
    *src += 4;
    *srclen -= 4;
  } else {                                                // Ill-formed
    e = 0;
    *src += 1;
    *srclen -= 1;
  }
  return e;
}

// Scan a UTF-8 stringpiece based on a state table.
// Always scan complete UTF-8 characters
// Set number of bytes scanned. Return reason for exiting
int UTF8GenericScan(const UTF8ScanObj* st,
                    const uint8* str,
                    const int len,
                    int* bytes_consumed) {
  int eshift = st->entry_shift;        // 6 (space optimized) or 8
  // int nEntries = (1 << eshift);       // 64 or 256 entries per state

  const uint8* isrc = str;
    //reinterpret_cast<const uint8*>(str.data());
  const uint8* src = isrc;
  //const int len = str.length();
  const uint8* srclimit = isrc + len;
  const uint8* srclimit8 = srclimit - 7;
  *bytes_consumed = 0;
  if (len == 0) return kExitOK;

  const uint8* Tbl_0 = &st->state_table[st->state0];

DoAgain:
  // Do state-table scan
  int e = 0;
  uint8 c;

  // Do fast for groups of 8 identity bytes.
  // This covers a lot of 7-bit ASCII ~8x faster then the 1-byte loop,
  // including slowing slightly on cr/lf/ht
  //----------------------------
  const uint8* Tbl2 = &st->fast_state[0];
  uint32 losub = st->losub;
  uint32 hiadd = st->hiadd;
  while (src < srclimit8) {
    uint32 s0123 = (reinterpret_cast<const uint32 *>(src))[0];
    uint32 s4567 = (reinterpret_cast<const uint32 *>(src))[1];
    src += 8;
    // This is a fast range check for all bytes in [lowsub..0x80-hiadd)
    uint32 temp = (s0123 - losub) | (s0123 + hiadd) |
                  (s4567 - losub) | (s4567 + hiadd);
    if ((temp & 0x80808080) != 0) {
      // We typically end up here on cr/lf/ht; src was incremented
      int e0123 = (Tbl2[src[-8]] | Tbl2[src[-7]]) |
                  (Tbl2[src[-6]] | Tbl2[src[-5]]);
      if (e0123 != 0) {src -= 8; break;}    // Exit on Non-interchange
      e0123 = (Tbl2[src[-4]] | Tbl2[src[-3]]) |
              (Tbl2[src[-2]] | Tbl2[src[-1]]);
      if (e0123 != 0) {src -= 4; break;}    // Exit on Non-interchange
      // Else OK, go around again
    }
  }
  //----------------------------

  // Byte-at-a-time scan
  //----------------------------
  const uint8* Tbl = Tbl_0;
  while (src < srclimit) {
    c = *src;
    e = Tbl[c];
    src++;
    if (e >= kExitIllegalStructure) {break;}
    Tbl = &Tbl_0[e << eshift];
  }
  //----------------------------


  // Exit posibilities:
  //  Some exit code, !state0, back up over last char
  //  Some exit code, state0, back up one byte exactly
  //  source consumed, !state0, back up over partial char
  //  source consumed, state0, exit OK
  // For illegal byte in state0, avoid backup up over PREVIOUS char
  // For truncated last char, back up to beginning of it

  if (e >= kExitIllegalStructure) {
    // Back up over exactly one byte of rejected/illegal UTF-8 character
    src--;
    // Back up more if needed
    if (!InStateZero(st, Tbl)) {
      do {src--;} while ((src > isrc) && ((src[0] & 0xc0) == 0x80));
    }
  } else if (!InStateZero(st, Tbl)) {
    // Back up over truncated UTF-8 character
    e = kExitIllegalStructure;
    do {src--;} while ((src > isrc) && ((src[0] & 0xc0) == 0x80));
  } else {
    // Normal termination, source fully consumed
    e = kExitOK;
  }

  if (e == kExitDoAgain) {
    // Loop back up to the fast scan
    goto DoAgain;
  }

  *bytes_consumed = src - isrc;
  return e;
}
