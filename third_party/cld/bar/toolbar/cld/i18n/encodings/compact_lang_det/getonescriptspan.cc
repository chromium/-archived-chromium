// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/getonescriptspan.h"
#include <stdio.h>
#include <string.h>

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/lang_enc.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/utf8propjustletter.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/utf8propletterscriptnum.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/utf8scannotjustletterspecial.h"

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_basictypes.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_commandlineflags.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_google.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_htmlutils.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_unilib.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8statetable.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8utils.h"

static const Language GRAY_LANG = (Language)254;

static const int kMaxUpToWordBoundary = 50;       // span < this make longer,
                                                  // else make shorter
static const int kMaxAdvanceToWordBoundary = 10;  // +/- this many bytes
                                                  // to round to word boundary,
                                                  // direction above

static const char kSpecialSymbol[256] = {       // true for < > &
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,1,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,1,0,1,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,

  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
};



#define LT 0      // <
#define GT 1      // >
#define EX 2      // !
#define HY 3      // -
#define QU 4      // "
#define AP 5      // '
#define SL 6      // /
#define S_ 7
#define C_ 8
#define R_ 9
#define I_ 10
#define P_ 11
#define T_ 12
#define Y_ 13
#define L_ 14
#define E_ 15
#define CR 16     // <cr> or <lf>
#define NL 17     // non-letter: ASCII whitespace, digit, punctuation
#define PL 18     // possible letter, incl. &
#define xx 19     // <unused>

// Map byte to one of ~20 interesting categories for cheap tag parsing
static const uint8 kCharToSub[256] = {
  NL,NL,NL,NL, NL,NL,NL,NL, NL,NL,CR,NL, NL,CR,NL,NL,
  NL,NL,NL,NL, NL,NL,NL,NL, NL,NL,NL,NL, NL,NL,NL,NL,
  NL,EX,QU,NL, NL,NL,PL,AP, NL,NL,NL,NL, NL,HY,NL,SL,
  NL,NL,NL,NL, NL,NL,NL,NL, NL,NL,NL,NL, LT,NL,GT,NL,

  PL,PL,PL,C_, PL,E_,PL,PL, PL,I_,PL,PL, L_,PL,PL,PL,
  P_,PL,R_,S_, T_,PL,PL,PL, PL,Y_,PL,NL, NL,NL,NL,NL,
  PL,PL,PL,C_, PL,E_,PL,PL, PL,I_,PL,PL, L_,PL,PL,PL,
  P_,PL,R_,S_, T_,PL,PL,PL, PL,Y_,PL,NL, NL,NL,NL,NL,

  NL,NL,NL,NL, NL,NL,NL,NL, NL,NL,NL,NL, NL,NL,NL,NL,
  NL,NL,NL,NL, NL,NL,NL,NL, NL,NL,NL,NL, NL,NL,NL,NL,
  NL,NL,NL,NL, NL,NL,NL,NL, NL,NL,NL,NL, NL,NL,NL,NL,
  NL,NL,NL,NL, NL,NL,NL,NL, NL,NL,NL,NL, NL,NL,NL,NL,

  PL,PL,PL,PL, PL,PL,PL,PL, PL,PL,PL,PL, PL,PL,PL,PL,
  PL,PL,PL,PL, PL,PL,PL,PL, PL,PL,PL,PL, PL,PL,PL,PL,
  PL,PL,PL,PL, PL,PL,PL,PL, PL,PL,PL,PL, PL,PL,PL,PL,
  PL,PL,PL,PL, PL,PL,PL,PL, PL,PL,PL,PL, PL,PL,PL,PL,
};

#undef LT
#undef GT
#undef EX
#undef HY
#undef QU
#undef AP
#undef SL
#undef S_
#undef C_
#undef R_
#undef I_
#undef P_
#undef T_
#undef Y_
#undef L_
#undef E_
#undef CR
#undef NL
#undef PL
#undef xx


#define OK 0
#define X_ 1

