/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the declaration of DisplayMode.

#ifndef O3D_CORE_CROSS_DISPLAY_MODE_H_
#define O3D_CORE_CROSS_DISPLAY_MODE_H_

#include "core/cross/types.h"

namespace o3d {

//  A DisplayMode describes the size and refresh rate of a display mode; it's
//  usually used in [or when transitioning into] fullscreen mode.  The id is a
//  platform-specific opaque identifier used to identify the mode when
//  requesting a fullscreen transition.
class DisplayMode {
 public:
  DisplayMode()
      : valid_(false),
        width_(0),
        height_(0),
        refresh_rate_(0),
        id_(-1) {  // Since this is platform-specific, -1 may well be valid.
  }
  void Set(int w, int h, int r, int i) {
    width_ = w;
    height_ = h;
    refresh_rate_ = r;
    id_ = i;
    valid_ = true;
  }
  DisplayMode(int w, int h, int r, int i) {
    Set(w, h, r, i);
  }
  DisplayMode(const DisplayMode& mode) {
    if (mode.valid()) {
      Set(mode.width(), mode.height(), mode.refresh_rate(), mode.id());
    } else {
      valid_ = false;
    }
  }
  DisplayMode& operator=(const DisplayMode& mode) {
    if (mode.valid()) {
      Set(mode.width(), mode.height(), mode.refresh_rate(), mode.id());
    } else {
      valid_ = false;
    }
    return *this;
  }

  int width() const {
    DCHECK(valid_);
    return width_;
  }
  int height() const {
    DCHECK(valid_);
    return height_;
  }
  int refresh_rate() const {
    DCHECK(valid_);
    return refresh_rate_;
  }
  int id() const {
    DCHECK(valid_);
    return id_;
  }
  bool valid() const { return valid_; }

 private:
  int width_;
  int height_;
  int refresh_rate_;
  int id_;
  bool valid_;
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_DISPLAY_MODE_H_




