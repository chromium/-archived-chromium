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


// This file declares the XWindowWrapper class.

#ifndef O3D_COMMAND_BUFFER_SERVICE_LINUX_X_UTILS_H_
#define O3D_COMMAND_BUFFER_SERVICE_LINUX_X_UTILS_H_

#include <GL/glx.h>
#include "base/basictypes.h"
#include "command_buffer/common/cross/logging.h"

namespace o3d {
namespace command_buffer {

// This class is a wrapper around an X Window and associated GL context. It is
// useful to isolate intrusive X headers, since it can be forward declared
// (Window and GLXContext can't).
class XWindowWrapper {
 public:
  XWindowWrapper(Display *display, Window window)
      : display_(display),
        window_(window) {
    DCHECK(display_);
    DCHECK(window_);
  }
  // Initializes the GL context.
  bool Initialize();

  // Destroys the GL context.
  void Destroy();

  // Makes the GL context current on the current thread.
  bool MakeCurrent();

  // Swaps front and back buffers.
  void SwapBuffers();

 private:
  Display *display_;
  Window window_;
  GLXContext context_;
  DISALLOW_COPY_AND_ASSIGN(XWindowWrapper);
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_LINUX_X_UTILS_H_
