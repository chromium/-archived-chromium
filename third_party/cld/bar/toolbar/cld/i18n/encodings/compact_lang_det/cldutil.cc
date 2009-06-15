// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/cldutil.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/cldutil_dbg.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det_generated_meanscore.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/utf8propletterscriptnum.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_commandlineflags.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_logging.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_unilib.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8statetable.h"

// Runtime routines for hashing, looking up, and scoring
// unigrams (CJK), bigrams (CJK), quadgrams, and octagrams.
// Unigrams and bigrams are for CJK languages only, including simplified/
// traditional Chinese, Japanese, Korean, Vietnamese Han characters, and
// Zhuang Han characters. Surrounding spaces are not considered.
// Quadgrams and octagrams for for non-CJK and include two bits indicating
// preceding and trailing spaces (word boundaries).


// Indicator bits for leading/trailing space around quad/octagram
// NOTE: 4444 bits are chosen to flip constant bits in hash of four chars of
// 1-, 2-, or 3-bytes each.
static const uint32 kPreSpaceIndicator =  0x00004444;
static const uint32 kPostSpaceIndicator = 0x44440000;

// Little-endian masks for 0..24 bytes picked up as uint32's
static const uint32 kWordMask0[4] = {
  0xFFFFFFFF, 0x000000FF, 0x0000FFFF, 0x00FFFFFF
};

static const int kMinCJKUTF8CharBytes = 3;

static const int kMinGramCount = 3;
static const int kMaxGramCount = 16;




// Routines to access a hash table of <key:wordhash, value:probs> pairs
// Buckets have 4-byte wordhash for sizes < 32K buckets, but only
// 2-byte wordhash for sizes >= 32K buckets, with other wordhash bits used as
// bucket subscript.
// Probs is a packed: three languages plus a subscript for probability table
// Buckets have all the keys together, then all the values.Key array never
// crosses a cache-line boundary, so no-match case takes exactly one cache miss.
// Match case may sometimes take an additional cache miss on value access.
//
// Other possibilites include 5 or 10 6-byte entries plus pad to make 32 or 64
// byte buckets with single cache miss.
// Or 2-byte key and 6-byte value, allowing 5 languages instead  of three.
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Hashing groups of 1/2/4/8 letters, perhaps with spaces or underscores
//------------------------------------------------------------------------------

// Design principles for these hash functions
// - Few operations
// - Handle 1-, 2-, and 3-byte UTF-8 scripts, ignoring intermixing except in
//   Latin script expect 1- and 2-byte mixtures.
// - Last byte of each character has about 5 bits of information
// - Spread good bits around so they can interact in at least two ways
//   with other characters
// - Use add for additional mixing thorugh carries

// CJK Three-byte bigram
//   ....dddd..cccccc..bbbbbb....aaaa
//   ..................ffffff..eeeeee
// make
//   ....dddd..cccccc..bbbbbb....aaaa
//   000....dddd..cccccc..bbbbbb....a
//   ..................ffffff..eeeeee
//   ffffff..eeeeee000000000000000000
//
// CJK Four-byte bigram
//   ..dddddd..cccccc....bbbb....aaaa
//   ..hhhhhh..gggggg....ffff....eeee
// make
//   ..dddddd..cccccc....bbbb....aaaa
//   000..dddddd..cccccc....bbbb....a
//   ..hhhhhh..gggggg....ffff....eeee
//   ..ffff....eeee000000000000000000

// BIGRAM
// Pick up 1..8 bytes and hash them via mask/shift/add. NO pre/post
// OVERSHOOTS up to 3 bytes
// For runtime use of tables
uint32 cld::BiHashV25(const char* word_ptr, int bytecount) {
  const uint32* word_ptr32 = reinterpret_cast<const uint32*>(word_ptr);
  uint32 word0, word1;
  if (bytecount <= 4) {
    word0 = word_ptr32[0] & kWordMask0[bytecount & 3];
    word0 = word0 ^ (word0 >> 3);
    return word0;
  }
  // Else do 8 bytes
  word0 = word_ptr32[0];
  word0 = word0 ^ (word0 >> 3);
  word1 = word_ptr32[1] & kWordMask0[bytecount & 3];
  word1 = word1 ^ (word1 << 18);
  return word0 + word1;
}

