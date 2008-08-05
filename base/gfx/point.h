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

#ifndef BASE_GFX_POINT_H__
#define BASE_GFX_POINT_H__

#include "build/build_config.h"

#ifdef UNIT_TEST
#include <iostream>
#endif

#if defined(OS_WIN)
typedef struct tagPOINT POINT;
#elif defined(OS_MACOSX)
#import <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

//
// A point has an x and y coordinate.
//
class Point {
 public:
  Point();
  Point(int x, int y);
#if defined(OS_WIN)
  explicit Point(const POINT& point);
#elif defined(OS_MACOSX)
  explicit Point(const CGPoint& point);
#endif

  ~Point() {}

  int x() const { return x_; }
  int y() const { return y_; }

  void SetPoint(int x, int y) {
    x_ = x;
    y_ = y;
  }

  void set_x(int x) { x_ = x; }
  void set_y(int y) { y_ = y; }

  bool operator==(const Point& rhs) const {
    return x_ == rhs.x_ && y_ == rhs.y_;
  }

  bool operator!=(const Point& rhs) const {
    return !(*this == rhs);
  }

#if defined(OS_WIN)
  POINT ToPOINT() const;
#elif defined(OS_MACOSX)
  CGPoint ToCGPoint() const;
#endif

 private:
  int x_;
  int y_;
};

}  // namespace gfx

#ifdef UNIT_TEST

inline std::ostream& operator<<(std::ostream& out, const gfx::Point& p) {
  return out << p.x() << "," << p.y();
}

#endif  // #ifdef UNIT_TEST

#endif // BASE_GFX_POINT_H__
