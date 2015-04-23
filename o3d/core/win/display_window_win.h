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


#ifndef O3D_CORE_WIN_DISPLAY_WINDOW_WIN_H_
#define O3D_CORE_WIN_DISPLAY_WINDOW_WIN_H_

#include "core/cross/display_window.h"

namespace o3d {

/**
 * The DisplayWindowWindows class is a platform-specific
 * representation of a platform's display window. This implements the
 * Windows subclass of the DisplayWindow.
 */

class DisplayWindowWindows : public DisplayWindow {
 public:
  DisplayWindowWindows() : hwnd_(NULL) {}
  virtual ~DisplayWindowWindows() {}
  HWND hwnd() const { return hwnd_; }
  void set_hwnd(const HWND& hwnd) { hwnd_ = hwnd; }
 private:
  HWND hwnd_;
  DISALLOW_COPY_AND_ASSIGN(DisplayWindowWindows);
};
}  // end namespace o3d

#endif  // O3D_CORE_WIN_DISPLAY_WINDOW_WIN_H_
