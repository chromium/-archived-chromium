// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/old_frames/point_buffer.h"

#include "base/logging.h"

PointBuffer::PointBuffer() : points_(NULL),
                             next_(0),
                             max_count_(0) {
}

PointBuffer::~PointBuffer() {
  if (points_) {
    delete []points_;
  }
}

void PointBuffer::AddPoint(int x, int y) {
  POINT t;
  t.x = x;
  t.y = y;
  AddPoint(t);
}

void PointBuffer::AddPoint(const POINT &p) {
  GrowPointBufferIfNeeded();
  points_[next_++] = p;
}

HRGN PointBuffer::GetCurrentPolygonRegion() const {
  return ::CreatePolygonRgn(points_, next_, ALTERNATE);
}

void PointBuffer::GrowPointBufferIfNeeded() {
  if (next_ == max_count_) {
    int nmc = 32 + (max_count_ * 2);
    POINT *new_buffer = new POINT[nmc];

    if (next_ > 0) {
      memcpy(new_buffer, points_, sizeof(POINT) * next_);
      delete []points_;
    }

    points_ = new_buffer;
    max_count_ = nmc;
  }
}

#if 0
void PointBuffer::Log() {
  LOG(INFO) << "PointBuffer {";
  int i;
  for (i=0 ; i < next_ ; i++) {
    LOG(INFO) << "\t" << points_[i].x << ", " << points_[i].y;
  }
}
#endif

