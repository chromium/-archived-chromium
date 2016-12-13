// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Remember a subset of a sequence of values, using a modest amount of memory

/***
  Design:
  Accumulate in powers of three, using 3-way median to collapse entries.
  At any given time, there is one most-dense (highest power of 3) range of
  entries and a series of less-dense ranges that hold 0..2 entries each. There
  is a bounded-size storage array of S cells for all the entries.

  The overflow detect is set up so that a new higher power of 3, K+1, is
  triggered precisely when range K has 3n entries and all ranges < K have
  zero entries.

  In general, think of the range sizes as a multi-digit base 3 number, except
  the highest digit may exceed 2:

  3**6  3**5  3**4  3**3  3**2  3**1  3**0  K=2
    0     0     0     0   3n-1    2     2   unused:1

  There are a total of 3n-1 + 2 + 2 entries in use. Assume a size limit S at
  one more than that, and we add a new 3**0 entry and "carry" by performing
  medians on any group of 3 elements:

  3**6  3**5  3**4  3**3  3**2  3**1  3**0       K=2
    0     0     0     0   3n-1    2     3        unused:0
    0     0     0     0   3n-1    3     0 carry  unused:2
    0     0     0     0    3n     0     0 carry  unused:4

  To accumulate 2 entries at all levels < K and 3 just before the first carry at
  level 0, we need 2*K + 1 unused cells after doing all carries, or five cells
  in this case. Since we only have 4 cells in the example above, we need to
  make room by starting a new power of three:

  3**6  3**5  3**4  3**3  3**2  3**1  3**0
    0     0     0     0    3n     0     0        K=2 unused:4
    0     0     0     n     0     0     0        K=3 unused:2n+4

  In the code below, we don't worry about overflow from the topmost place.


***/

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/subsetsequence.h"
#include <stdio.h>

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_logging.h"

void DumpInts(const char* label, const int* v, int n) {
  printf("%s ", label);
  for (int i = 0; i < n; ++i) {
    printf("%d ", v[i]);
  }
  printf("\n");
}

void DumpUint8s(const char* label, const uint8* v, int n) {
  printf("%s ", label);
  for (int i = 0; i < n; ++i) {
    printf("%d ", v[i]);
  }
  printf("\n");
}

// Return median of seq_[sub] .. seq_[sub+2], favoring middle element
uint8 SubsetSequence::Median3(int sub) {
  if (seq_[sub] == seq_[sub + 1]) {
    return seq_[sub];
  }
  if (seq_[sub] == seq_[sub + 2]) {
    return seq_[sub];
  }
  return seq_[sub + 1];
}

void SubsetSequence::Init() {
  // printf("Init\n");

  k_ = 0;
  count_[0] = 0;
  next_e_ = 0;
  seq_[0] = 0;    // Default value if no calls to Add

  // Want largest <= kMaxSeq_ that allows reserve and makes count_[k_] = 0 mod 3
  int reserve = (2 * k_ + 1);
  level_limit_e_ = kMaxSeq_ - reserve;
  level_limit_e_ = (level_limit_e_ / 3) * 3;  // Round down to multiple of 3
  limit_e_ = level_limit_e_;
}

// Compress level k by 3x, creating level k+1
void SubsetSequence::NewLevel() {
  // printf("NewLevel 3 ** %d\n", k_ + 1);
  //DumpUint8s("count[k]", count_, k_ + 1);
  //DumpUint8s("seq[next]", seq_, next_e_);

  // Incoming level must be an exact multiple of three in size
  CHECK((count_[k_] % 3) == 0);
  int k_size = count_[k_];
  int new_size = k_size / 3;

  // Compress down by 3x, via median
  for (int j = 0; j < new_size; ++j) {
    seq_[j] = Median3(j * 3);
  }

  // Update counts
  count_[k_] = 0;
  // Else Overflow -- just continue with 3x dense Level K
  if (k_ < (kMaxLevel_ - 1)) {++k_;}
  count_[k_] = new_size;

  // Update limits
  next_e_ = new_size;
  limit_e_ = next_e_ + 3;

  // Want largest <= kMaxSeq_ that allows reserve and makes count_[k_] = 0 mod 3
  int reserve = (2 * k_ + 1);
  level_limit_e_ = kMaxSeq_ - reserve;
  level_limit_e_ = (level_limit_e_ / 3) * 3;  // Round down to multiple of 3
                                              //
  //DumpUint8s("after: count[k]", count_, k_ + 1);
  //DumpUint8s("after: seq[next]", seq_, next_e_);
}

