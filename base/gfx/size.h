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

#ifndef BASE_GFX_SIZE_H__
#define BASE_GFX_SIZE_H__

#ifdef UNIT_TEST
#include <iostream>
#endif

#if defined(OS_WIN)
typedef struct tagSIZE SIZE;
#elif defined(OS_MACOSX)
#import <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

//
// A size has width and height values.
//
class Size {
 public:
  Size() : width_(0), height_(0) {}
  Size(int width, int height) : width_(width), height_(height) {}

  ~Size() {}

  int width() const { return width_; }
  int height() const { return height_; }

  void SetSize(int width, int height) {
    width_ = width;
    height_ = height;
  }

  void set_width(int width) { width_ = width; }
  void set_height(int height) { height_ = height; }

  bool operator==(const Size& s) const {
    return width_ == s.width_ && height_ == s.height_;
  }

  bool operator!=(const Size& s) const {
    return !(*this == s);
  }

  bool IsEmpty() const {
    return !width_ && !height_;
  }

#if defined(OS_WIN)
  SIZE ToSIZE() const;
#elif defined(OS_MACOSX)
  CGSize ToCGSize() const;
#endif

 private:
  int width_;
  int height_;
};

}  // namespace gfx

#ifdef UNIT_TEST

inline std::ostream& operator<<(std::ostream& out, const gfx::Size& s) {
  return out << s.width() << "x" << s.height();
}

#endif  // #ifdef UNIT_TEST

#endif // BASE_GFX_SIZE_H__