//
// Ascii-7 One-byte chars
//   ...ddddd...ccccc...bbbbb...aaaaa
// make
//   ...ddddd...ccccc...bbbbb...aaaaa
//   000...ddddd...ccccc...bbbbb...aa
//
// Latin 1- and 2-byte chars
//   ...ddddd...ccccc...bbbbb...aaaaa
//   ...................fffff...eeeee
// make
//   ...ddddd...ccccc...bbbbb...aaaaa
//   000...ddddd...ccccc...bbbbb...aa
//   ...................fffff...eeeee
//   ...............fffff...eeeee0000
//
// Non-CJK Two-byte chars
//   ...ddddd...........bbbbb........
//   ...hhhhh...........fffff........
// make
//   ...ddddd...........bbbbb........
//   000...ddddd...........bbbbb.....
//   ...hhhhh...........fffff........
//   hhhh...........fffff........0000
//
// Non-CJK Three-byte chars
//   ...........ccccc................
//   ...................fffff........
//   ...lllll...................iiiii
// make
//   ...........ccccc................
//   000...........ccccc.............
//   ...................fffff........
//   ...............fffff........0000
//   ...lllll...................iiiii
//   .lllll...................iiiii00
//

// QUADGRAM
// Pick up 1..12 bytes plus pre/post space and hash them via mask/shift/add
// OVERSHOOTS up to 3 bytes
// For runtime use of tables
uint32 QuadHashV25Mix(const char* word_ptr, int bytecount, uint32 prepost) {
  const uint32* word_ptr32 = reinterpret_cast<const uint32*>(word_ptr);
  uint32 word0, word1, word2;
  if (bytecount <= 4) {
    word0 = word_ptr32[0] & kWordMask0[bytecount & 3];
    word0 = word0 ^ (word0 >> 3);
    return word0 ^ prepost;
  } else if (bytecount <= 8) {
    word0 = word_ptr32[0];
    word0 = word0 ^ (word0 >> 3);
    word1 = word_ptr32[1] & kWordMask0[bytecount & 3];
    word1 = word1 ^ (word1 << 4);
    return (word0 ^ prepost) + word1;
  }
  // else do 12 bytes
  word0 = word_ptr32[0];
  word0 = word0 ^ (word0 >> 3);
  word1 = word_ptr32[1];
  word1 = word1 ^ (word1 << 4);
  word2 = word_ptr32[2] & kWordMask0[bytecount & 3];
  word2 = word2 ^ (word2 << 2);
  return (word0 ^ prepost) + word1 + word2;
}


// QUADGRAM wrapper with surrounding spaces
// Pick up 1..12 bytes plus pre/post space and hash them via mask/shift/add
// UNDERSHOOTS 1 byte, OVERSHOOTS up to 3 bytes
// For runtime use of tables
uint32 cld::QuadHashV25(const char* word_ptr, int bytecount) {
  uint32 prepost = 0;
  if (word_ptr[-1] == ' ') {prepost |= kPreSpaceIndicator;}
  if (word_ptr[bytecount] == ' ') {prepost |= kPostSpaceIndicator;}
  return QuadHashV25Mix(word_ptr, bytecount, prepost);
}

// QUADGRAM wrapper with surrounding underscores (offline use)
// Pick up 1..12 bytes plus pre/post '_' and hash them via mask/shift/add
// OVERSHOOTS up to 3 bytes
// For offline construction of tables
uint32 cld::QuadHashV25Underscore(const char* word_ptr, int bytecount) {
  const char* local_word_ptr = word_ptr;
  int local_bytecount = bytecount;
  uint32 prepost = 0;
  if (local_word_ptr[0] == '_') {
    prepost |= kPreSpaceIndicator;
    ++local_word_ptr;
    --local_bytecount;
  }
  if (local_word_ptr[local_bytecount - 1] == '_') {
    prepost |= kPostSpaceIndicator;
    --local_bytecount;
  }
  return QuadHashV25Mix(local_word_ptr, local_bytecount, prepost);
}


