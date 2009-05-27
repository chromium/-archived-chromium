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


// This file implements the GAPIGL class.

#include <build/build_config.h>
#include "command_buffer/service/cross/gl/gl_utils.h"
#include "command_buffer/service/cross/gl/gapi_gl.h"

#ifdef OS_LINUX
#include "command_buffer/service/linux/x_utils.h"
#endif  // OS_LINUX

namespace o3d {
namespace command_buffer {

GAPIGL::GAPIGL()
#ifdef OS_LINUX
    : window_(NULL),
#endif
      cg_context_(NULL),
      current_vertex_struct_(kInvalidResource),
      validate_streams_(true),
      max_vertices_(0),
      current_effect_id_(kInvalidResource),
      validate_effect_(true),
      current_effect_(NULL) {
}

GAPIGL::~GAPIGL() {
}

bool GAPIGL::Initialize() {
#ifdef OS_LINUX
  DCHECK(window_);
  if (!window_->Initialize())
    return false;
  if (!window_->MakeCurrent())
    return false;
  InitCommon();
  CHECK_GL_ERROR();
  return true;
#else
  return false;
#endif
}

void GAPIGL::InitCommon() {
  cg_context_ = cgCreateContext();
  // Set up all Cg State Assignments for OpenGL.
  cgGLRegisterStates(cg_context_);
  cgGLSetDebugMode(CG_FALSE);
  // Enable the profiles we use.
  cgGLEnableProfile(CG_PROFILE_ARBVP1);
  cgGLEnableProfile(CG_PROFILE_ARBFP1);
  // Initialize global GL settings.
  // Tell GL that texture buffers can be single-byte aligned.
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
  glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

  // Get the initial viewport (set to the window size) to set up the helper
  // constant.
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  SetViewport(viewport[0], viewport[1], viewport[2], viewport[3], 0.f, 1.f);
  CHECK_GL_ERROR();
}

void GAPIGL::Destroy() {
  vertex_buffers_.DestroyAllResources();
  index_buffers_.DestroyAllResources();
  vertex_structs_.DestroyAllResources();
  effects_.DestroyAllResources();
  effect_params_.DestroyAllResources();
  // textures_.DestroyAllResources();
  // samplers_.DestroyAllResources();
  cgDestroyContext(cg_context_);
  cg_context_ = NULL;
#ifdef OS_LINUX
  DCHECK(window_);
  window_->Destroy();
#endif
}

void GAPIGL::BeginFrame() {
}

void GAPIGL::EndFrame() {
#ifdef OS_LINUX
  DCHECK(window_);
  window_->SwapBuffers();
#endif
  CHECK_GL_ERROR();
}

void GAPIGL::Clear(unsigned int buffers,
                   const RGBA &color,
                   float depth,
                   unsigned int stencil) {
  glClearColor(color.red, color.green, color.blue, color.alpha);
  glClearDepth(depth);
  glClearStencil(stencil);
  glClear((buffers & COLOR ? GL_COLOR_BUFFER_BIT : 0) |
          (buffers & DEPTH ? GL_DEPTH_BUFFER_BIT : 0) |
          (buffers & STENCIL ? GL_STENCIL_BUFFER_BIT : 0));
  CHECK_GL_ERROR();
}

}  // namespace command_buffer
}  // namespace o3d
