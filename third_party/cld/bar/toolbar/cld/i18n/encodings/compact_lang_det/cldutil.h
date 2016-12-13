// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef I18N_ENCODINGS_COMPACT_LANG_DET_CLDUTIL_H_
#define I18N_ENCODINGS_COMPACT_LANG_DET_CLDUTIL_H_

#include <string>
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_macros.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/ext_lang_enc.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/tote.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_basictypes.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_commandlineflags.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8statetable.h"

namespace cld {

  // Hash bucket for four-way associative lookup with < 64K buckets
  // 32 bytes per bucket, 8-byte entries
  typedef struct {
    uint32 key[4];        // hashed word to look up
    uint32 value[4];      // packed three lang numbers and probability subscript
  } SmallWordProbBucket4;

  // Hash bucket for fouro-way associative lookup with >= 64K buckets
  // 24 bytes per bucket, 6-byte entries
  typedef struct {
    uint16 key[4];        // Half of hashed word to look up; other
                          //  half is used to pick the bucket
    uint32 value[4];      // packed three lang numbers and probability subscript
  } LargeQuadProbBucket4;

  // Hash bucket for four-way associative lookup, indirect probabilities
  // 16 bytes per bucket, 4-byte entries
  typedef struct {
    uint32 keyvalue[4];   // Upper part of word is hash, lower is indirect prob
  } IndirectProbBucket4;


  // This describes a complete CLD table, consisting of
  // a main lookup table, an indirect language/probability table, and
  // three constants.
  // The main table key is a quadgram, bigram, or longword hash, with
  // part of the key used to select a bucket modulo kCLDTableSize,
  // and the rest matched against the key portion of four entries in a bucket,
  // defined by kCLDTableKeyMask. The remaining bits of an entry, defined
  // by ~kCLDTableKeyMask, are usually a subscript in the indirect table.
  //
  // By using part of the key to select a bucket, those key bits do not need
  // to be stored in the main table entries, saving space (typically 2 bytes).
  //
  // By using an indirect table for lang/prob triples, only the subscript needs
  // to be stored in the main table entires, saving space (typically 2 bytes).
  //
  // Each entry in the indirect table has three languages and three
  // corresponding probabilities, packed into four bytes.
  //
  // The build date constant is included just for version tracking and is not
  // otherwise used.
  //
  // Different-size tables can be linked in for different production
  // environments. By going indirect through this struct, the runtime code is
  // insensitive to the actual sizes.
  //
  // An empty placeholder table can be described by a table size of 1
  // bucket, a keymask of 0xffffffff, a degenerate bucket of four no-match
  // entries, and a degenerate indirect table of one no-languages entry.
  //
  //
  struct CLDTableSummary {
    const IndirectProbBucket4* kCLDTable;
                                        // Each bucket has four entries, part
                                        //  key and part indirect subscript
    const uint32* kCLDTableInd;         // Each entry is three packed lang/prob
    const int kCLDTableSize;            // Bucket count
    const int kCLDTableIndSize;         // Entries count
    const int kCLDTableKeyMask;         // Mask hash key
    const int kCLDTableBuildDate;       // yyyymmdd
  };


  // Keeps per-character 0-12 language probabilities for CTJKVZ-- in that order.
  // Chinese ChineseT Japanese Korean Vietnamese Zhuang
  // (2 bytes unused, for alignment padding and future)
  typedef struct {
    uint8 probs[8];
  } UnigramProbArray;