// State machine to do cheap parse of non-letter strings incl. tags
// advances <tag>
//          |    |
// advances <tag> ... </tag>  for <script> <style>
//          |               |
// advances <!-- ... <tag> ... -->
//          |                     |
// advances <tag
//          ||  (0)
// advances <tag <tag2>
//          ||  (0)
static const uint8 kTagParseTbl_0[] = {
// <  >  !  -   "  '  /  S   C  R  I  P   T  Y  L  E  CR NL PL xx
   3, 2, 2, 2,  2, 2, 2,OK, OK,OK,OK,OK, OK,OK,OK,OK,  2, 2,OK,X_, // [0] OK
  X_,X_,X_,X_, X_,X_,X_,X_, X_,X_,X_,X_, X_,X_,X_,X_, X_,X_,X_,X_, // [1] error
   3, 2, 2, 2,  2, 2, 2,OK, OK,OK,OK,OK, OK,OK,OK,OK,  2, 2,OK,X_, // [2] NL*
  X_, 2, 4, 9, 10,11, 9,13,  9, 9, 9, 9,  9, 9, 9, 9,  9, 9, 9,X_, // [3] <
  X_, 2, 9, 5, 10,11, 9, 9,  9, 9, 9, 9,  9, 9, 9, 9,  9, 9, 9,X_, // [4] <!
  X_, 2, 9, 6, 10,11, 9, 9,  9, 9, 9, 9,  9, 9, 9, 9,  9, 9, 9,X_, // [5] <!-
   6, 6, 6, 7,  6, 6, 6, 6,  6, 6, 6, 6,  6, 6, 6, 6,  6, 6, 6,X_, // [6] <!--.*
   6, 6, 6, 8,  6, 6, 6, 6,  6, 6, 6, 6,  6, 6, 6, 6,  6, 6, 6,X_, // [7] <!--.*-
   6, 2, 6, 8,  6, 6, 6, 6,  6, 6, 6, 6,  6, 6, 6, 6,  6, 6, 6,X_, // [8] <!--.*--
  X_, 2, 9, 9, 10,11, 9, 9,  9, 9, 9, 9,  9, 9, 9, 9,  9, 9, 9,X_, // [9] <.*
  10,10,10,10,  9,10,10,10, 10,10,10,10, 10,10,10,10, 12,10,10,X_, // [10] <.*"
  11,11,11,11, 11, 9,11,11, 11,11,11,11, 11,11,11,11, 12,11,11,X_, // [11] <.*'
  X_, 2,12,12, 12,12,12,12, 12,12,12,12, 12,12,12,12, 12,12,12,X_, // [12] <.* no " '

// <  >  !  -   "  '  /  S   C  R  I  P   T  Y  L  E  CR NL PL xx
  X_, 2, 9, 9, 10,11, 9, 9, 14, 9, 9, 9, 28, 9, 9, 9,  9, 9, 9,X_, // [13] <S
  X_, 2, 9, 9, 10,11, 9, 9,  9,15, 9, 9,  9, 9, 9, 9,  9, 9, 9,X_, // [14] <SC
  X_, 2, 9, 9, 10,11, 9, 9,  9, 9,16, 9,  9, 9, 9, 9,  9, 9, 9,X_, // [15] <SCR
  X_, 2, 9, 9, 10,11, 9, 9,  9, 9, 9,17,  9, 9, 9, 9,  9, 9, 9,X_, // [16] <SCRI
  X_, 2, 9, 9, 10,11, 9, 9,  9, 9, 9, 9, 18, 9, 9, 9,  9, 9, 9,X_, // [17] <SCRIP
  X_,19, 9, 9, 10,11, 9, 9,  9, 9, 9, 9,  9, 9, 9, 9, 19,19, 9,X_, // [18] <SCRIPT
  20,19,19,19, 19,19,19,19, 19,19,19,19, 19,19,19,19, 19,19,19,X_, // [19] <SCRIPT .*
  19,19,19,19, 19,19,21,19, 19,19,19,19, 19,19,19,19, 19,19,19,X_, // [20] <SCRIPT .*<
  19,19,19,19, 19,19,19,22, 19,19,19,19, 19,19,19,19, 19,19,19,X_, // [21] <SCRIPT .*</
  19,19,19,19, 19,19,19,19, 23,19,19,19, 19,19,19,19, 19,19,19,X_, // [22] <SCRIPT .*</S
  19,19,19,19, 19,19,19,19, 19,24,19,19, 19,19,19,19, 19,19,19,X_, // [23] <SCRIPT .*</SC
  19,19,19,19, 19,19,19,19, 19,19,25,19, 19,19,19,19, 19,19,19,X_, // [24] <SCRIPT .*</SCR
  19,19,19,19, 19,19,19,19, 19,19,19,26, 19,19,19,19, 19,19,19,X_, // [25] <SCRIPT .*</SCRI
  19,19,19,19, 19,19,19,19, 19,19,19,19, 27,19,19,19, 19,19,19,X_, // [26] <SCRIPT .*</SCRIP
  19, 2,19,19, 19,19,19,19, 19,19,19,19, 19,19,19,19, 19,19,19,X_, // [27] <SCRIPT .*</SCRIPT

// <  >  !  -   "  '  /  S   C  R  I  P   T  Y  L  E  CR NL PL xx
  X_, 2, 9, 9, 10,11, 9, 9,  9, 9, 9, 9,  9,29, 9, 9,  9, 9, 9,X_, // [28] <ST
  X_, 2, 9, 9, 10,11, 9, 9,  9, 9, 9, 9,  9, 9,30, 9,  9, 9, 9,X_, // [29] <STY
  X_, 2, 9, 9, 10,11, 9, 9,  9, 9, 9, 9,  9, 9, 9,31,  9, 9, 9,X_, // [30] <STYL
  X_,32, 9, 9, 10,11, 9, 9,  9, 9, 9, 9,  9, 9, 9, 9, 32,32, 9,X_, // [31] <STYLE
  33,32,32,32, 32,32,32,32, 32,32,32,32, 32,32,32,32, 32,32,32,X_, // [32] <STYLE .*
  32,32,32,32, 32,32,34,32, 32,32,32,32, 32,32,32,32, 32,32,32,X_, // [33] <STYLE .*<
  32,32,32,32, 32,32,32,35, 32,32,32,32, 32,32,32,32, 32,32,32,X_, // [34] <STYLE .*</
  32,32,32,32, 32,32,32,32, 32,32,32,32, 36,32,32,32, 32,32,32,X_, // [35] <STYLE .*</S
  32,32,32,32, 32,32,32,32, 32,32,32,32, 32,37,32,32, 32,32,32,X_, // [36] <STYLE .*</ST
  32,32,32,32, 32,32,32,32, 32,32,32,32, 32,32,38,32, 32,32,32,X_, // [37] <STYLE .*</STY
  32,32,32,32, 32,32,32,32, 32,32,32,32, 32,32,32,39, 32,32,32,X_, // [38] <STYLE .*</STYL
  32, 2,32,32, 32,32,32,32, 32,32,32,32, 32,32,32,32, 32,32,32,X_, // [39] <STYLE .*</STYLE
};

