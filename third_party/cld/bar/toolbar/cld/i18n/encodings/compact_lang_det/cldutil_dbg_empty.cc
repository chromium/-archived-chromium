// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/cldutil_dbg.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_commandlineflags.h"

DEFINE_bool(dbgscore, false, "Print picture of score calculation");
DEFINE_bool(dbglookup, false, "Print every quad/uni lookup in score calc");
DEFINE_bool(dbgreli, false, "Print reliability in score calc");

namespace cld {


//------------------------------------------------------------------------------
// Debugging. Not thread safe
// This is the empty version -- routines return immediately
//------------------------------------------------------------------------------

  void DbgScoreInit(const char* src, int len) {};

  // Return a 3-byte + NUL code for language
  void DbgLangName3(Language lang, char* temp) {};

  // Show all per-language totals
  void DbgScoreState() {};

  void DbgScoreTop(const char* src, int srclen, Tote* chunk_tote) {};

  void DbgScoreFlush() {};

  // Allow additional scoring debug output
  void DbgScoreRecord(const char* src, uint32 probs, int len) {};

  void DbgScoreRecordUni(const char* src, int propval, int len) {};

  // Debug print language name(s)
  void PrintLang(FILE* f, const Tote* chunk_tote,
                 const Language cur_lang, const bool cur_unreliable,
                 Language prior_lang, bool prior_unreliable) {};

  // Debug print language name(s)
  void PrintLang2(FILE* f,
                  const Language lang1, const Language lang2, bool diff_prior) {};

  // Debug print text span
  void PrintText(FILE* f, Language cur_lang, const string& str) {};

  // Debug print text span with speculative language
  void PrintTextSpeculative(FILE* f, Language cur_lang, const string& str) {};

  // Debug print ignored text span
  void PrintSkippedText(FILE* f, const string& str) {};

  void DbgProbsToStderr(uint32 probs) {};
  void DbgUniTermToStderr(int propval, const uint8* usrc, int len) {};
  // No pre/post space
  void DbgBiTermToStderr(uint32 bihash, uint32 probs,
                          const char* src, int len) {};
  void DbgQuadTermToStderr(uint32 quadhash, uint32 probs,
                          const char* src, int len) {};
  void DbgWordTermToStderr(uint64 wordhash, uint32 probs,
                          const char* src, int len) {};


}       // End namespace cld
