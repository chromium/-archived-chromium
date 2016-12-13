// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/cldutil.h"

// Suppressed:
//      az-Arab az-Cyrl ku-Latn tg-Arab za-Hani zzb-Latn zze-Latn zzh-Latn ru-Latn

// Remapped:
//      xxx-Latn=>ut-Latn sh-Latn=>hr-Latn sh-Cyrl=>sr-Cyrl

static const int kCjkBiTableBuildDate = 20090129;    // yyyymmdd
static const int kCjkBiTableSize = 1;    // Bucket count
static const int kCjkBiTableKeyMask = 0xffffffff;    // Mask hash key

COMPILE_ASSERT(MONTENEGRIN == 160, k_montenegrin_changed);
COMPILE_ASSERT(EXT_NUM_LANGUAGES == 209, k_ext_num_languages_changed);

// Empty table
static const cld::IndirectProbBucket4 kCjkBiTable[kCjkBiTableSize] = {
  // key[4], words[4] in UTF-8
  // value[4]
  { {0x00000000,0x00000000,0x00000000,0x00000000}},  // [000] c
};

static const uint32 kCjkBiTableInd[1] = {
  // [0000]
  0x00000000, };

COMPILE_ASSERT(1 < (1 << 16), k_indirectbits_too_small);


extern const cld::CLDTableSummary kCjkBiTable_obj = {
  kCjkBiTable,
  kCjkBiTableInd,
  kCjkBiTableSize,
  ARRAYSIZE(kCjkBiTableInd),
  kCjkBiTableKeyMask,
  kCjkBiTableBuildDate,
};

// End of generated tables