  // Map 8-bit subscript to CTJKVZ probabilities
  // Target runtime probabilities for CTJK + VZ
  // Hand-generated to cover a reasonable range of choices
  static const int kTargetCTJKVZProbsSize = 242;
  static const UnigramProbArray kTargetCTJKVZProbs[kTargetCTJKVZProbsSize] = {
    {{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,12,0,0}},
    {{0,0,0,0,12,0,0,0}},
    {{0,0,0,12,0,0,0,0}},
    {{0,0,12,0,0,0,0,0}},
    {{0,12,0,0,0,0,0,0}},
    {{12,0,0,0,0,0,0,0}},

    {{8,0,0,0,4,0,0,0}},
    {{8,0,0,4,0,0,0,0}},
    {{8,0,4,0,0,0,0,0}},
    {{8,4,0,0,0,0,0,0}},
    {{8,2,0,2,0,0,0,0}},
    {{0,0,0,0,0,8,0,0}},
    {{0,4,8,0,0,0,0,0}},
    {{4,0,0,0,0,8,0,0}},
    {{0,0,8,0,0,0,0,0}},
    {{8,2,2,0,0,0,0,0}},
    {{0,8,4,0,0,0,0,0}},
    {{8,0,0,0,0,4,0,0}},
    {{0,8,2,0,0,0,0,0}},
    {{4,8,0,0,0,0,0,0}},
    {{2,8,0,2,0,0,0,0}},
    {{2,2,8,0,0,0,0,0}},
    {{0,8,0,0,0,0,0,0}},
    {{0,2,8,0,0,0,0,0}},
    {{2,8,2,0,0,0,0,0}},
    {{8,0,0,0,0,0,0,0}},
    {{2,8,0,0,0,0,0,0}},
    {{8,2,0,0,0,0,0,0}},

    {{0,6,2,0,2,0,0,0}},
    {{2,0,0,0,6,0,0,0}},
    {{4,0,0,0,6,0,0,0}},
    {{4,6,0,0,4,0,0,0}},
    {{4,6,2,0,2,0,0,0}},
    {{4,6,4,0,2,0,0,0}},
    {{5,4,6,0,0,0,0,0}},
    {{6,0,0,0,4,0,0,0}},
    {{6,0,2,0,4,0,0,0}},
    {{6,0,4,0,4,0,0,0}},
    {{6,2,0,0,4,0,0,0}},
    {{6,2,2,0,4,0,0,0}},
    {{6,2,4,0,2,0,0,0}},
    {{6,4,0,0,2,0,0,0}},
    {{6,4,2,0,2,0,0,0}},
    {{0,0,6,2,0,0,0,0}},
    {{0,6,2,0,0,2,0,0}},
    {{2,2,2,0,0,6,0,0}},
    {{2,2,6,4,0,0,0,0}},
    {{2,4,0,0,0,6,0,0}},
    {{2,6,0,4,0,0,0,0}},
    {{2,6,2,4,0,0,0,0}},
    {{2,6,4,4,0,0,0,0}},
    {{4,0,2,0,0,6,0,0}},
    {{4,2,6,2,0,0,0,0}},
    {{4,4,2,0,0,6,0,0}},
    {{4,6,4,0,0,2,0,0}},
    {{6,0,2,0,0,2,0,0}},
    {{6,2,0,0,0,2,0,0}},
    {{6,2,2,0,0,4,0,0}},
    {{6,2,4,0,0,2,0,0}},
    {{4,6,2,0,0,4,0,0}},
    {{6,4,2,0,0,4,0,0}},
    {{2,0,0,0,0,6,0,0}},
    {{6,2,0,2,0,0,0,0}},
    {{2,2,0,0,0,6,0,0}},
    {{6,2,6,0,0,0,0,0}},
    {{6,4,2,0,0,2,0,0}},
    {{6,4,2,2,0,0,0,0}},
    {{4,6,4,2,0,0,0,0}},
    {{6,0,2,0,0,4,0,0}},
    {{6,0,4,0,0,2,0,0}},
    {{6,0,6,0,0,0,0,0}},
    {{6,2,2,0,0,0,0,0}},
    {{6,4,0,0,0,2,0,0}},
    {{6,4,5,0,0,0,0,0}},
    {{0,6,0,2,0,0,0,0}},
    {{0,6,2,2,0,0,0,0}},
    {{2,6,0,2,0,0,0,0}},
    {{2,6,2,2,0,0,0,0}},
    {{4,2,0,0,0,6,0,0}},
    {{6,4,0,0,0,4,0,0}},
    {{6,4,0,2,0,0,0,0}},
    {{6,6,0,2,0,0,0,0}},
    {{6,0,4,0,0,4,0,0}},
    {{6,2,0,0,0,4,0,0}},
    {{6,6,2,2,0,0,0,0}},
    {{4,6,0,0,0,2,0,0}},
    {{2,6,6,0,0,0,0,0}},
    {{4,5,6,0,0,0,0,0}},
    {{4,6,0,2,0,0,0,0}},
    {{6,2,0,0,0,6,0,0}},
    {{0,6,4,2,0,0,0,0}},
    {{4,0,6,0,0,0,0,0}},
    {{2,6,4,2,0,0,0,0}},
    {{4,6,0,0,0,4,0,0}},
    {{6,2,2,0,0,0,0,0}},
    {{4,6,2,2,0,0,0,0}},
    {{4,6,5,0,0,0,0,0}},
    {{6,0,2,0,0,0,0,0}},
    {{6,4,4,0,0,0,0,0}},
    {{4,2,6,0,0,0,0,0}},
    {{2,0,6,0,0,0,0,0}},
    {{4,4,0,0,0,6,0,0}},
    {{4,4,6,0,0,0,0,0}},
    {{4,6,2,0,0,2,0,0}},
    {{2,2,6,0,0,0,0,0}},
    {{2,4,6,0,0,0,0,0}},
    {{0,6,6,0,0,0,0,0}},
    {{6,2,4,0,0,0,0,0}},
    {{0,4,6,0,0,0,0,0}},
    {{4,0,0,0,0,6,0,0}},
    {{4,6,4,0,0,0,0,0}},
    {{6,0,0,0,0,6,0,0}},
    {{6,0,0,0,0,2,0,0}},
    {{6,0,4,0,0,0,0,0}},
    {{6,5,4,0,0,0,0,0}},
    {{0,2,6,0,0,0,0,0}},
    {{0,0,6,0,0,0,0,0}},
    {{6,6,2,0,0,0,0,0}},
    {{2,6,4,0,0,0,0,0}},
    {{6,4,2,0,0,0,0,0}},
    {{2,6,2,0,0,0,0,0}},
    {{2,6,0,0,0,0,0,0}},
    {{6,0,0,0,0,4,0,0}},
    {{6,4,0,0,0,0,0,0}},
    {{6,6,0,0,0,0,0,0}},
    {{5,6,4,0,0,0,0,0}},
    {{0,6,0,0,0,0,0,0}},
    {{6,2,0,0,0,0,0,0}},
    {{0,6,2,0,0,0,0,0}},
    {{4,6,2,0,0,0,0,0}},
    {{0,6,4,0,0,0,0,0}},
    {{4,6,0,0,0,0,0,0}},
    {{6,0,0,0,0,0,0,0}},
    {{6,6,5,0,0,0,0,0}},
    {{6,5,6,0,0,0,0,0}},
    {{5,6,6,0,0,0,0,0}},
    {{5,5,6,0,0,0,0,0}},
    {{5,6,5,0,0,0,0,0}},
    {{6,5,5,0,0,0,0,0}},
    {{6,6,6,0,0,0,0,0}},
    {{6,5,0,0,0,0,0,0}},
    {{6,0,5,0,0,0,0,0}},
    {{0,6,5,0,0,0,0,0}},
    {{5,6,0,0,0,0,0,0}},
    {{5,0,6,0,0,0,0,0}},
    {{0,5,6,0,0,0,0,0}},

    {{0,0,0,0,4,0,0,0}},
    {{0,0,0,4,0,0,0,0}},
    {{2,2,0,0,4,0,0,0}},
    {{2,2,2,0,4,0,0,0}},
    {{2,4,0,0,2,0,0,0}},
    {{2,4,2,0,2,0,0,0}},
    {{2,4,4,0,2,0,0,0}},
    {{4,0,2,0,4,0,0,0}},
    {{4,0,4,0,2,0,0,0}},
    {{4,2,0,0,2,0,0,0}},
    {{4,2,2,0,2,0,0,0}},
    {{4,4,0,0,2,0,0,0}},
    {{4,4,2,0,2,0,0,0}},
    {{4,4,4,0,2,0,0,0}},
    {{0,2,2,4,0,0,0,0}},
    {{2,2,4,2,0,0,0,0}},
    {{2,4,4,0,0,2,0,0}},
    {{2,4,4,2,0,0,0,0}},
    {{4,0,4,0,0,2,0,0}},
    {{4,0,4,0,0,4,0,0}},
    {{4,2,2,4,0,0,0,0}},
    {{4,4,0,2,0,0,0,0}},
    {{2,2,0,4,0,0,0,0}},
    {{2,4,2,2,0,0,0,0}},
    {{4,4,2,2,0,0,0,0}},
    {{4,0,4,0,0,0,0,0}},
    {{4,4,4,0,0,4,0,0}},
    {{0,4,0,2,0,0,0,0}},
    {{0,4,2,2,0,0,0,0}},
    {{4,0,2,0,0,2,0,0}},
    {{4,2,0,0,0,4,0,0}},
    {{2,2,2,0,0,4,0,0}},
    {{4,0,0,2,0,0,0,0}},
    {{4,4,4,0,0,2,0,0}},
    {{4,0,0,0,0,4,0,0}},
    {{4,0,2,0,0,4,0,0}},
    {{4,2,0,0,0,2,0,0}},
    {{4,2,2,0,0,2,0,0}},
    {{2,4,0,2,0,0,0,0}},
    {{2,2,0,0,0,4,0,0}},
    {{2,4,0,0,0,4,0,0}},
    {{2,4,2,0,0,4,0,0}},
    {{4,2,4,0,0,0,0,0}},
    {{2,0,4,0,0,0,0,0}},
    {{4,0,2,0,0,0,0,0}},
    {{4,4,0,0,0,4,0,0}},
    {{4,4,2,0,0,4,0,0}},
    {{0,4,4,0,0,0,0,0}},
    {{4,4,0,0,0,2,0,0}},
    {{2,4,0,0,0,2,0,0}},
    {{2,2,4,0,0,0,0,0}},
    {{0,2,4,0,0,0,0,0}},
    {{4,2,2,0,0,0,0,0}},
    {{2,4,2,0,0,2,0,0}},
    {{4,4,4,0,0,0,0,0}},
    {{2,4,4,0,0,0,0,0}},
    {{0,0,4,0,0,0,0,0}},
    {{0,4,2,0,0,0,0,0}},
    {{4,4,2,0,0,2,0,0}},
    {{2,4,2,0,0,0,0,0}},
    {{4,2,0,0,0,0,0,0}},
    {{4,4,0,0,0,0,0,0}},
    {{4,4,2,0,0,0,0,0}},
    {{2,4,0,0,0,0,0,0}},
    {{0,4,0,0,0,0,0,0}},
    {{4,0,0,0,0,0,0,0}},
    {{0,0,0,4,4,0,0,0}},
    {{0,0,4,0,4,0,0,0}},
    {{0,0,4,4,0,0,0,0}},
    {{0,4,0,0,4,0,0,0}},
    {{0,4,0,4,0,0,0,0}},
    {{4,0,0,0,4,0,0,0}},
    {{4,0,0,4,0,0,0,0}},

    {{2,0,0,0,0,0,0,0}},
    {{0,2,0,0,0,0,0,0}},
    {{0,2,0,2,2,0,0,0}},
    {{0,2,2,0,2,0,0,0}},
    {{2,0,0,2,2,0,0,0}},
    {{2,0,2,0,2,0,0,0}},
    {{2,0,2,2,0,0,0,0}},
    {{2,2,0,0,2,0,0,0}},
    {{2,2,2,2,0,0,0,0}},
    {{2,2,0,2,0,0,0,0}},
    {{2,2,0,0,0,0,0,0}},
    {{0,0,2,0,0,0,0,0}},
    {{0,2,2,0,0,0,0,0}},
    {{2,2,2,0,0,0,0,0}},
    {{0,0,0,2,0,0,0,0}},
    {{2,0,2,0,0,0,0,0}},
    {{0,2,0,2,0,0,0,0}},
    {{0,0,2,2,0,0,0,0}},
    {{0,2,2,2,0,0,0,0}},
  };




