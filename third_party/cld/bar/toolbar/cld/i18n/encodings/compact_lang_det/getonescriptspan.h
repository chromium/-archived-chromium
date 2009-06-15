// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef I18N_ENCODINGS_COMPACT_LANG_DET_GETONESCRIPTSPAN_H_
#define I18N_ENCODINGS_COMPACT_LANG_DET_GETONESCRIPTSPAN_H_

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/letterscript_enum.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det_impl.h"

namespace getone {
  static const int kMaxScriptBuffer = 4096;
  static const int kMaxScriptLowerBuffer = (kMaxScriptBuffer * 3) / 2;
  static const int kMaxScriptBytes = kMaxScriptBuffer- 8;   // Leave some room
  static const int kMaxAnswerBuffer = 256;

  typedef enum UnicodeLScript ULScript;

  typedef struct {
    char* text;             // Pointer to the span, somewhere
    int text_bytes;         // Number of bytes of text in the span
    int offset;             // Offset of start of span in original input buffer
    ULScript script;        // Script of all the letters in this span
    Language lang;          // Language identified for this span
    bool truncated;         // true if buffer filled up before a
                            // different script or EOF was found
  } LangSpan;


  static inline bool IsContinuationByte(char c) {
    return static_cast<signed char>(c) < -64;
  }

  // Gets lscript number for letters; always returns
  //   0 (common script) for non-letters
  int GetUTF8LetterScriptNum(const char* src);


  // Update src pointer to point to next quadgram, +2..+5
  // Looks at src[0..4]
  const char* AdvanceQuad(const char* src);
}     // end namespace getone






class ScriptScanner {
 public:
  ScriptScanner(const char* buffer, int buffer_length, bool is_plain_text);
  ~ScriptScanner();

  // Copy next run of same-script non-tag letters to buffer [NUL terminated]
  bool GetOneScriptSpan(getone::LangSpan* span);

  // Force Latin and Cyrillic scripts to be lowercase
  void LowerScriptSpan(getone::LangSpan* span);

  // Copy next run of same-script non-tag letters to buffer [NUL terminated]
  // Force Latin and Cyrillic scripts to be lowercase
  bool GetOneScriptSpanLower(getone::LangSpan* span);

 private:
  int SkipToFrontOfSpan(const char* src, int len, int* script);

  const char* start_byte_;
  const char* next_byte_;
  const char* next_byte_limit_;
  int byte_length_;
  bool is_plain_text_;
  char* script_buffer_;           // Holds text with expanded entities
  char* script_buffer_lower_;     // Holds lowercased text
};


class LangScanner {
 public:
  LangScanner(const CompactLangDetImpl::LangDetObj* langdetobj,
              getone::LangSpan* spn, int smoothwidth, int smoothcandidates,
              int maxlangs, int minlangspan);
  ~LangScanner();


  int script() {return script_;}

  // Use new text
  // Keep smoothing state if same script, otherwise reinit smoothing
  void NewText(getone::LangSpan* spn);

  bool GetOneShortLangSpanBoot(getone::LangSpan* span);  // Just for bootstrapping
  bool GetOneLangSpanBoot(getone::LangSpan* span);       // Just for bootstrapping

  // The real ones
  bool GetOneShortLangSpan(const CompactLangDetImpl::LangDetObj* langdetobj,
                           getone::LangSpan* span);
  bool GetOneLangSpan(const CompactLangDetImpl::LangDetObj* langdetobj,
                      getone::LangSpan* span);

  // Increases language bias by delta
  void SetLanguageBias(const CompactLangDetImpl::LangDetObj* langdetobj,
                       Language key, int delta);

  // For debugging output
  int next_answer_;
  char answer_buffer_[getone::kMaxAnswerBuffer];
  char answer_buffer2_[getone::kMaxAnswerBuffer];
  char answer_buffer3_[getone::kMaxAnswerBuffer];
  char answer_buffer4_[getone::kMaxAnswerBuffer];

 private:
  const char* start_byte_;
  const char* next_byte_limit_;
  const char* next_byte_;
  const char* onelangspan_begin_;
  int byte_length_;
  int script_;
  Language spanlang_;
  int smoothwidth_;
  int smoothwidth_2_;
  int smoothcandidates_;
  int maxlangs_;
  int minlangspan_;
  int rb_size_;
  int next_rb_;
  int rb_mask_;
  uint32* rb_;
  int* offset_rb_;
};

#endif  // I18N_ENCODINGS_COMPACT_LANG_DET_GETONESCRIPTSPAN_H_