#undef OK
#undef X_


/*
// Convert GetTimeOfDay output to 64-bit usec
static inline uint64 Microseconds(const struct timeval& t) {
  // The SumReducer uses uint64, so convert to (uint64) microseconds,
  // not (double) seconds.
  return t.tv_sec * 1000000ULL + t.tv_usec;
}
*/


// Returns true if character is < > or &
bool inline IsSpecial(char c) {
  if ((c & 0xe0) == 0x20) {
    return kSpecialSymbol[static_cast<uint8>(c)];
  }
  return false;
}

// Quick Skip to next letter or < > & or to end of string (eos)
// Always return is_letter for eos
int ScanToLetterOrSpecial(const char* src, int len) {
  int bytes_consumed;
  cld::UTF8GenericScan(&utf8scannotjustletterspecial_obj, src, len,
                       &bytes_consumed);
  return bytes_consumed;
}



// src points to non-letter, such as tag-opening '<'
// Return length from here to next possible letter
// On eos or another < before >, return 1
// advances <tag>
//          |    |
// advances <tag> ... </tag>  for <script> <style>
//          |               |
// advances <!-- ... <tag> ... -->
//          |                     |
// advances <tag
//          ||  (1)
// advances <tag <tag2>
//          ||  (1)
int ScanToPossibleLetter(const char* isrc, int len) {
  const uint8* src = reinterpret_cast<const uint8*>(isrc);
  const uint8* srclimit = src + len;
  const uint8* tagParseTbl = kTagParseTbl_0;
  int e = 0;
  while (src < srclimit) {
    e = tagParseTbl[kCharToSub[*src++]];
    if ((e & ~1) == 0) {
      // We overshot by one byte
      --src;
      break;
    }
    tagParseTbl = &kTagParseTbl_0[e * 20];
  }

  if (src >= srclimit) {
    // We fell off the end of the text.
    // It looks like the most common case for this is a truncated file, not
    // mismatched angle brackets. So we pretend that the last char was '>'
    return len;
  }

  // OK to be in state 0 or state 2 at exit
  if ((e != 0) && (e != 2)) {
    // Error, '<' followed by '<'
    // We want to back up to first <, then advance by one byte past it
    int offset = src - reinterpret_cast<const uint8*>(isrc);
    // printf("ScanToPossibleLetter error at %d[%d] in '%s'\n",offset,e,isrc);

    // Backscan to first '<' and return enough length to just get past it
    --offset;   // back up over the second '<', which caused us to stop
    while ((0 < offset) && (isrc[offset] != '<')) {
      // Find the first '<', which is unmatched
      --offset;
    }
    // skip to just beyond first '<'
    // printf("  returning %d\n", offset + 1);
    return offset + 1;
  }

  return src - reinterpret_cast<const uint8*>(isrc);
}