  // 1 to skip ASCII space, vowels AEIOU aeiou and UTF-8 continuation bytes 80-BF
  static const uint8 kSkipSpaceVowelContinue[256] = {
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,1,0,0,0,1,0,0, 0,1,0,0,0,0,0,1, 0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0,
    0,1,0,0,0,1,0,0, 0,1,0,0,0,0,0,1, 0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0,

    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  };

  // 1 to skip ASCII space, and UTF-8 continuation bytes 80-BF
  static const uint8 kSkipSpaceContinue[256] = {
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,

    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  };


  // If != UNKNOWN, use nilgrams to determine language of this script
  static const Language kOnlyLanguagePerLScript[] = {
    ENGLISH,            // ULScript_Common, [no words should be in this script]
    UNKNOWN_LANGUAGE,   // ULScript_Latin,
    //UNKNOWN_LANGUAGE,   // ULScript_Greek,  Jan 2009: change so we can score quads
    GREEK,              // ULScript_Greek,  Mar 2009: change back; do gibberish separately
    UNKNOWN_LANGUAGE,   // ULScript_Cyrillic,
    ARMENIAN,           // ULScript_Armenian,
    UNKNOWN_LANGUAGE,   // ULScript_Hebrew,
    UNKNOWN_LANGUAGE,   // ULScript_Arabic,
    SYRIAC,             // ULScript_Syriac,
    DHIVEHI,            // ULScript_Thaana,
    UNKNOWN_LANGUAGE,   // ULScript_Devanagari,
    UNKNOWN_LANGUAGE,   // ULScript_Bengali,
    PUNJABI,            // ULScript_Gurmukhi,
    GUJARATI,           // ULScript_Gujarati,
    ORIYA,              // ULScript_Oriya,
    TAMIL,              // ULScript_Tamil,
    TELUGU,             // ULScript_Telugu,
    KANNADA,            // ULScript_Kannada,
    MALAYALAM,          // ULScript_Malayalam,
    SINHALESE,          // ULScript_Sinhala,
    THAI,               // ULScript_Thai,
    LAOTHIAN,           // ULScript_Lao,
    UNKNOWN_LANGUAGE,   // ULScript_Tibetan,
    BURMESE,            // ULScript_Myanmar,
    GEORGIAN,           // ULScript_Georgian,
    UNKNOWN_LANGUAGE,   // ULScript_HanCJK,
    UNKNOWN_LANGUAGE,   // ULScript_Ethiopic,
    CHEROKEE,           // ULScript_Cherokee,
    INUKTITUT,          // ULScript_Canadian_Aboriginal,
    X_OGHAM,            // ULScript_Ogham,
    X_RUNIC,            // ULScript_Runic,
    KHMER,              // ULScript_Khmer,
    MONGOLIAN,          // ULScript_Mongolian,
    X_YI,               // ULScript_Yi,
    X_OLD_ITALIC,       // ULScript_Old_Italic,
    X_GOTHIC,           // ULScript_Gothic,
    X_DESERET,          // ULScript_Deseret,
    ENGLISH,            // ULScript_Inherited, [no words should be in this script]
    TAGALOG,            // ULScript_Tagalog,
    X_HANUNOO,          // ULScript_Hanunoo,
    X_BUHID,            // ULScript_Buhid,
    X_TAGBANWA,         // ULScript_Tagbanwa,
    LIMBU,              // ULScript_Limbu,
    X_TAI_LE,           // ULScript_Tai_Le,
    X_LINEAR_B,         // ULScript_Linear_B,
    X_UGARITIC,         // ULScript_Ugaritic,
    X_SHAVIAN,          // ULScript_Shavian,
    X_OSMANYA,          // ULScript_Osmanya,
    X_CYPRIOT,          // ULScript_Cypriot,
    X_BUGINESE,         // ULScript_Buginese,
    X_COPTIC,           // ULScript_Coptic,
    X_NEW_TAI_LUE,      // ULScript_New_Tai_Lue,
    X_GLAGOLITIC,       // ULScript_Glagolitic,
    X_TIFINAGH,         // ULScript_Tifinagh,
    X_SYLOTI_NAGRI,     // ULScript_Syloti_Nagri,
    X_OLD_PERSIAN,      // ULScript_Old_Persian,
    X_KHAROSHTHI,       // ULScript_Kharoshthi,
    X_BALINESE,         // ULScript_Balinese,
    X_CUNEIFORM,        // ULScript_Cuneiform,
    X_PHOENICIAN,       // ULScript_Phoenician,
    X_PHAGS_PA,         // ULScript_Phags_Pa,
    X_NKO,              // ULScript_Nko,

    // Unicode 5.1
    X_SUDANESE,         // ULScript_Sundanese,
    X_LEPCHA,           // ULScript_Lepcha,
    X_OL_CHIKI,         // ULScript_Ol_Chiki,
    X_VAI,              // ULScript_Vai,
    X_SAURASHTRA,       // ULScript_Saurashtra,
    X_KAYAH_LI,         // ULScript_Kayah_Li,
    X_REJANG,           // ULScript_Rejang,
    X_LYCIAN,           // ULScript_Lycian,
    X_CARIAN,           // ULScript_Carian,
    X_LYDIAN,           // ULScript_Lydian,
    X_CHAM,             // ULScript_Cham,
  };