// OCTAGRAM
// Pick up 1..24 bytes plus pre/post space and hash them via mask/shift/add
// UNDERSHOOTS 1 byte, OVERSHOOTS up to 3 bytes
//
// The low 32 bits follow the pattern from above, tuned to different scripts
// The high 8 bits are a simple sum of all bytes, shifted by 0/1/2/3 bits each
// For runtime use of tables V3
uint64 OctaHash40Mix(const char* word_ptr, int bytecount, uint64 prepost) {
  const uint32* word_ptr32 = reinterpret_cast<const uint32*>(word_ptr);
  uint64 word0;
  uint64 word1;
  uint64 sum;

  if (word_ptr[-1] == ' ') {prepost |= kPreSpaceIndicator;}
  if (word_ptr[bytecount] == ' ') {prepost |= kPostSpaceIndicator;}
  switch ((bytecount - 1) >> 2) {
  case 0:       // 1..4 bytes
    word0 = word_ptr32[0] & kWordMask0[bytecount & 3];
    sum = word0;
    word0 = word0 ^ (word0 >> 3);
    break;
  case 1:       // 5..8 bytes
    word0 = word_ptr32[0];
    sum = word0;
    word0 = word0 ^ (word0 >> 3);
    word1 = word_ptr32[1] & kWordMask0[bytecount & 3];
    sum += word1;
    word1 = word1 ^ (word1 << 4);
    word0 += word1;
    break;
  case 2:       // 9..12 bytes
    word0 = word_ptr32[0];
    sum = word0;
    word0 = word0 ^ (word0 >> 3);
    word1 = word_ptr32[1];
    sum += word1;
    word1 = word1 ^ (word1 << 4);
    word0 += word1;
    word1 = word_ptr32[2] & kWordMask0[bytecount & 3];
    sum += word1;
    word1 = word1 ^ (word1 << 2);
    word0 += word1;
    break;
  case 3:       // 13..16 bytes
    word0 = word_ptr32[0];
    sum = word0;
    word0 = word0 ^ (word0 >> 3);
    word1 = word_ptr32[1];
    sum += word1;
    word1 = word1 ^ (word1 << 4);
    word0 += word1;
    word1 = word_ptr32[2];
    sum += word1;
    word1 = word1 ^ (word1 << 2);
    word0 += word1;
    word1 = word_ptr32[3] & kWordMask0[bytecount & 3];
    sum += word1;
    word1 = word1 ^ (word1 >> 8);
    word0 += word1;
    break;
  case 4:       // 17..20 bytes
    word0 = word_ptr32[0];
    sum = word0;
    word0 = word0 ^ (word0 >> 3);
    word1 = word_ptr32[1];
    sum += word1;
    word1 = word1 ^ (word1 << 4);
    word0 += word1;
    word1 = word_ptr32[2];
    sum += word1;
    word1 = word1 ^ (word1 << 2);
    word0 += word1;
    word1 = word_ptr32[3];
    sum += word1;
    word1 = word1 ^ (word1 >> 8);
    word0 += word1;
    word1 = word_ptr32[4] & kWordMask0[bytecount & 3];
    sum += word1;
    word1 = word1 ^ (word1 >> 4);
    word0 += word1;
    break;
  default:      // 21..24 bytes and higher (ignores beyond 24)
    word0 = word_ptr32[0];
    sum = word0;
    word0 = word0 ^ (word0 >> 3);
    word1 = word_ptr32[1];
    sum += word1;
    word1 = word1 ^ (word1 << 4);
    word0 += word1;
    word1 = word_ptr32[2];
    sum += word1;
    word1 = word1 ^ (word1 << 2);
    word0 += word1;
    word1 = word_ptr32[3];
    sum += word1;
    word1 = word1 ^ (word1 >> 8);
    word0 += word1;
    word1 = word_ptr32[4];
    sum += word1;
    word1 = word1 ^ (word1 >> 4);
    word0 += word1;
    word1 = word_ptr32[5] & kWordMask0[bytecount & 3];
    sum += word1;
    word1 = word1 ^ (word1 >> 6);
    word0 += word1;
    break;
  }

  sum += (sum >> 17);             // extra 1-bit shift for bytes 2 & 3
  sum += (sum >> 9);              // extra 1-bit shift for bytes 1 & 3
  sum = (sum & 0xff) << 32;
  return (word0 ^ prepost) + sum;
}

// OCTAGRAM wrapper with surrounding spaces
// Pick up 1..24 bytes plus pre/post space and hash them via mask/shift/add
// UNDERSHOOTS 1 byte, OVERSHOOTS up to 3 bytes
//
// The low 32 bits follow the pattern from above, tuned to different scripts
// The high 8 bits are a simple sum of all bytes, shifted by 0/1/2/3 bits each
// For runtime use of tables V3
uint64 cld::OctaHash40(const char* word_ptr, int bytecount) {
  uint64 prepost = 0;
  if (word_ptr[-1] == ' ') {prepost |= kPreSpaceIndicator;}
  if (word_ptr[bytecount] == ' ') {prepost |= kPostSpaceIndicator;}
  return OctaHash40Mix(word_ptr, bytecount, prepost);
}


