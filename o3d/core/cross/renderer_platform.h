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


// This include determines which renderer to include, based on the
// compile options, so that this is the only place that needs to know
// what all the choices are.

#ifndef O3D_CORE_CROSS_RENDERER_PLATFORM_H_
#define O3D_CORE_CROSS_RENDERER_PLATFORM_H_

#include <build/build_config.h>
#if defined(OS_MACOSX)
#include <OpenGL/OpenGL.h>
#include <AGL/agl.h>
#elif defined(OS_LINUX)
#include <GL/glx.h>
#elif defined(OS_WIN) && defined(RENDERER_GL)
#include <gl/GL.h>
#endif

#if defined(OS_WIN)
#include "core/win/display_window_win.h"
#elif defined(OS_MACOSX)
#include "core/mac/display_window_mac.h"
#elif defined(OS_LINUX)
#include "core/linux/display_window_linux.h"
#else
#error Platform not recognized.
#endif

#if defined(RENDERER_D3D9) && defined(OS_WIN)
#include "core/win/d3d9/renderer_d3d9.h"
#elif defined(RENDERER_GL)
#include "core/cross/gl/renderer_gl.h"
#elif defined(RENDERER_CB)
#include "core/cross/command_buffer/renderer_cb.h"
#else
#error Renderer not recognized.
#endif

#endif  // O3D_CORE_CROSS_RENDERER_PLATFORM_H_