  COMPILE_ASSERT(arraysize(kOnlyLanguagePerLScript) == ULScript_NUM_SCRIPTS,
       kOnlyLanguagePerLScript_has_incorrect_length);


  // This is, in a sense, the complement of the table above
  // If != UNKNOWN, determines a default language of this script
  static const Language kDefaultLanguagePerLScript[] = {
    UNKNOWN_LANGUAGE,            // ULScript_Common, [no words should be in this script]
    ENGLISH,   // ULScript_Latin,
    UNKNOWN_LANGUAGE,              // ULScript_Greek,
    RUSSIAN,   // ULScript_Cyrillic,
    UNKNOWN_LANGUAGE,           // ULScript_Armenian,
    HEBREW,   // ULScript_Hebrew,
    ARABIC,   // ULScript_Arabic,
    UNKNOWN_LANGUAGE,             // ULScript_Syriac,
    UNKNOWN_LANGUAGE,            // ULScript_Thaana,
    HINDI,   // ULScript_Devanagari,
    BENGALI,   // ULScript_Bengali,
    UNKNOWN_LANGUAGE,            // ULScript_Gurmukhi,
    UNKNOWN_LANGUAGE,           // ULScript_Gujarati,
    UNKNOWN_LANGUAGE,              // ULScript_Oriya,
    UNKNOWN_LANGUAGE,              // ULScript_Tamil,
    UNKNOWN_LANGUAGE,             // ULScript_Telugu,
    UNKNOWN_LANGUAGE,            // ULScript_Kannada,
    UNKNOWN_LANGUAGE,          // ULScript_Malayalam,
    UNKNOWN_LANGUAGE,          // ULScript_Sinhala,
    UNKNOWN_LANGUAGE,               // ULScript_Thai,
    UNKNOWN_LANGUAGE,           // ULScript_Lao,
    TIBETAN,   // ULScript_Tibetan,
    UNKNOWN_LANGUAGE,            // ULScript_Myanmar,
    UNKNOWN_LANGUAGE,           // ULScript_Georgian,
    CHINESE,   // ULScript_HanCJK,
    AMHARIC,   // ULScript_Ethiopic,
    UNKNOWN_LANGUAGE,           // ULScript_Cherokee,
    UNKNOWN_LANGUAGE,          // ULScript_Canadian_Aboriginal,
    UNKNOWN_LANGUAGE,            // ULScript_Ogham,
    UNKNOWN_LANGUAGE,            // ULScript_Runic,
    UNKNOWN_LANGUAGE,              // ULScript_Khmer,
    UNKNOWN_LANGUAGE,          // ULScript_Mongolian,
    UNKNOWN_LANGUAGE,               // ULScript_Yi,
    UNKNOWN_LANGUAGE,       // ULScript_Old_Italic,
    UNKNOWN_LANGUAGE,           // ULScript_Gothic,
    UNKNOWN_LANGUAGE,          // ULScript_Deseret,
    UNKNOWN_LANGUAGE,            // ULScript_Inherited, [no words should be in this script]
    UNKNOWN_LANGUAGE,            // ULScript_Tagalog,
    UNKNOWN_LANGUAGE,          // ULScript_Hanunoo,
    UNKNOWN_LANGUAGE,            // ULScript_Buhid,
    UNKNOWN_LANGUAGE,         // ULScript_Tagbanwa,
    UNKNOWN_LANGUAGE,              // ULScript_Limbu,
    UNKNOWN_LANGUAGE,           // ULScript_Tai_Le,
    UNKNOWN_LANGUAGE,         // ULScript_Linear_B,
    UNKNOWN_LANGUAGE,         // ULScript_Ugaritic,
    UNKNOWN_LANGUAGE,          // ULScript_Shavian,
    UNKNOWN_LANGUAGE,          // ULScript_Osmanya,
    UNKNOWN_LANGUAGE,          // ULScript_Cypriot,
    UNKNOWN_LANGUAGE,         // ULScript_Buginese,
    UNKNOWN_LANGUAGE,           // ULScript_Coptic,
    UNKNOWN_LANGUAGE,      // ULScript_New_Tai_Lue,
    UNKNOWN_LANGUAGE,       // ULScript_Glagolitic,
    UNKNOWN_LANGUAGE,         // ULScript_Tifinagh,
    UNKNOWN_LANGUAGE,     // ULScript_Syloti_Nagri,
    UNKNOWN_LANGUAGE,      // ULScript_Old_Persian,
    UNKNOWN_LANGUAGE,       // ULScript_Kharoshthi,
    UNKNOWN_LANGUAGE,         // ULScript_Balinese,
    UNKNOWN_LANGUAGE,        // ULScript_Cuneiform,
    UNKNOWN_LANGUAGE,       // ULScript_Phoenician,
    UNKNOWN_LANGUAGE,         // ULScript_Phags_Pa,
    UNKNOWN_LANGUAGE,              // ULScript_Nko,

    // Unicode 5.1
    UNKNOWN_LANGUAGE,         // ULScript_Sundanese,
    UNKNOWN_LANGUAGE,           // ULScript_Lepcha,
    UNKNOWN_LANGUAGE,         // ULScript_Ol_Chiki,
    UNKNOWN_LANGUAGE,              // ULScript_Vai,
    UNKNOWN_LANGUAGE,       // ULScript_Saurashtra,
    UNKNOWN_LANGUAGE,         // ULScript_Kayah_Li,
    UNKNOWN_LANGUAGE,           // ULScript_Rejang,
    UNKNOWN_LANGUAGE,           // ULScript_Lycian,
    UNKNOWN_LANGUAGE,           // ULScript_Carian,
    UNKNOWN_LANGUAGE,           // ULScript_Lydian,
    UNKNOWN_LANGUAGE,             // ULScript_Cham,
  };

  COMPILE_ASSERT(arraysize(kDefaultLanguagePerLScript) == ULScript_NUM_SCRIPTS,
       kDefaultLanguagePerLScript_has_incorrect_length);