// OCTAGRAM wrapper with surrounding underscores (offline use)
// Pick up 1..24 bytes plus pre/post space and hash them via mask/shift/add
// UNDERSHOOTS 1 byte, OVERSHOOTS up to 3 bytes
//
// The low 32 bits follow the pattern from above, tuned to different scripts
// The high 8 bits are a simple sum of all bytes, shifted by 0/1/2/3 bits each
// For offline construction of tables
uint64 cld::OctaHash40underscore(const char* word_ptr, int bytecount) {
  const char* local_word_ptr = word_ptr;
  int local_bytecount = bytecount;
  uint64 prepost = 0;
  if (local_word_ptr[0] == '_') {
    prepost |= kPreSpaceIndicator;
    ++local_word_ptr;
    --local_bytecount;
  }
  if (local_word_ptr[local_bytecount - 1] == '_') {
    prepost |= kPostSpaceIndicator;
    --local_bytecount;
  }
  return OctaHash40Mix(local_word_ptr, local_bytecount, prepost);
}




//------------------------------------------------------------------------------
// Scoring single groups of letters
//------------------------------------------------------------------------------

// UNIGRAM score one => tote
// Input: 1-byte entry of subscript into unigram probs, plus
//  an accumulator tote.
// Output: running sums in tote updated
void cld::ProcessProbV25UniTote(int propval, Tote* tote) {
  tote->AddGram();
  const UnigramProbArray* pa = &kTargetCTJKVZProbs[propval];
  if (pa->probs[0] > 0) {tote->Add(cld::PackLanguage(CHINESE), pa->probs[0]);}
  if (pa->probs[1] > 0) {tote->Add(cld::PackLanguage(CHINESE_T), pa->probs[1]);}
  if (pa->probs[2] > 0) {tote->Add(cld::PackLanguage(JAPANESE), pa->probs[2]);}
  if (pa->probs[3] > 0) {tote->Add(cld::PackLanguage(KOREAN), pa->probs[3]);}
  if (pa->probs[4] > 0) {tote->Add(cld::PackLanguage(VIETNAMESE), pa->probs[4]);}
  if (pa->probs[5] > 0) {tote->Add(cld::PackLanguage(ZHUANG), pa->probs[5]);}
}

// BIGRAM, QUADGRAM, OCTAGRAM score one => tote
// Input: 4-byte entry of 3 language numbers and one probability subscript, plus
//  an accumulator tote. (language 0 means unused entry)
// Output: running sums in tote updated
void cld::ProcessProbV25Tote(uint32 probs, Tote* tote) {
  tote->AddGram();
  uint8 prob123 = (probs >> 0) & 0xff;
  const uint8* prob123_entry = cld::LgProb2TblEntry(prob123);

  uint8 top1 = (probs >> 8) & 0xff;
  if (top1 > 0) {tote->Add(top1, cld::LgProb3(prob123_entry, 0));}
  uint8 top2 = (probs >> 16) & 0xff;
  if (top2 > 0) {tote->Add(top2, cld::LgProb3(prob123_entry, 1));}
  uint8 top3 = (probs >> 24) & 0xff;
  if (top3 > 0) {tote->Add(top3, cld::LgProb3(prob123_entry, 2));}
}


//------------------------------------------------------------------------------
// Routines to accumulate probabilities
//------------------------------------------------------------------------------


