// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/tote.h"
#include <string.h>     // memset

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_logging.h"


// Take a set of <key, value> pairs and tote them up.
// After explicitly sorting, retrieve top key, value pairs
Tote::Tote() {
  gram_count_ = 0;
  incr_count_ = 0;
  byte_count_ = 0;
  memset(key_, 0, sizeof(key_));
  // No need to initialize values
}

Tote::~Tote() {
}

void Tote::Reinit() {
  gram_count_ = 0;
  incr_count_ = 0;
  byte_count_ = 0;
  memset(key_, 0, sizeof(key_));
  // No need to initialize values
}

// Increment count of quadgrams/trigrams/unigrams scored
void Tote::AddGram() {
  ++gram_count_;
}

// Three-way associative, guaranteeing that the largest two counts are always
// in the data structure. kMaxSize must be a multiple of 3, and is tied to the
// subscript calculations here, which are for 8 sets of 3-way associative
// buckets. The subscripts for set N are [N], [N+8], and [N+16] used in a
// slightly-weird way: The initial probe point is [N] or [N+8], whichever
// is specified by key mod 16. In most cases (nearly *all* cases except Latin
// script), this entry matches and we update/return. The second probe is
// the other of [N] and [N+8]. The third probe is only used as a fallback to
// these two, and is there only for the rare case that there are three or more
// languages with Language enum values equal mod 8, contending within the same
// bucket. This can only happen in Latin and (rarely) Cyrillic scripts, because
// the other scripts have fewer than 17 languages total.
// If you change kMaxSize, change the constants 7/8/15/16 below
void Tote::Add(uint8 ikey, int idelta) {
  DCHECK(ikey != 0);
  ++incr_count_;

  // Look for existing entry
  int sub0 = ikey & 15;
  if (key_[sub0] == ikey) {
    value_[sub0] += idelta;
    return;
  }
  int sub1 = sub0 ^ 8;
  if (key_[sub1] == ikey) {
    value_[sub1] += idelta;
    return;
  }
  int sub2 = (ikey & 7) + 16;
  if (key_[sub2] == ikey) {
    value_[sub2] += idelta;
    return;
  }

  // Allocate new entry
  int alloc = -1;
  if (key_[sub0] == 0) {
    alloc = sub0;
  } else if (key_[sub1] == 0) {
    alloc = sub1;
  } else if (key_[sub2] == 0) {
    alloc = sub2;
  } else {
    // All choices allocated, need to replace smallest one
    alloc = sub0;
    if (value_[sub1] < value_[alloc]) {alloc = sub1;}
    if (value_[sub2] < value_[alloc]) {alloc = sub2;}
  }
  key_[alloc] = ikey;
  value_[alloc] = idelta;
  return;
}

// Return current top key
int Tote::CurrentTopKey() {
  int top_key = 0;
  int top_value = -1;
  for (int sub = 0; sub < kMaxSize_; ++sub) {
    if (key_[sub] == 0) {continue;}
    if (top_value < value_[sub]) {
      top_value = value_[sub];
      top_key = key_[sub];
    }
  }
  return top_key;
}


// Sort first n entries by decreasing order of value
// If key==0 other fields are not valid, treat value as -1
void Tote::Sort(int n) {
  // This is n**2, but n is small
  for (int sub = 0; sub < n; ++sub) {
    if (key_[sub] == 0) {value_[sub] = -1;}

    // Bubble sort key[sub] and entry[sub]
    for (int sub2 = sub + 1; sub2 < kMaxSize_; ++sub2) {
      if (key_[sub2] == 0) {value_[sub2] = -1;}
      if (value_[sub] < value_[sub2]) {
        // swap
        uint8 tmpk = key_[sub];
        key_[sub] = key_[sub2];
        key_[sub2] = tmpk;
        int tmpv = value_[sub];
        value_[sub] = value_[sub2];
        value_[sub2] = tmpv;
      }
    }
  }
}

void Tote::Dump(FILE* f) {
  for (int sub = 0; sub < kMaxSize_; ++sub) {
    if (key_[sub] > 0) {
      fprintf(f, "[%2d] %3d %8d\n", sub, key_[sub], value_[sub]);
    }
  }
  fprintf(f, "%d %d %d\n", gram_count_, incr_count_, byte_count_);
}




// Take a set of <key, value> pairs and tote them up.
// After explicitly sorting, retrieve top key, value pairs
ToteWithReliability::ToteWithReliability() {
  // No need to initialize score_ or value_
  incr_count_ = 0;
  sorted_ = 0;
  memset(closepair_, 0, sizeof(closepair_));
  memset(key_, 0, sizeof(key_));
}