  // True for standalone languages (only lang in a script)
  // Subscripted by packed language number
  // If 1, we will use nilgrams to determine language
  static const uint8 kIsStandaloneLang[EXT_NUM_LANGUAGES + 1] = {
     0,
     0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,1,0,    // GREEK
     0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,
     0,1,0,0,1, 0,1,0,0,0, 0,0,1,1,0, 0,0,0,0,1,    // MALAYALAM..KANNADA
     1,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 1,0,0,0,1,    // PUNJABI..SINHALESE
     0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,1,1,0,    // ARMENIAN..LAOTHIAN

     0,0,0,0,1, 0,1,1,1,0, 1,0,0,0,0, 0,0,0,0,0,    // KHMER..ORIYA
     0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,
     0,1,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,    // INUKTITUT

     0,0,0,0,0,                                     // [160..164]
    // Add new language standalone bit just before here
     0,0,0,0,0, 1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,
     1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,

     1,1,1,1,
   };

   // True for ULScript_HanCJK
   // (Vietnamese and Zhuang also have Latin script quadgrams)
   // Subscripted by packed language number
   static const uint8 kIsUnigramLang[EXT_NUM_LANGUAGES + 1] = {
      0,
      0,0,0,0,0, 0,0,0,1,1, 0,0,0,0,0, 0,1,0,0,0,    // JAPANESE KOREAN CHINESE
      0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,    //
      0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,    //
      0,0,0,0,0, 0,1,0,0,1, 0,0,0,0,0, 0,0,0,0,0,    // VIETNAMESE CHINESE_T
      0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,    //

      0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,    //
      0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,    //
      0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 1,0,0,0,0,    // ZHUANG

      0,0,0,0,0,                                     // [160..164]
     // Add new language unigram bit just before here

      0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,    //
      0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,    //

      0,0,0,0,
   };


  // True for ULScript_HanCJK
  // Subscripted by lscript number
  static const uint8 kScoreUniPerLScript[] = {
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
  };

  COMPILE_ASSERT(arraysize(kScoreUniPerLScript) == ULScript_NUM_SCRIPTS,
       kScoreUniPerLScript_has_incorrect_length);


  // Defines Top40 packed languages
  //
  // Tier 0/1 Language enum list (16)
  //   ENGLISH, /*no en_GB,*/ FRENCH, ITALIAN, GERMAN, SPANISH,    // E - FIGS
  //   DUTCH, CHINESE, CHINESE_T, JAPANESE, KOREAN,
  //   PORTUGUESE, RUSSIAN, POLISH, TURKISH, THAI,
  //   ARABIC,
  //
  // Tier 2 Language enum list (22)
  //   SWEDISH, FINNISH, DANISH, /*no pt-PT,*/ ROMANIAN, HUNGARIAN,
  //   HEBREW, INDONESIAN, CZECH, GREEK, NORWEGIAN,
  //   VIETNAMESE, BULGARIAN, CROATIAN, LITHUANIAN, SLOVAK,
  //   TAGALOG, SLOVENIAN, SERBIAN, CATALAN, LATVIAN,
  //   UKRAINIAN, HINDI,
  //
  //   use SERBO_CROATIAN instead of BOSNIAN, SERBIAN, CROATIAN, MONTENEGRIN(21)
  //
  // Include IgnoreMe (TG_UNKNOWN_LANGUAGE, 25+1) as a top 40

  // NOTE: packed, i.e. Language enum + 1
  static const uint8 kIsPackedTop40[EXT_NUM_LANGUAGES + 1] = {
    0,
    1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,0,
    1,1,1,1,0, 1,0,1,0,0, 0,0,1,1,1, 1,0,0,1,0,
    0,0,0,0,0, 0,0,0,0,0, 0,0,0,1,1, 1,0,0,0,0,
    0,0,0,1,0, 0,1,0,1,1, 0,0,0,0,0, 0,0,0,0,0,
    0,0,0,0,0, 0,0,0,0,0, 0,0,1,0,0, 0,0,0,0,0,

    0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,
    0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,
    0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,

    0,0,0,0,0,                                    // [160..164]
    // Add new language top40 bit just before here

    0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,
    0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,

    0,0,0,0,
  };