// UNIGRAM, using UTF-8 property table, advancing by 1/2/4/8 chars
// Caller supplies table, such as compact_lang_det_generated_ctjkvz_b1_obj
// Score up to n unigrams, returning number of bytes consumed
// Updates tote_grams
int cld::DoUniScoreV3(const UTF8PropObj* unigram_obj,
                      const char* isrc, int srclen, int advance_by,
                      int* tote_grams, int gram_limit, Tote* chunk_tote) {
  const char* src = isrc;
  if (FLAGS_dbgscore) {DbgScoreInit(src, srclen);}

  // Property-based CJK unigram lookup
  if (src[0] == ' ') {++src; --srclen;}

  const uint8* usrc = reinterpret_cast<const uint8*>(src);
  int usrclen = srclen;

  while (usrclen > 0) {
    int len = kAdvanceOneChar[usrc[0]];
    // Look up property of one UTF-8 character and advance over it
    // Return 0 if input length is zero
    // Return 0 and advance one byte if input is ill-formed

    int propval = UTF8GenericPropertyBigOneByte(unigram_obj, &usrc, &usrclen);

    if (FLAGS_dbglookup) {
      DbgUniTermToStderr(propval, usrc, len);
    }

    if (propval > 0) {
      ProcessProbV25UniTote(propval, chunk_tote);
      ++(*tote_grams);
      if (FLAGS_dbgscore) {DbgScoreRecordUni((const char*)usrc, propval, len);}
    }

    // Advance by 1/2/4/8 characters (half of quad advance)
    if (advance_by == 2) {
      // Already advanced by 1
    } else if (advance_by == 4) {
      // Advance by 2 chars total, if not at end
      if (UTFmax <= usrclen) {
        int n = kAdvanceOneChar[*usrc]; usrc += n; usrclen -= n;
      }
    } else if (advance_by == 8) {
      // Advance by 4 chars total, if not at end
      if ((UTFmax * 3) <= usrclen) {
        int n = kAdvanceOneChar[*usrc]; usrc += n; usrclen -= n;
        n = kAdvanceOneChar[*usrc]; usrc += n; usrclen -= n;
        n = kAdvanceOneChar[*usrc]; usrc += n; usrclen -= n;
      }
    } else {
      // Advance by 8 chars total, if not at end
      if ((UTFmax * 7) <= usrclen) {
        int n = kAdvanceOneChar[*usrc]; usrc += n; usrclen -= n;
        n = kAdvanceOneChar[*usrc]; usrc += n; usrclen -= n;
        n = kAdvanceOneChar[*usrc]; usrc += n; usrclen -= n;
        n = kAdvanceOneChar[*usrc]; usrc += n; usrclen -= n;
        n = kAdvanceOneChar[*usrc]; usrc += n; usrclen -= n;
        n = kAdvanceOneChar[*usrc]; usrc += n; usrclen -= n;
        n = kAdvanceOneChar[*usrc]; usrc += n; usrclen -= n;
      }
    }
    DCHECK(usrclen >= 0);

    if (*tote_grams >= gram_limit) {
      break;
    }
  }
  if (FLAGS_dbgscore) {
    // With advance_by>2, we consume more input to get the same number of quads
    int len = src - isrc;
    DbgScoreTop(src, (len * 2) / advance_by, chunk_tote);
    DbgScoreFlush();
  }

  int consumed2 = reinterpret_cast<const char*>(usrc) - isrc;
  return consumed2;
}


// BIGRAM, using hash table, always advancing by 1 char
// Caller supplies table, such as &kCjkBiTable_obj or &kGibberishTable_obj
// Score all bigrams in isrc, using languages that have bigrams (CJK)
// Return number of bigrams that hit in the hash table
int cld::DoBigramScoreV3(const cld::CLDTableSummary* bigram_obj,
                         const char* isrc, int srclen, Tote* chunk_tote) {
  int hit_count = 0;
  const char* src = isrc;

  // Hashtable-based CJK bigram lookup
  const uint8* usrc = reinterpret_cast<const uint8*>(src);
  const uint8* usrclimit1 = usrc + srclen - UTFmax;
  if (FLAGS_dbgscore) {
    fprintf(stderr, "  " );
  }

  while (usrc < usrclimit1) {
    int len = kAdvanceOneChar[usrc[0]];
    int len2 = kAdvanceOneChar[usrc[len]] + len;

    if ((kMinCJKUTF8CharBytes * 2) <= len2) {      // Two CJK chars possible
      // Lookup and score this bigram
      // Always ignore pre/post spaces
      uint32 bihash = BiHashV25(reinterpret_cast<const char*>(usrc), len2);
      uint32 probs = QuadHashV3Lookup4(bigram_obj, bihash);
      // Now go indirect on the subscript
      probs = bigram_obj->kCLDTableInd[probs &
        ~bigram_obj->kCLDTableKeyMask];

      // Process the bigram
      if (FLAGS_dbglookup) {
        const char* ssrc = reinterpret_cast<const char*>(usrc);
        DbgBiTermToStderr(bihash, probs, ssrc, len2);
        DbgScoreRecord(NULL, probs, len2);
      } else if (FLAGS_dbgscore && (probs != 0)) {
        const char* ssrc = reinterpret_cast<const char*>(usrc);
        DbgScoreRecord(NULL, probs, len2);
        string temp(ssrc, len2);
        fprintf(stderr, "%s ", temp.c_str());
      }

      if (probs != 0) {
        ProcessProbV25Tote(probs, chunk_tote);
        ++hit_count;
      }
    }
    usrc += len;  // Advance by one char
  }

  if (FLAGS_dbgscore) {
    fprintf(stderr, "[%d bigrams scored]\n", hit_count);
    DbgScoreState();
  }
  return hit_count;
}