ScriptScanner::ScriptScanner(const char* buffer,
                             int buffer_length,
                             bool is_plain_text)
  : start_byte_(buffer),
  next_byte_(buffer),
  next_byte_limit_(buffer + buffer_length),
  byte_length_(buffer_length),
  is_plain_text_(is_plain_text) {
    script_buffer_ = new char[getone::kMaxScriptBuffer];
    script_buffer_lower_ = new char[getone::kMaxScriptLowerBuffer];
}

ScriptScanner::~ScriptScanner() {
  delete[] script_buffer_;
  delete[] script_buffer_lower_;
}




// Get to the first real non-tag letter or entity that is a letter
// Sets script of that letter
// Return len if no more letters
int ScriptScanner::SkipToFrontOfSpan(const char* src, int len, int* script) {
  int sc = UNKNOWN_LSCRIPT;
  int skip = 0;
  int tlen, plen;

  // Do run of non-letters (tag | &NL | NL)*
  while (skip < len) {
    // Do fast scan to next interesting byte
    // int oldskip = skip;
    skip += ScanToLetterOrSpecial(src + skip, len - skip);
    // TEMP
    // printf("ScanToLetterOrSpecial[%d] 0x%02x => [%d] 0x%02x\n",
    //       oldskip, src[oldskip], skip, src[skip]);

    // Check for no more letters/specials
    if (skip >= len) {
      // All done
      return len;
    }

    // We are at a letter, nonletter, tag, or entity
    if (IsSpecial(src[skip]) && !is_plain_text_) {
      if (src[skip] == '<') {
        // Begining of tag; skip to end and go around again
        tlen = ScanToPossibleLetter(src + skip, len - skip);
        sc = 0;
        // printf("<...> ");
      } else if (src[skip] == '>') {
        // Unexpected end of tag; skip it and go around again
        tlen = 1;         // Over the >
        sc = 0;
        // printf("..> ");
      } else if (src[skip] == '&') {
        // Expand entity, no advance
        char temp[4];
        EntityToBuffer(src + skip, len - skip,
                       temp, &tlen, &plen);
        sc = getone::GetUTF8LetterScriptNum(temp);
        // printf("#(%02x%02x)=%d ", temp[0], temp[1], sc);
      }
    } else {
      // Update 1..4 bytes
      tlen = cld_UniLib::OneCharLen(src + skip);
      sc = getone::GetUTF8LetterScriptNum(src + skip);
      // printf("#(%02x%02x)=%d ", src[skip], src[skip+1], sc);
    }
    // TEMP
    // printf("sc=%d ", sc);
    if (sc != 0) {break;}           // Letter found
    skip += tlen;                   // Advance
  }

  *script = sc;
  return skip;
}



