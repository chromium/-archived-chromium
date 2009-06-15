// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef I18N_ENCODINGS_COMPACT_LANG_DET_TOTE_H_
#define I18N_ENCODINGS_COMPACT_LANG_DET_TOTE_H_

#include <stdio.h>
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_basictypes.h"

// Take a set of <key, value> pairs and tote them up.
// After explicitly sorting, retrieve top key, value pairs
class Tote {
 public:
  Tote();
  ~Tote();
  void Reinit();
  void AddGram();
  void Add(uint8 ikey, int idelta);
  void AddBytes(int ibytes) {byte_count_ += ibytes;}
  int CurrentTopKey();
  void Sort(int n);
  void Dump(FILE* f);
  uint16 GetGramCount() const {return gram_count_;}
  uint16 GetIncrCount() const {return incr_count_;}
  int GetByteCount() const {return byte_count_;}
  int MaxSize() const {return kMaxSize_;}
  uint8 Key(int i) const {return key_[i];}
  int Value(int i) const {return value_[i];}
  void SetGramCount(uint16 v) {gram_count_ = v;}
  void SetIncrCount(uint16 v) {incr_count_ = v;}
  void SetKey(int i, int v) {key_[i] = v;}
  void SetValue(int i, int v) {value_[i] = v;}

 private:
  static const int kMaxSize_ = 24;
  uint16 gram_count_;       // Number of quadgrams/etc. scored
  uint16 incr_count_;       // Number of Add calls (1-3 per gram)
  int byte_count_;          // Bytes of text scored
  // Align at multiple of 8 bytes
  uint8 key_[kMaxSize_];    // Lang unassigned = 0, valid = 1..255
  int value_[kMaxSize_];    // Probability score sum
};


// Take a set of <key, value, reliability> triples and tote them up.
// After explicitly sorting, retrieve top key, value, reliability triples
class ToteWithReliability {
 public:
  ToteWithReliability();
  ~ToteWithReliability();
  void Reinit();
  void Add(uint8 ikey, int ibytes, int score, int ireliability);
  int Find(uint8 ikey);
  void AddClosePair(int subscr, int val) {closepair_[subscr] += val;}
  int CurrentTopKey();
  void Sort(int n);
  void Dump(FILE* f);

  ////void AddSeq(uint8 ikey) {ss_.Add(ikey);}
  ////void ExtractSeq(int n, uint8* dst) {ss_.Extract(n, dst);}

  int GetIncrCount() const {return incr_count_;}
  int GetClosePair(int subscr) const {return closepair_[subscr];}
  int MaxSize() const {return kMaxSize_;}
  uint8 Key(int i) const {return key_[i];}
  int Value(int i) const {return value_[i];}
  int Score(int i) const {return score_[i];}
  int Reliability(int i) const {return reliability_[i];}
  void SetKey(int i, int v) {key_[i] = v;}
  void SetValue(int i, int v) {value_[i] = v;}
  void SetScore(int i, int v) {score_[i] = v;}
  void SetReliability(int i, int v) {reliability_[i] = v;}

 private:
  static const int kMaxSize_ = 24;
  static const int kMaxClosePairSize_ = 8;
  int incr_count_;         // Number of Add calls
  int sorted_;             // Contents have been sorted, cannot Add
  // Align at multiple of 8 bytes
  int closepair_[kMaxClosePairSize_];
  uint8 key_[kMaxSize_];    // Lang unassigned = 0, valid = 1..255
  int value_[kMaxSize_];    // Bytecount this lang
  int score_[kMaxSize_];    // Probability score sum
  int reliability_[kMaxSize_];  // Percentage 0..100
  ////SubsetSequence ss_;
};

#endif  // I18N_ENCODINGS_COMPACT_LANG_DET_TOTE_H_