// QUADGRAM, using hash table, advancing by 2/4/8/16 chars
// Caller supplies table, such as &kQuadTable_obj or &kGibberishTable_obj
// Score up to n quadgrams, returning number of bytes consumed
// Updates tote_grams
int cld::DoQuadScoreV3(const cld::CLDTableSummary* quadgram_obj,
                       const char* isrc, int srclen, int advance_by,
                       int* tote_grams, int gram_limit, Tote* chunk_tote) {
  const char* src = isrc;
  const char* srclimit = src + srclen;
  // Limit is end, which has extra 20 20 20 00 past len
  const char* srclimit7 = src + srclen - (UTFmax * 7);
  const char* srclimit15 = src + srclen - (UTFmax * 15);

  if (FLAGS_dbgscore) {DbgScoreInit(src, srclen);}

  // Visit all quadgrams
  if (src[0] == ' ') {++src;}
  while (src < srclimit) {
    // Find one quadgram
    const char* src_end = src;
    src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
    src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
    const char* src_mid = src_end;
    src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
    src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
    int len = src_end - src;

    // Lookup and score this quadgram
    uint32 quadhash = QuadHashV25(src, len);
    uint32 probs = QuadHashV3Lookup4(quadgram_obj, quadhash);
    // Now go indirect on the subscript
    probs = quadgram_obj->kCLDTableInd[probs &
      ~quadgram_obj->kCLDTableKeyMask];

    // Process the quadgram
    if (FLAGS_dbglookup) {
      DbgQuadTermToStderr(quadhash, probs, src, len);
    }
    if (probs != 0) {
      ProcessProbV25Tote(probs, chunk_tote);
      ++(*tote_grams);
      if (FLAGS_dbgscore) {DbgScoreRecord(src, probs, len);}
    }

    // Advance all the way past word if at end-of-word
    if (src_end[0] == ' ') {
      src_mid = src_end;
    }

    // Advance by 2/4/8/16 characters
    if (advance_by == 2) {
      src = src_mid;
    } else if (advance_by == 4) {
      src = src_end;
    } else if (advance_by == 8) {
      // Advance by 8 chars total (4 more), if not at end
      if (src < srclimit7) {
        src_end += kAdvanceOneChar[(uint8)src_end[0]];
        src_end += kAdvanceOneChar[(uint8)src_end[0]];
        src_end += kAdvanceOneChar[(uint8)src_end[0]];
        src_end += kAdvanceOneChar[(uint8)src_end[0]];
      }
      src = src_end;
    } else {
      // Advance by 16 chars total (12 more), if not at end
      if (src < srclimit15) {
        // Advance by ~16 chars by adding 3 * current bytelen
        int fourcharlen = src_end - src;
        src = src_end + (3 * fourcharlen);
        // Advance a bit more if mid-character
        src += kAdvanceOneCharSpaceVowel[(uint8)src[0]];
        src += kAdvanceOneCharSpaceVowel[(uint8)src[0]];
      } else {
        src = src_end;
      }
    }
    DCHECK(src < srclimit);
    src += kAdvanceOneCharSpaceVowel[(uint8)src[0]];

    if (*tote_grams >= gram_limit) {
      break;
    }
  }

  if (FLAGS_dbgscore) {
    // With advance_by>2, we consume more input to get the same number of quads
    int len = src - isrc;
    DbgScoreTop(src, (len * 2) / advance_by, chunk_tote);
    DbgScoreFlush();
  }

  int consumed = src - isrc;

  // If advancing by more than 2, src may have overshot srclimit
  if (consumed > srclen) {
    consumed = srclen;
  }

  return consumed;
}


