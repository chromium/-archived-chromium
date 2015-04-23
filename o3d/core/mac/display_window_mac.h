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


#ifndef O3D_CORE_MAC_DISPLAY_WINDOW_MAC_H_
#define O3D_CORE_MAC_DISPLAY_WINDOW_MAC_H_

#include <OpenGL/OpenGL.h>
#include <AGL/agl.h>

#include "core/cross/display_window.h"

namespace o3d {

/**
 * The DisplayWindowMac class is a platform-specific representation of
 * a platform's display window.  This implements the Macintosh OS X
 * subclass of the DisplayWindow.
 */

class DisplayWindowMac : public DisplayWindow {
 public:
  DisplayWindowMac() : agl_context_(NULL), cgl_context_(NULL) {}
  virtual ~DisplayWindowMac() {}

  AGLContext agl_context() const { return agl_context_; }
  CGLContextObj cgl_context() const { return cgl_context_; }

  void set_agl_context(const AGLContext& agl_context) {
    agl_context_ = agl_context;
  }

  void set_cgl_context(const CGLContextObj& cgl_context) {
    cgl_context_ = cgl_context;
  }
 private:
  AGLContext agl_context_;
  CGLContextObj cgl_context_;

  DISALLOW_COPY_AND_ASSIGN(DisplayWindowMac);
};
}  // end namespace o3d

#endif  // O3D_CORE_MAC_DISPLAY_WINDOW_MAC_H_
