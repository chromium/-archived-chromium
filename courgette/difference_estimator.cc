// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We want to measure the similarity of two sequences of bytes as a surrogate
// for measuring how well a second sequence will compress differentially to the
// first sequence.

#include "courgette/difference_estimator.h"

#include "base/hash_tables.h"

namespace courgette {

// Our difference measure is the number of k-tuples that occur in Subject that
// don't occur in Base.
const int kTupleSize = 4;

namespace {

COMPILE_ASSERT(kTupleSize >= 4 && kTupleSize <= 8, kTupleSize_between_4_and_8);

size_t HashTuple(const uint8* source) {
  size_t hash1 = *reinterpret_cast<const uint32*>(source);
  size_t hash2 = *reinterpret_cast<const uint32*>(source + kTupleSize - 4);
  size_t hash = ((hash1 * 17 + hash2 * 37) + (hash1 >> 17)) ^ (hash2 >> 23);
  return hash;
}

bool RegionsEqual(const Region& a, const Region& b) {
  if (a.length() != b.length())
    return false;
  return memcmp(a.start(), b.start(), a.length()) == 0;
}

}  // anonymous namepace

class DifferenceEstimator::Base {
 public:
  explicit Base(const Region& region) : region_(region) { }

  void Init() {
    const uint8* start = region_.start();
    const uint8* end = region_.end() - (kTupleSize - 1);
    for (const uint8* p = start;  p < end;  ++p) {
      size_t hash = HashTuple(p);
      hashes_.insert(hash);
    }
  }

  const Region& region() const { return region_; }

 private:
  Region region_;
  base::hash_set<size_t> hashes_;

  friend class DifferenceEstimator;
  DISALLOW_COPY_AND_ASSIGN(Base);
};

class DifferenceEstimator::Subject {
 public:
  explicit Subject(const Region& region) : region_(region) {}

  const Region& region() const { return region_; }

 private:
  Region region_;

  DISALLOW_COPY_AND_ASSIGN(Subject);
};

DifferenceEstimator::DifferenceEstimator() {}

DifferenceEstimator::~DifferenceEstimator() {
  for (size_t i = 0;  i < owned_bases_.size();  ++i)
    delete owned_bases_[i];
  for (size_t i = 0;  i < owned_subjects_.size();  ++i)
    delete owned_subjects_[i];
}

DifferenceEstimator::Base* DifferenceEstimator::MakeBase(const Region& region) {
  Base* base = new Base(region);
  base->Init();
  owned_bases_.push_back(base);
  return base;
}

DifferenceEstimator::Subject* DifferenceEstimator::MakeSubject(
    const Region& region) {
  Subject* subject = new Subject(region);
  owned_subjects_.push_back(subject);
  return subject;
}

size_t DifferenceEstimator::Measure(Base* base, Subject* subject) {
  size_t mismatches = 0;
  const uint8* start = subject->region().start();
  const uint8* end = subject->region().end() - (kTupleSize - 1);

  const uint8* p = start;
  while (p < end) {
    size_t hash = HashTuple(p);
    if (base->hashes_.find(hash) == base->hashes_.end()) {
      ++mismatches;
    }
    p += 1;
  }

  if (mismatches == 0) {
    if (RegionsEqual(base->region(), subject->region()))
      return 0;
  }
  ++mismatches;  // Guarantee not zero.
  return mismatches;
}

}  // namespace