// OCTAGRAM, using hash table, always advancing by 1 word
// Caller supplies table, such as &kLongWord8Table_obj
// Score all words in isrc, using languages that have quadgrams
// We don't normally use this routine except on the first quadgram run,
// but it can be used to resolve unreliable pages.
// This routine does not have an optimized advance_by
// SOON: Uses indirect language/probability longword
//
// Return number of words that hit in the hash table
int cld::DoOctaScoreV3(const cld::CLDTableSummary* octagram_obj,
                       const char* isrc, int srclen, Tote* chunk_tote) {
  int hit_count = 0;
  const char* src = isrc;
  const char* srclimit = src + srclen + 1;
  // Limit is end+1, to include extra space char (0x20) off the end
  //
  // Score all words truncated to 8 characters
  int charcount = 0;
  // Skip any initial space
  if (src[0] == ' ') {++src;}
  const char* word_ptr = src;
  const char* word_end = word_ptr;
  if (FLAGS_dbgscore) {
    fprintf(stderr, "  " );
  }
  while (src < srclimit) {
    // Terminate previous word or continue current word
    if (src[0] == ' ') {
      int bytecount = word_end - word_ptr;
      // Lookup and score this word
      uint64 wordhash40 = OctaHash40(word_ptr, bytecount);
      uint32 probs = OctaHashV3Lookup4(octagram_obj, wordhash40);
      // Now go indirect on the subscript
      probs = octagram_obj->kCLDTableInd[probs &
        ~octagram_obj->kCLDTableKeyMask];

      // // Lookup and score this word
      // uint32 wordhash = QuadHashV25(word_ptr, bytecount);
      // uint32 probs = WordHashLookup4(wordhash, kLongWord8Table,
      //                                kLongWord8TableSize);
      //
      if (FLAGS_dbglookup) {
        DbgWordTermToStderr(wordhash40, probs, word_ptr, bytecount);
        DbgScoreRecord(NULL, probs, bytecount);
      } else if (FLAGS_dbgscore && (probs != 0)) {
        DbgScoreRecord(NULL, probs, bytecount);
        string temp(word_ptr, bytecount);
        fprintf(stderr, "%s ", temp.c_str());
      }

      if (probs != 0) {
        ProcessProbV25Tote(probs, chunk_tote);
        ++hit_count;
      }
      charcount = 0;
      word_ptr = src + 1;   // Over the space
      word_end = word_ptr;
    } else {
      ++charcount;
    }

    // Advance to next char
    src += cld_UniLib::OneCharLen(src);
    if (charcount <= 8) {
      word_end = src;
    }
  }

  if (FLAGS_dbgscore) {
    fprintf(stderr, "[%d words scored]\n", hit_count);
    DbgScoreState();
  }
  return hit_count;
}



//------------------------------------------------------------------------------
// Reliability calculations, for single language and between languages
//------------------------------------------------------------------------------

// Return reliablity of result 0..100 for top two scores
// delta==0 is 0% reliable, delta==fully_reliable_thresh is 100% reliable
// (on a scale where +1 is a factor of  2 ** 1.6 = 3.02)
// Threshold is uni/quadgram increment count, bounded above and below.
//
// Requiring a factor of 3 improvement (e.g. +1 log base 3)
// for each scored quadgram is too stringent, so I've backed this off to a
// factor of 2 (e.g. +5/8 log base 3).
//
// I also somewhat lowered the Min/MaxGramCount limits above
//
// Added: if fewer than 8 quads/unis, max reliability is 12*n percent
//
int cld::ReliabilityDelta(int value1, int value2, int gramcount) {
  int max_reliability_percent = 100;
  if (gramcount < 8) {
    max_reliability_percent = 12 * gramcount;
  }
  int fully_reliable_thresh = (gramcount * 5) >> 3;     // see note above
  if (fully_reliable_thresh < kMinGramCount) {          // Fully = 3..16
    fully_reliable_thresh = kMinGramCount;
  } else if (fully_reliable_thresh > kMaxGramCount) {
    fully_reliable_thresh = kMaxGramCount;
  }

  int delta = value1 - value2;
  if (delta >= fully_reliable_thresh) {return max_reliability_percent;}
  if (delta <= 0) {return 0;}
  return cld::minint(max_reliability_percent,
                     (100 * delta) / fully_reliable_thresh);
}

// Return reliablity of result 0..100 for top score vs. mainsteam score
// Values are score per 1024 bytes of input
// ratio = max(top/mainstream, mainstream/top)
// ratio > 4.0 is 0% reliable, <= 2.0 is 100% reliable
// Change: short-text word scoring can give unusually good results.
//  Let top exceed mainstream by 4x at 50% reliable
int cld::ReliabilityMainstream(int topscore, int len, int mean_score) {
  if (mean_score == 0) {return 100;}    // No reliability data available yet
  if (topscore == 0) {return 0;}        // zero score = unreliable
  if (len == 0) {return 0;}             // zero len = unreliable
  int top_kb = (topscore << 10) / len;
  double ratio;
  double ratio_cutoff;
  if (top_kb > mean_score) {
    ratio = (1.0 * top_kb) / mean_score;
    ratio_cutoff = 5.0;                 // ramp down from 100% to 0%: 3.0-5.0
  } else {
    ratio = (1.0 * mean_score) / top_kb;
    ratio_cutoff = 4.0;                 // ramp down from 100% to 0%: 2.0-4.0
  }
  if (ratio <= ratio_cutoff - 2.0) {return 100;}
  if (ratio > ratio_cutoff) {return 0;}

  int iratio = static_cast<int>(100 * (ratio_cutoff - ratio) / 2.0);
  return iratio;
}