// Copy next run of same-script non-tag letters to buffer [NUL terminated]
// Buffer has leading space and all text is lowercased
bool ScriptScanner::GetOneScriptSpan(getone::LangSpan* span) {
  span->text = script_buffer_;
  span->text_bytes = 0;
  span->offset = next_byte_ - start_byte_;
  span->script = UNKNOWN_LSCRIPT;
  span->lang = UNKNOWN_LANGUAGE;
  span->truncated = false;

  // printf("GetOneScriptSpan[[ ");
  // struct timeval script_start, script_mid, script_end;

  int spanscript;           // The script of this span
  int sc = UNKNOWN_LSCRIPT;  // The script of next character
  int tlen, plen;


  script_buffer_[0] = ' ';  // Always a space at front of output
  script_buffer_[1] = '\0';
  int take = 0;
  int put = 1;              // Start after the initial space

  // gettimeofday(&script_start, NULL);
  // Get to the first real non-tag letter or entity that is a letter
  int skip = SkipToFrontOfSpan(next_byte_, byte_length_, &spanscript);
  next_byte_ += skip;
  byte_length_ -= skip;
  if (byte_length_ <= 0) {
    // printf("]]\n");
    return false;               // No more letters to be found
  }

  // gettimeofday(&script_mid, NULL);

  // There is at least one letter, so we know the script for this span
  // printf("{%d} ", spanscript);
  span->script = (UnicodeLScript)spanscript;


  // Go over alternating spans of same-script letters and non-letters,
  // copying letters to buffer with single spaces for each run of non-letters
  while (take < byte_length_) {
    // Copy run of letters in same script (&LS | LS)*
    int letter_count = 0;              // Keep track of word length
    bool need_break = false;
    while (take < byte_length_) {
      // We are at a letter, nonletter, tag, or entity
      if (IsSpecial(next_byte_[take]) && !is_plain_text_) {
        // printf("\"%c\" ", next_byte_[take]);
        if (next_byte_[take] == '<') {
          // Begining of tag
          sc = 0;
          break;
        } else if (next_byte_[take] == '>') {
          // Unexpected end of tag
          sc = 0;
          break;
        } else if (next_byte_[take] == '&') {
          // Copy entity, no advance
          EntityToBuffer(next_byte_ + take, byte_length_ - take,
                         script_buffer_ + put, &tlen, &plen);
          sc = getone::GetUTF8LetterScriptNum(script_buffer_ + put);
        }
      } else {
        // Real letter, safely copy up to 4 bytes, increment by 1..4
        // Will update by 1..4 bytes at Advance, below
        tlen = plen = cld_UniLib::OneCharLen(next_byte_ + take);
        if (take < (byte_length_ - 3)) {
          // Fast case
          *reinterpret_cast<uint32*>(script_buffer_ + put) =
            *reinterpret_cast<const uint32*>(next_byte_ + take);
        } else {
          // Slow case, happens 1-3 times per input document
          memcpy(script_buffer_ + put, next_byte_ + take, plen);
        }
        sc = getone::GetUTF8LetterScriptNum(next_byte_ + take);
      }
      // printf("sc(%c)=%d ", next_byte_[take], sc);
      // char xtmp[8]; memcpy(xtmp,script_buffer_ + put, plen);
      // xtmp[plen] = '\0'; printf("'%s'{%d} ", xtmp, sc);

      // Allow continue across a single letter in a different script:
      // A B D = three scripts, c = common script, i = inherited script,
      // - = don't care, ( = take position before the += below
      //  AAA(A-    continue
      //
      //  AAA(BA    continue
      //  AAA(BB    break
      //  AAA(Bc    continue (breaks after B)
      //  AAA(BD    break
      //  AAA(Bi    break
      //
      //  AAA(c-    break
      //
      //  AAA(i-    continue
      //

      if ((sc != spanscript) && (sc != ULScript_Inherited)) {
        // Might need to break this script span
        if (sc == ULScript_Common) {
          need_break = true;
        } else {
          // Look at next following character, ignoring entity as Common
          int sc2 = getone::GetUTF8LetterScriptNum(next_byte_ + take + tlen);
          if ((sc2 != ULScript_Common) && (sc2 != spanscript)) {
            need_break = true;
          }
        }
      }
      if (need_break) {break;}  // Non-letter or letter in wrong script

      take += tlen;                   // Advance
      put += plen;                    // Advance
      ++letter_count;
      if (put >= getone::kMaxScriptBytes) {
        // Buffer is full
        span->truncated = true;
        break;
      }
    }     // End while letters

    // Do run of non-letters (tag | &NL | NL)*
    while (take < byte_length_) {
      // Do fast scan to next interesting byte
      take += ScanToLetterOrSpecial(next_byte_ + take, byte_length_ - take);
      
      // Check for no more letters/specials
      if (take >= byte_length_) {
        take = byte_length_;
        break;
      }

      // We are at a letter, nonletter, tag, or entity
      if (IsSpecial(next_byte_[take]) && !is_plain_text_) {
        // printf("\"%c\" ", next_byte_[take]);
        if (next_byte_[take] == '<') {
          // Begining of tag; skip to end and go around again
          tlen = ScanToPossibleLetter(next_byte_ + take, byte_length_ - take);
          sc = 0;
          // printf("<...> ");
        } else if (next_byte_[take] == '>') {
          // Unexpected end of tag; skip it and go around again
          tlen = 1;         // Over the >
          sc = 0;
          // printf("..> ");
        } else if (next_byte_[take] == '&') {
          // Expand entity, no advance
          EntityToBuffer(next_byte_ + take, byte_length_ - take,
                         script_buffer_ + put, &tlen, &plen);
          sc = getone::GetUTF8LetterScriptNum(script_buffer_ + put);
        }
      } else {
        // Update 1..4
        tlen = cld_UniLib::OneCharLen(next_byte_ + take);
        sc = getone::GetUTF8LetterScriptNum(next_byte_ + take);
      }
      // printf("sc[%c]=%d ", next_byte_[take], sc);
      if (sc != 0) {break;}           // Letter found
      take += tlen;                   // Advance
    }     // End while not-letters

    script_buffer_[put++] = ' ';

    // We are at a letter again (or eos), after letter* not-letter*
    if (sc != spanscript) {break;}            // Letter in wrong script
    if (put >= getone::kMaxScriptBytes - 8) {
      // Buffer is almost full
      span->truncated = true;
      break;
    }
  }

  // Update input position
  next_byte_ += take;
  byte_length_ -= take;

  // Put four more spaces/NUL. Worst case is abcd _ _ _ \0
  //                          kMaxScriptBytes |   | put
  script_buffer_[put + 0] = ' ';
  script_buffer_[put + 1] = ' ';
  script_buffer_[put + 2] = ' ';
  script_buffer_[put + 3] = '\0';

  span->text_bytes = put;       // Does not include the last four chars above

  // printf(" %d]]\n\n", put);
  return true;
}

