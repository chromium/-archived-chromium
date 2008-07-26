// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/point_buffer.h"
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