void SubsetSequence::DoCarries() {
  CHECK(count_[k_] > 3);    // We depend on count_[k_] being > 3 to stop while
  // Make room by carrying

  //DumpUint8s("DoCarries count[k]", count_, k_ + 1);
  //DumpUint8s("DoCarries seq[next]", seq_, next_e_);

  int i = 0;
  while (count_[i] == 3) {
    next_e_ -= 3;
    seq_[next_e_] = Median3(next_e_);
    ++next_e_;
    count_[i] = 0;
    ++count_[i + 1];
    ++i;
  }
  limit_e_ = next_e_ + 3;

  //DumpUint8s("after: DoCarries count[k]", count_, k_ + 1);
  //DumpUint8s("after: DoCarries seq[next]", seq_, next_e_);

  // If we just fully carried into level K,
  // Make sure there is now enough room, else start level K + 1
  if (i >= k_) {
    CHECK(count_[k_] == next_e_);
    if (next_e_ >= level_limit_e_) {
      NewLevel();
    }
  }
}

void SubsetSequence::Add(uint8 e) {
  // Add an entry then carry as needed
  seq_[next_e_] = e;
  ++next_e_;
  ++count_[0];

  if (next_e_ >= limit_e_) {
    DoCarries();
  }
}


// Collapse tail end by simple median across disparate-weight values,
// dropping or duplicating last value if need be.
// This routine is idempotent.
void SubsetSequence::Flush() {
  // printf("Flush %d\n", count_[k_]);
  int start_tail = count_[k_];
  int size_tail = next_e_ - start_tail;
  if ((size_tail % 3) == 2) {
    seq_[next_e_] = seq_[next_e_ - 1];      // Duplicate last value
    ++size_tail;
  }

  // Compress tail down by 3x, via median
  int new_size = size_tail / 3;             // May delete last value
  for (int j = 0; j < new_size; ++j) {
    seq_[start_tail + j] = Median3(start_tail + j * 3);
  }

  next_e_ = start_tail + new_size;
  count_[k_] = next_e_;
}


// Extract representative pattern of exactly N values into dst[0..n-1]
// This routine may be called multiple times, but it may downsample as a
// side effect, causing subsequent calls with larger N to get poor answers.
void SubsetSequence::Extract(int to_n, uint8* dst) {
  // Collapse partial-carries in tail
  Flush();

  // Just use Bresenham to resample
  int from_n = next_e_;
  if (to_n >= from_n) {
    // Up-sample from_n => to_n
    int err = to_n - 1;          // bias toward no overshoot
    int j = 0;
    for (int i = 0; i < to_n; ++i) {
      dst[i] = seq_[j];
      err -= from_n;
      if (err < 0) {
        ++j;
        err += to_n;
      }
    }
  } else {
    // Get to the point that the number of samples is <= 3 * to_n
    while (next_e_ > (to_n * 3)) {
      // Compress down by 3x, via median
      // printf("Extract, median %d / 3\n", next_e_);
      if ((next_e_ % 3) == 2) {
        seq_[next_e_] = seq_[next_e_ - 1];    // Duplicate last value
        ++next_e_;
      }
      int new_size = next_e_ / 3;             // May delete last value
      for (int j = 0; j < new_size; ++j) {
        seq_[j] = Median3(j * 3);
      }
      next_e_ = new_size;
      count_[k_] = next_e_;
    }
    from_n = next_e_;

    if (to_n == from_n) {
      // Copy verbatim
      for (int i = 0; i < to_n; ++i) {
        dst[i] = seq_[i];
      }
      return;
    }

    // Down-sample from_n => to_n, using medians
    int err = 0;          // Bias to immediate median sample
    int j = 0;
    for (int i = 0; i < from_n; ++i) {
      err -= to_n;
      if (err < 0) {
        if (i <= (next_e_ - 2)) {
          dst[j] = Median3(i);
        } else {
          dst[j] = seq_[i];
        }
        ++j;
        err += from_n;
      }
    }
  }

}
