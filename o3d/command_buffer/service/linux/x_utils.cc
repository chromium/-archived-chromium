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


// This class implements the XWindowWrapper class.

#include "command_buffer/common/cross/logging.h"
#include "command_buffer/service/linux/x_utils.h"

namespace o3d {
namespace command_buffer {

bool XWindowWrapper::Initialize() {
  XWindowAttributes attributes;
  XGetWindowAttributes(display_, window_, &attributes);
  XVisualInfo visual_info_template;
  visual_info_template.visualid = XVisualIDFromVisual(attributes.visual);
  int visual_info_count = 0;
  XVisualInfo *visual_info_list = XGetVisualInfo(display_, VisualIDMask,
                                                 &visual_info_template,
                                                 &visual_info_count);
  DCHECK(visual_info_list);
  DCHECK_GT(visual_info_count, 0);
  context_ = 0;
  for (int i = 0; i < visual_info_count; ++i) {
    context_ = glXCreateContext(display_, visual_info_list + i, 0,
                                True);
    if (context_) break;
  }
  XFree(visual_info_list);
  if (!context_) {
    DLOG(ERROR) << "Couldn't create GL context.";
    return false;
  }
  return true;
}

bool XWindowWrapper::MakeCurrent() {
  if (glXMakeCurrent(display_, window_, context_) != True) {
    glXDestroyContext(display_, context_);
    context_ = 0;
    DLOG(ERROR) << "Couldn't make context current.";
    return false;
  }
  return true;
}

void XWindowWrapper::Destroy() {
  Bool result = glXMakeCurrent(display_, 0, 0);
  // glXMakeCurrent isn't supposed to fail when unsetting the context, unless
  // we have pending draws on an invalid window - which shouldn't be the case
  // here.
  DCHECK(result);
  if (context_) {
    glXDestroyContext(display_, context_);
    context_ = 0;
  }
}

void XWindowWrapper::SwapBuffers() {
  glXSwapBuffers(display_, window_);
}

}  // namespace command_buffer
}  // namespace o3d