  // Table has 234 eight-byte entries. Each entry has a five-byte array and
  // a three-byte array of log base 2 probabilities in the range 0..11.
  // The intended use is to express five or three probabilities in a single-byte
  // subscript, then decode via this table. These probabilities are
  // intended to go with an array of five or three language numbers.
  //
  // The corresponding language numbers will have to be sorted by descending
  // probability, then the actual probability subscript chosen to match the
  // closest available entry in this table.
  //
  // Pattern of probability values:
  // hi 3/4 1/2 1/4 lo    hi mid lo
  // where "3/4" is (hi*3+lo)/4, "1/2" is (hi+lo)/2, and "1/4" is (hi+lo*3)/4 and
  // mid is one of 3/4 1/2 or 1/4.
  // There are three groups of 78 (=12*13/2) entries, with hi running 0..11 and
  // lo running 0..hi. Only the first group is used for five-entry lookups.
  // The mid value in the first group is 1/2, the second group 3/4, and the
  // third group 1/4. For three-entry lookups, this allows the mid entry to be
  // somewhat higher or lower than the midpoint, to allow a better match to the
  // original probabilities.
  static const int kLgProbV2TblSize = 234;
  static const uint8 kLgProbV2Tbl[kLgProbV2TblSize * 8] = {
    1,1,1,1,1, 1,1,1,     // [0]
    2,2,2,1,1, 2,2,1,     // [1]
    2,2,2,2,2, 2,2,2,
    3,3,2,2,1, 3,2,1,     // [3]
    3,3,3,2,2, 3,3,2,
    3,3,3,3,3, 3,3,3,
    4,3,3,2,1, 4,3,1,     // [6]
    4,4,3,3,2, 4,3,2,
    4,4,4,3,3, 4,4,3,
    4,4,4,4,4, 4,4,4,
    5,4,3,2,1, 5,3,1,     // [10]
    5,4,4,3,2, 5,4,2,
    5,5,4,4,3, 5,4,3,
    5,5,5,4,4, 5,5,4,
    5,5,5,5,5, 5,5,5,
    6,5,4,2,1, 6,4,1,     // [15]
    6,5,4,3,2, 6,4,2,
    6,5,5,4,3, 6,5,3,
    6,6,5,5,4, 6,5,4,
    6,6,6,5,5, 6,6,5,
    6,6,6,6,6, 6,6,6,
    7,6,4,3,1, 7,4,1,     // [21]
    7,6,5,3,2, 7,5,2,
    7,6,5,4,3, 7,5,3,
    7,6,6,5,4, 7,6,4,
    7,7,6,6,5, 7,6,5,
    7,7,7,6,6, 7,7,6,
    7,7,7,7,7, 7,7,7,
    8,6,5,3,1, 8,5,1,     // [28]
    8,7,5,4,2, 8,5,2,
    8,7,6,4,3, 8,6,3,
    8,7,6,5,4, 8,6,4,
    8,7,7,6,5, 8,7,5,
    8,8,7,7,6, 8,7,6,
    8,8,8,7,7, 8,8,7,
    8,8,8,8,8, 8,8,8,
    9,7,5,3,1, 9,5,1,     // [36]
    9,7,6,4,2, 9,6,2,
    9,8,6,5,3, 9,6,3,
    9,8,7,5,4, 9,7,4,
    9,8,7,6,5, 9,7,5,
    9,8,8,7,6, 9,8,6,
    9,9,8,8,7, 9,8,7,
    9,9,9,8,8, 9,9,8,
    9,9,9,9,9, 9,9,9,
    10,8,6,3,1, 10,6,1,   // [45]
    10,8,6,4,2, 10,6,2,
    10,8,7,5,3, 10,7,3,
    10,9,7,6,4, 10,7,4,
    10,9,8,6,5, 10,8,5,
    10,9,8,7,6, 10,8,6,
    10,9,9,8,7, 10,9,7,
    10,10,9,9,8, 10,9,8,
    10,10,10,9,9, 10,10,9,
    10,10,10,10,10, 10,10,10,
    11,9,6,4,1, 11,6,1,   // [55]
    11,9,7,4,2, 11,7,2,
    11,9,7,5,3, 11,7,3,
    11,9,8,6,4, 11,8,4,
    11,10,8,7,5, 11,8,5,
    11,10,9,7,6, 11,9,6,
    11,10,9,8,7, 11,9,7,
    11,10,10,9,8, 11,10,8,
    11,11,10,10,9, 11,10,9,
    11,11,11,10,10, 11,11,10,
    11,11,11,11,11, 11,11,11,
    12,9,7,4,1, 12,7,1,   // [66]
    12,10,7,5,2, 12,7,2,
    12,10,8,5,3, 12,8,3,
    12,10,8,6,4, 12,8,4,
    12,10,9,7,5, 12,9,5,
    12,11,9,8,6, 12,9,6,
    12,11,10,8,7, 12,10,7,
    12,11,10,9,8, 12,10,8,
    12,11,11,10,9, 12,11,9,
    12,12,11,11,10, 12,11,10,
    12,12,12,11,11, 12,12,11,
    12,12,12,12,12, 12,12,12,

    1,1,1,1,1, 1,1,1,
    2,2,2,1,1, 2,2,1,
    2,2,2,2,2, 2,2,2,
    3,3,2,2,1, 3,3,1,
    3,3,3,2,2, 3,3,2,
    3,3,3,3,3, 3,3,3,
    4,3,3,2,1, 4,3,1,
    4,4,3,3,2, 4,4,2,
    4,4,4,3,3, 4,4,3,
    4,4,4,4,4, 4,4,4,
    5,4,3,2,1, 5,4,1,
    5,4,4,3,2, 5,4,2,
    5,5,4,4,3, 5,5,3,
    5,5,5,4,4, 5,5,4,
    5,5,5,5,5, 5,5,5,
    6,5,4,2,1, 6,5,1,
    6,5,4,3,2, 6,5,2,
    6,5,5,4,3, 6,5,3,
    6,6,5,5,4, 6,6,4,
    6,6,6,5,5, 6,6,5,
    6,6,6,6,6, 6,6,6,
    7,6,4,3,1, 7,6,1,
    7,6,5,3,2, 7,6,2,
    7,6,5,4,3, 7,6,3,
    7,6,6,5,4, 7,6,4,
    7,7,6,6,5, 7,7,5,
    7,7,7,6,6, 7,7,6,
    7,7,7,7,7, 7,7,7,
    8,6,5,3,1, 8,6,1,
    8,7,5,4,2, 8,7,2,
    8,7,6,4,3, 8,7,3,
    8,7,6,5,4, 8,7,4,
    8,7,7,6,5, 8,7,5,
    8,8,7,7,6, 8,8,6,
    8,8,8,7,7, 8,8,7,
    8,8,8,8,8, 8,8,8,
    9,7,5,3,1, 9,7,1,
    9,7,6,4,2, 9,7,2,
    9,8,6,5,3, 9,8,3,
    9,8,7,5,4, 9,8,4,
    9,8,7,6,5, 9,8,5,
    9,8,8,7,6, 9,8,6,
    9,9,8,8,7, 9,9,7,
    9,9,9,8,8, 9,9,8,
    9,9,9,9,9, 9,9,9,
    10,8,6,3,1, 10,8,1,
    10,8,6,4,2, 10,8,2,
    10,8,7,5,3, 10,8,3,
    10,9,7,6,4, 10,9,4,
    10,9,8,6,5, 10,9,5,
    10,9,8,7,6, 10,9,6,
    10,9,9,8,7, 10,9,7,
    10,10,9,9,8, 10,10,8,
    10,10,10,9,9, 10,10,9,
    10,10,10,10,10, 10,10,10,
    11,9,6,4,1, 11,9,1,
    11,9,7,4,2, 11,9,2,
    11,9,7,5,3, 11,9,3,
    11,9,8,6,4, 11,9,4,
    11,10,8,7,5, 11,10,5,
    11,10,9,7,6, 11,10,6,
    11,10,9,8,7, 11,10,7,
    11,10,10,9,8, 11,10,8,
    11,11,10,10,9, 11,11,9,
    11,11,11,10,10, 11,11,10,
    11,11,11,11,11, 11,11,11,
    12,9,7,4,1, 12,9,1,
    12,10,7,5,2, 12,10,2,
    12,10,8,5,3, 12,10,3,
    12,10,8,6,4, 12,10,4,
    12,10,9,7,5, 12,10,5,
    12,11,9,8,6, 12,11,6,
    12,11,10,8,7, 12,11,7,
    12,11,10,9,8, 12,11,8,
    12,11,11,10,9, 12,11,9,
    12,12,11,11,10, 12,12,10,
    12,12,12,11,11, 12,12,11,
    12,12,12,12,12, 12,12,12,

    1,1,1,1,1, 1,1,1,
    2,2,2,1,1, 2,1,1,
    2,2,2,2,2, 2,2,2,
    3,3,2,2,1, 3,2,1,
    3,3,3,2,2, 3,2,2,
    3,3,3,3,3, 3,3,3,
    4,3,3,2,1, 4,2,1,
    4,4,3,3,2, 4,3,2,
    4,4,4,3,3, 4,3,3,
    4,4,4,4,4, 4,4,4,
    5,4,3,2,1, 5,2,1,
    5,4,4,3,2, 5,3,2,
    5,5,4,4,3, 5,4,3,
    5,5,5,4,4, 5,4,4,
    5,5,5,5,5, 5,5,5,
    6,5,4,2,1, 6,2,1,
    6,5,4,3,2, 6,3,2,
    6,5,5,4,3, 6,4,3,
    6,6,5,5,4, 6,5,4,
    6,6,6,5,5, 6,5,5,
    6,6,6,6,6, 6,6,6,
    7,6,4,3,1, 7,3,1,
    7,6,5,3,2, 7,3,2,
    7,6,5,4,3, 7,4,3,
    7,6,6,5,4, 7,5,4,
    7,7,6,6,5, 7,6,5,
    7,7,7,6,6, 7,6,6,
    7,7,7,7,7, 7,7,7,
    8,6,5,3,1, 8,3,1,
    8,7,5,4,2, 8,4,2,
    8,7,6,4,3, 8,4,3,
    8,7,6,5,4, 8,5,4,
    8,7,7,6,5, 8,6,5,
    8,8,7,7,6, 8,7,6,
    8,8,8,7,7, 8,7,7,
    8,8,8,8,8, 8,8,8,
    9,7,5,3,1, 9,3,1,
    9,7,6,4,2, 9,4,2,
    9,8,6,5,3, 9,5,3,
    9,8,7,5,4, 9,5,4,
    9,8,7,6,5, 9,6,5,
    9,8,8,7,6, 9,7,6,
    9,9,8,8,7, 9,8,7,
    9,9,9,8,8, 9,8,8,
    9,9,9,9,9, 9,9,9,
    10,8,6,3,1, 10,3,1,
    10,8,6,4,2, 10,4,2,
    10,8,7,5,3, 10,5,3,
    10,9,7,6,4, 10,6,4,
    10,9,8,6,5, 10,6,5,
    10,9,8,7,6, 10,7,6,
    10,9,9,8,7, 10,8,7,
    10,10,9,9,8, 10,9,8,
    10,10,10,9,9, 10,9,9,
    10,10,10,10,10, 10,10,10,
    11,9,6,4,1, 11,4,1,
    11,9,7,4,2, 11,4,2,
    11,9,7,5,3, 11,5,3,
    11,9,8,6,4, 11,6,4,
    11,10,8,7,5, 11,7,5,
    11,10,9,7,6, 11,7,6,
    11,10,9,8,7, 11,8,7,
    11,10,10,9,8, 11,9,8,
    11,11,10,10,9, 11,10,9,
    11,11,11,10,10, 11,10,10,
    11,11,11,11,11, 11,11,11,
    12,9,7,4,1, 12,4,1,
    12,10,7,5,2, 12,5,2,
    12,10,8,5,3, 12,5,3,
    12,10,8,6,4, 12,6,4,
    12,10,9,7,5, 12,7,5,
    12,11,9,8,6, 12,8,6,
    12,11,10,8,7, 12,8,7,
    12,11,10,9,8, 12,9,8,
    12,11,11,10,9, 12,10,9,
    12,12,11,11,10, 12,11,10,
    12,12,12,11,11, 12,11,11,
    12,12,12,12,12, 12,12,12,
  };