ToteWithReliability::~ToteWithReliability() {
}

void ToteWithReliability::Reinit() {
  // No need to initialize score_ or value_
  incr_count_ = 0;
  sorted_ = 0;
  memset(closepair_, 0, sizeof(closepair_));
  memset(key_, 0, sizeof(key_));
  ////ss_.Init();
}

// Weight reliability by ibytes
// Also see three-way associative comments above for Tote
void ToteWithReliability::Add(uint8 ikey, int ibytes,
                              int score, int ireliability) {
  DCHECK(ikey != 0);
  CHECK(sorted_ == 0);
  ++incr_count_;

  // Look for existing entry
  int sub0 = ikey & 15;
  if (key_[sub0] == ikey) {
    value_[sub0] += ibytes;
    score_[sub0] += score;
    reliability_[sub0] += ireliability * ibytes;
    return;
  }
  int sub1 = sub0 ^ 8;
  if (key_[sub1] == ikey) {
    value_[sub1] += ibytes;
    score_[sub1] += score;
    reliability_[sub1] += ireliability * ibytes;
    return;
  }
  int sub2 = (ikey & 7) + 16;
  if (key_[sub2] == ikey) {
    value_[sub2] += ibytes;
    score_[sub2] += score;
    reliability_[sub2] += ireliability * ibytes;
    return;
  }

  // Allocate new entry
  int alloc = -1;
  if (key_[sub0] == 0) {
    alloc = sub0;
  } else if (key_[sub1] == 0) {
    alloc = sub1;
  } else if (key_[sub2] == 0) {
    alloc = sub2;
  } else {
    // All choices allocated, need to replace smallest one
    alloc = sub0;
    if (value_[sub1] < value_[alloc]) {alloc = sub1;}
    if (value_[sub2] < value_[alloc]) {alloc = sub2;}
  }
  key_[alloc] = ikey;
  value_[alloc] = ibytes;
  score_[alloc] = score;
  reliability_[alloc] = ireliability * ibytes;
  return;
}

// Find subscript of a given packed language, or -1
int ToteWithReliability::Find(uint8 ikey) {
  DCHECK(ikey != 0);

  if (sorted_) {
    // Linear search if sorted
    for (int sub = 0; sub < kMaxSize_; ++sub) {
      if (key_[sub] == ikey) {return sub;}
    }
    return -1;
  }

  // Look for existing entry
  int sub0 = ikey & 15;
  if (key_[sub0] == ikey) {
    return sub0;
  }
  int sub1 = sub0 ^ 8;
  if (key_[sub1] == ikey) {
    return sub1;
  }
  int sub2 = (ikey & 7) + 16;
  if (key_[sub2] == ikey) {
    return sub2;
  }

  return -1;
}

// Return current top key
int ToteWithReliability::CurrentTopKey() {
  int top_key = 0;
  int top_value = -1;
  for (int sub = 0; sub < kMaxSize_; ++sub) {
    if (key_[sub] == 0) {continue;}
    if (top_value < value_[sub]) {
      top_value = value_[sub];
      top_key = key_[sub];
    }
  }
  return top_key;
}


// Sort first n entries by decreasing order of value
// If key==0 other fields are not valid, treat value as -1
void ToteWithReliability::Sort(int n) {
  // This is n**2, but n is small
  for (int sub = 0; sub < n; ++sub) {
    if (key_[sub] == 0) {value_[sub] = -1;}

    // Bubble sort key[sub] and entry[sub]
    for (int sub2 = sub + 1; sub2 < kMaxSize_; ++sub2) {
      if (key_[sub2] == 0) {value_[sub2] = -1;}
      if (value_[sub] < value_[sub2]) {
        // swap
        uint8 tmpk = key_[sub];
        key_[sub] = key_[sub2];
        key_[sub2] = tmpk;

        int tmpv = value_[sub];
        value_[sub] = value_[sub2];
        value_[sub2] = tmpv;

        double tmps = score_[sub];
        score_[sub] = score_[sub2];
        score_[sub2] = tmps;

        int tmpr = reliability_[sub];
        reliability_[sub] = reliability_[sub2];
        reliability_[sub2] = tmpr;
      }
    }
  }
  sorted_ = 1;
}

void ToteWithReliability::Dump(FILE* f) {
  for (int sub = 0; sub < kMaxSize_; ++sub) {
    if (key_[sub] > 0) {
      fprintf(f, "[%2d] %3d %6d %5d %4d\n",
              sub, key_[sub], value_[sub], score_[sub], reliability_[sub]);
    }
  }
  fprintf(f, "  %d#\n", incr_count_);
}