// Calculate ratio of score per 1KB vs. expected score per 1KB
double cld::GetNormalizedScore(Language lang, UnicodeLScript lscript,
                          int bytes, int score) {
  // Average training-data score for this language-script combo, per 1KB
  int expected_score = kMeanScore[lang * 4 + LScript4(lscript)];
  if (lscript == ULScript_Common) {
    // We don't know the script (only happens with second-chance score)
    // Look for first non-zero mean value
    for (int i = 0; i < 3; ++i) {
      if (kMeanScore[lang * 4 + i] > 0) {
        expected_score = kMeanScore[lang * 4 + i];
      }
    }
  }
  if (expected_score < 100) {
      expected_score = 1000;
  }

  // Our score per 1KB
  double our_score = (score << 10) / (bytes ? bytes : 1);  // Avoid zdiv
  double ratio = our_score / expected_score;

  // Just the raw count normalized as though each language has mean=1000;
  ratio = (score * 1000.0) /  expected_score;
  return ratio;
}

// Calculate reliablity of len bytes of script lscript with chunk_tote
int cld::GetReliability(int len, UnicodeLScript lscript,
                   const Tote* chunk_tote) {
  Language cur_lang = UnpackLanguage(chunk_tote->Key(0));
  // Average score for this language-script combo
  int mean_score = kMeanScore[cur_lang * 4 + LScript4(lscript)];
  if (lscript == ULScript_Common) {
    // We don't know the script (only happens with second-chance score)
    // Look for first non-zero mean value
    for (int i = 0; i < 3; ++i) {
      if (kMeanScore[cur_lang * 4 + i] > 0) {
        mean_score = kMeanScore[cur_lang * 4 + i];
      }
    }
  }
  int reliability_delta = ReliabilityDelta(chunk_tote->Value(0),
                                           chunk_tote->Value(1),
                                           chunk_tote->GetGramCount());

  int reliability_main = ReliabilityMainstream(chunk_tote->Value(0),
                                               len,
                                               mean_score);

  int reliability_min = minint(reliability_delta, reliability_main);


  if (FLAGS_dbgreli) {
    char temp1[4];
    char temp2[4];
    cld::DbgLangName3(UnpackLanguage(chunk_tote->Key(0)), temp1);
    if (temp1[2] == ' ') {temp1[2] = '\0';}
    cld::DbgLangName3(UnpackLanguage(chunk_tote->Key(1)), temp2);
    if (temp2[2] == ' ') {temp2[2] = '\0';}
    int srclen = len;
    fprintf(stderr, "CALC GetReliability gram=%d incr=%d srclen=%d,  %s=%d %s=%d "
                   "top/KB=%d mean/KB=%d del=%d%% reli=%d%%   "
                   "lang/lscript %d %d\n",
           chunk_tote->GetGramCount(),
           chunk_tote->GetIncrCount(),
           srclen,
           temp1, chunk_tote->Value(0),
           temp2, chunk_tote->Value(1),
           (chunk_tote->Value(0) << 10) / (srclen ? srclen : 1),
           mean_score,
           reliability_delta,
           reliability_main,
           cur_lang, lscript);
  }

  return reliability_min;
}


//------------------------------------------------------------------------------
// Miscellaneous
//------------------------------------------------------------------------------

// Demote all languages except Top40 and plus_one
// Do this just before sorting chunk_tote results
void cld::DemoteNotTop40(Tote* chunk_tote, int packed_plus_one) {
  for (int sub = 0; sub < chunk_tote->MaxSize(); ++sub) {
    if (chunk_tote->Key(sub) == 0) continue;
    if (chunk_tote->Key(sub) == packed_plus_one) continue;
    if (kIsPackedTop40[chunk_tote->Key(sub)]) continue;
    // Quarter the score of others
    chunk_tote->SetValue(sub, chunk_tote->Value(sub) >> 2);
  }
}