// Force Latin, Cyrillic, Greek scripts to be lowercase
void ScriptScanner::LowerScriptSpan(getone::LangSpan* span) {
  // On Windows, text is lowercased beforehand, so no need to do anything here.
#if !defined(CLD_WINDOWS)
  // If needed, lowercase all the text. If we do it sooner, might miss
  // lowercasing an entity such as &Aacute;
  // We only need to do this for Latn and Cyrl scripts
  if ((span->script == ULScript_Latin) ||
      (span->script == ULScript_Cyrillic) ||
      (span->script == ULScript_Greek)) {
    // Full Unicode lowercase of the entire buffer, including
    // four pad bytes off the end
    int consumed, filled;
    UniLib::ToLower(span->text, span->text_bytes + 4,
                    script_buffer_lower_, getone::kMaxScriptLowerBuffer,
                    &consumed, &filled);
    span->text = script_buffer_lower_;
    span->text_bytes = filled - 4;
  }
#endif
}

// Copy next run of same-script non-tag letters to buffer [NUL terminated]
// Force Latin and Cyrillic scripts to be lowercase
bool ScriptScanner::GetOneScriptSpanLower(getone::LangSpan* span) {
  bool ok = GetOneScriptSpan(span);
  LowerScriptSpan(span);
  return ok;
}

// Gets lscript number for letters; always returns
//   0 (common script) for non-letters
int getone::GetUTF8LetterScriptNum(const char* src) {
  int srclen = cld_UniLib::OneCharLen(src);
  const uint8* usrc = reinterpret_cast<const uint8*>(src);
  return UTF8GenericProperty(&utf8propletterscriptnum_obj, &usrc, &srclen);
}