  // Backmap a single desired probability into an entry in kLgProbV2Tbl
  static const uint8 kLgProbV2TblBackmap[13] = {
    0,
    0, 1, 3, 6,   10, 15, 21, 28,   36, 45, 55, 66,
  };


  // Always advances one UTF-8 character
  static const uint8 kAdvanceOneChar[256] = {
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,

    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,
  };

  // Does not advance past space or cr/lf/nul
  static const uint8 kAdvanceOneCharButSpace[256] = {
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,

    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,
  };

  // Advances *only* on space or ASCII vowel (or illegal byte)
  static const uint8 kAdvanceOneCharSpaceVowel[256] = {
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,1,0,0,0,1,0,0, 0,1,0,0,0,0,0,1, 0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0,
    0,1,0,0,0,1,0,0, 0,1,0,0,0,0,0,1, 0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0,

    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  };

  // Advances *only* on space (or illegal byte)
  static const uint8 kAdvanceOneCharSpace[256] = {
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,

    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  };


//------------------------------------------------------------------------------
// General
//------------------------------------------------------------------------------
  static inline int minint(int a, int b) {return (a < b) ? a: b;}
  static inline int maxint(int a, int b) {return (a > b) ? a: b;}

  // Here to make available for debugging
  int ReliabilityDelta(int value1, int value2, int count);
  int ReliabilityMainstream(int topscore, int len, int mean_score);

  // Returns "0" for too small
  inline const char* MyExtLanguageCode(Language lang) {
    return ExtLanguageCode(lang);
  }

  // Map script into Latin, Cyrillic, Arabic, Other. Used in keeping track of
  // amount of training data for language-script combinations
  inline int LScript4(UnicodeLScript lscript) {
    if (lscript == ULScript_Latin) {return 0;}
    if (lscript == ULScript_Cyrillic) {return 1;}
    if (lscript == ULScript_Arabic) {return 2;}
    return 3;
  }


  // Routines to access 3 or 5 log probabilities in a single byte.

  // Return address of 8-byte entry[i]
  inline const uint8* LgProb2TblEntry(int i) {
    return &kLgProbV2Tbl[i * 8];
  }

  // Return one of five probabilities in an entry
  // CURRENTLY UNUSED
  inline uint8 LgProb5(const uint8* entry, int j) {
    return entry[j];
  }

  // Return one of three probabilities in an entry
  inline uint8 LgProb3(const uint8* entry, int j) {
    return entry[j + 5];
  }



//------------------------------------------------------------------------------
// Hashing groups of 1/2/4/8 letters, perhaps with spaces or underscores
//------------------------------------------------------------------------------

  // Pick up 1..12 bytes and hash them via mask/shift/add. NO pre/post
  // OVERSHOOTS up to 3 bytes
  uint32 BiHashV25(const char* word_ptr, int bytecount);

  // Pick up 1..12 bytes plus pre/post space and hash them via mask/shift/add
  // UNDERSHOOTS 1 byte, OVERSHOOTS up to 3 bytes
  uint32 QuadHashV25(const char* word_ptr, int bytecount);

  // Pick up 1..12 bytes plus pre/post '_' and hash them via mask/shift/add
  // OVERSHOOTS up to 3 bytes
  uint32 QuadHashV25Underscore(const char* word_ptr, int bytecount);


  // Pick up 1..24 bytes plus pre/post space and hash them via mask/shift/add
  // UNDERSHOOTS 1 byte, OVERSHOOTS up to 3 bytes
  // For runtime use of tables V3
  uint64 OctaHash40(const char* word_ptr, int bytecount);

  uint64 OctaHash40underscore(const char* word_ptr, int bytecount);


  // From 32-bit gram FP, return hash table subscript and remaining key
  inline void QuadFPJustHash(uint32 quadhash,
                                  uint32 keymask,
                                  int bucketcount,
                                  uint32* subscr, uint32* hashkey) {
    *subscr = (quadhash + (quadhash >> 12)) & (bucketcount - 1);
    *hashkey = quadhash & keymask;
  }

