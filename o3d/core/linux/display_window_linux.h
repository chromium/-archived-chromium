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


#ifndef O3D_CORE_LINUX_DISPLAY_WINDOW_LINUX_H_
#define O3D_CORE_LINUX_DISPLAY_WINDOW_LINUX_H_

#include <GL/glx.h>

#include "core/cross/display_window.h"

namespace o3d {

/**
 * The DisplayWindowLinux class is a platform-specific representation
 * of a platform's display window.  This implements the Linux subclass
 * of the DisplayWindow.
 */

class DisplayWindowLinux : public DisplayWindow {
 public:
  DisplayWindowLinux() : display_(NULL), window_(0) {}
  virtual ~DisplayWindowLinux() {}

  Display* display() const { return display_; }
  Window window() const { return window_; }
  void set_window(Window window) { window_ = window; }
  void set_display(Display* display) { display_ = display; }

 private:
  Display* display_;
  Window window_;

  DISALLOW_COPY_AND_ASSIGN(DisplayWindowLinux);
};
}  // end namespace o3d

#endif  // O3D_CORE_LINUX_DISPLAY_WINDOW_LINUX_H_
