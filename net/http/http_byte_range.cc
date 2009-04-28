// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "net/http/http_byte_range.h"

namespace {

const int64 kPositionNotSpecified = -1;

}  // namespace

namespace net {

HttpByteRange::HttpByteRange()
    : first_byte_position_(kPositionNotSpecified),
      last_byte_position_(kPositionNotSpecified),
      suffix_length_(kPositionNotSpecified),
      has_computed_bounds_(false) {
}

bool HttpByteRange::IsSuffixByteRange() const {
  return suffix_length_ != kPositionNotSpecified;
}

bool HttpByteRange::HasFirstBytePosition() const {
  return first_byte_position_ != kPositionNotSpecified;
}

bool HttpByteRange::HasLastBytePosition() const {
  return last_byte_position_ != kPositionNotSpecified;
}

bool HttpByteRange::IsValid() const {
  if (suffix_length_ > 0)
    return true;
  return first_byte_position_ >= 0 &&
         (last_byte_position_ == kPositionNotSpecified ||
          last_byte_position_ >= first_byte_position_);
}

bool HttpByteRange::ComputeBounds(int64 size) {
  if (size < 0)
    return false;
  if (has_computed_bounds_)
    return false;
  has_computed_bounds_ = true;

  // Empty values.
  if (!HasFirstBytePosition() &&
      !HasLastBytePosition() &&
      !IsSuffixByteRange()) {
      first_byte_position_ = 0;
      last_byte_position_ = size - 1;
      return true;
  }
  if (!IsValid())
    return false;
  if (IsSuffixByteRange()) {
    first_byte_position_ = size - std::min(size, suffix_length_);
    last_byte_position_ = size - 1;
    return true;
  }
  if (first_byte_position_ < size) {
    if (HasLastBytePosition())
      last_byte_position_ = std::min(size - 1, last_byte_position_);
    else
      last_byte_position_ = size - 1;
    return true;
  }
  return false;
}

}  // namespace net