  // Look up 32-bit gram FP in caller-passed table
  // Typical size 256K entries (1.5MB)
  // Two-byte hashkey
  inline const uint32 QuadHashV3Lookup4(const cld::CLDTableSummary* gram_obj,
                                        uint32 quadhash) {

    uint32 subscr, hashkey;
    const IndirectProbBucket4* quadtable = gram_obj->kCLDTable;
    uint32 keymask = gram_obj->kCLDTableKeyMask;
    int bucketcount = gram_obj->kCLDTableSize;
    QuadFPJustHash(quadhash, keymask, bucketcount, &subscr, &hashkey);
    const IndirectProbBucket4* bucket_ptr = &quadtable[subscr];
    // Four-way associative, 4 compares
    if (((hashkey ^ bucket_ptr->keyvalue[0]) & keymask) == 0) {
      return bucket_ptr->keyvalue[0];
    }
    if (((hashkey ^ bucket_ptr->keyvalue[1]) & keymask) == 0) {
      return bucket_ptr->keyvalue[1];
    }
    if (((hashkey ^ bucket_ptr->keyvalue[2]) & keymask) == 0) {
      return bucket_ptr->keyvalue[2];
    }
    if (((hashkey ^ bucket_ptr->keyvalue[3]) & keymask) == 0) {
      return bucket_ptr->keyvalue[3];
    }
    return 0;
  }


  // Map 40 bits to subscript, hashkey, expected 18-22 bit subscript (min 16)
  //     wwwwwwww xxxxxxxx xxxxxxxx yyyyyyyy yyyyyyyy
  //   + ........ ....wwww wwwwxxxx xxxxxxxx xxxxyyyy
  //     00000000 00000000 00000011 11111111 11111111 (18-bit bucketcount-1)
  //
  // hashkey:
  //              wwwwxxxx xxxxxxxx xxxx.... ........ (20-bit keymask)
  // 12-bit shift in subscript mixes in ~4 letters x 4 bits each

  // From 40-bit gram FP, return hash table subscript and remaining key
  inline void OctaFPJustHash(uint64 longwordhash,
                                    uint32 keymask,
                                    int bucketcount,
                                    uint32* subscr, uint32* hashkey) {
    uint32 temp = (longwordhash + (longwordhash >> 12)) & (bucketcount - 1);
    *subscr = temp;
    temp = longwordhash >> 4;
    *hashkey = temp & keymask;
  }

  // Look up 40-bit gram FP in caller-passed table
  // Typical size 256K-4M entries (1-16MB)
  // 24-12 bit hashkey packed with 8-20 bit indirect lang/probs
  // keymask is 0xfffff000 for 20-bit hashkey and 12-bit indirect
  inline const uint32 OctaHashV3Lookup4(const cld::CLDTableSummary* gram_obj,
                                            uint64 longwordhash) {
    uint32 subscr, hashkey;
    const IndirectProbBucket4* octatable = gram_obj->kCLDTable;
    uint32 keymask = gram_obj->kCLDTableKeyMask;
    int bucketcount = gram_obj->kCLDTableSize;
    OctaFPJustHash(longwordhash, keymask, bucketcount,
                          &subscr, &hashkey);
    const IndirectProbBucket4* bucket_ptr = &octatable[subscr];
    // Four-way associative, 4 compares
    if (((hashkey ^ bucket_ptr->keyvalue[0]) & keymask) == 0) {
      return bucket_ptr->keyvalue[0];
    }
    if (((hashkey ^ bucket_ptr->keyvalue[1]) & keymask) == 0) {
      return bucket_ptr->keyvalue[1];
    }
    if (((hashkey ^ bucket_ptr->keyvalue[2]) & keymask) == 0) {
      return bucket_ptr->keyvalue[2];
    }
    if (((hashkey ^ bucket_ptr->keyvalue[3]) & keymask) == 0) {
      return bucket_ptr->keyvalue[3];
    }
    return 0;
  }



//------------------------------------------------------------------------------
// Scoring single groups of letters
//------------------------------------------------------------------------------

  // UNIGRAM score one => tote
  // Input: 1-byte entry of subscript into unigram probs, plus
  //  an accumulator tote.
  // Output: running sums in tote updated
  void ProcessProbV25UniTote(int propval, Tote* tote);

  // BIGRAM, QUADGRAM, OCTAGRAM score one => tote
  // Input: 4-byte entry of 3 language numbers and one probability subscript,
  //  plus an accumulator tote. (language 0 means unused entry)
  // Output: running sums in tote updated
  void ProcessProbV25Tote(uint32 probs, Tote* tote);


//------------------------------------------------------------------------------
// Routines to accumulate probabilities
//------------------------------------------------------------------------------

  // Score up to n=gram_limit unigrams, returning number of bytes consumed
  // Caller supplies table, such as compact_lang_det_generated_ctjkvz_b1_obj
  int DoUniScoreV3(const UTF8PropObj* unigram_obj,
                   const char* isrc, int srclen, int advance_by,
                   int* tote_grams, int gram_limit, Tote* chunk_tote);


  // Score all words in isrc, using languages that have bigrams (CJK)
  // Caller supplies table, such as &kCjkBiTable_obj or &kGibberishTable_obj
  // Return number of bigrams that hit in the hash table
  int DoBigramScoreV3(const cld::CLDTableSummary* bigram_obj,
                      const char* isrc, int srclen, Tote* chunk_tote);


  // Score up to n=gram_limit quadgrams, returning number of bytes consumed
  // Caller supplies table, such as &kQuadTable_obj or &kGibberishTable_obj
  int DoQuadScoreV3(const cld::CLDTableSummary* quadgram_obj,
                    const char* isrc, int srclen, int advance_by,
                    int* tote_grams, int gram_limit, Tote* chunk_tote);

  // Score all octagrams (words) in isrc, using languages that have quadgrams
  // Caller supplies table, such as &kLongWord8Table_obj
  // Return number of words that hit in the hash table
  int DoOctaScoreV3(const cld::CLDTableSummary* octagram_obj,
                    const char* isrc, int srclen, Tote* chunk_tote);

//------------------------------------------------------------------------------
// Reliability calculations, for single language and between languages
//------------------------------------------------------------------------------

  // Reliability = 0..100
  static const int kMinReliable = 75;

  // Calculate ratio of score per 1KB vs. expected score per 1KB
  double GetNormalizedScore(Language lang, UnicodeLScript lscript,
                          int bytes, int score);

  // Calculate reliablity of len bytes of script lscript with chunk_tote
  int GetReliability(int len, UnicodeLScript lscript, const Tote* chunk_tote);


//------------------------------------------------------------------------------
// Miscellaneous
//------------------------------------------------------------------------------

  // Make languages packed into uint32 values non-zero
  // These routines later could remap so languages not in QuadHash tables are not
  // represented, and so that any thrashing in accumulation is eliminated
  uint8 inline PackLanguage(Language lang) {
    return static_cast<uint8>(lang + 1);}

  Language inline UnpackLanguage(int ilang) {
    return static_cast<Language>(ilang - 1);}

  // Useful single-byte tests
  bool inline IsUTF8ContinueByte(char c) {
    return static_cast<signed char>(c) < -64;}
  bool inline IsUTF8HighByte(char c) {
    return static_cast<signed char>(c) < 0;}


  // Demote all languages except Top40 and plus_one
  // Do this just before sorting
  void DemoteNotTop40(Tote* chunk_tote, int packed_plus_one);

}       // End namespace cld


#endif  // I18N_ENCODINGS_COMPACT_LANG_DET_CLDUTIL_H_
