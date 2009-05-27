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


// Set of external declarations for global objects required for O3D
// testing.

#ifndef O3D_TESTS_COMMON_WIN_TESTING_COMMON_H__
#define O3D_TESTS_COMMON_WIN_TESTING_COMMON_H__

#include "core/cross/types.h"
#include "gtest/gtest.h"

namespace o3d {
class ServiceLocator;
class Renderer;
}

extern o3d::ServiceLocator* g_service_locator;

// g_renderer should be declared in a separate .cc file.  The
// code in this file must remain platform agnostic.
extern o3d::Renderer* g_renderer;

// Path to the executable, used to load files relative to it.
extern o3d::String *g_program_path;

// the window handle used to create the current window, used to instance a
// specific Renderer class.
#if defined(OS_WIN)
extern HWND g_window_handle;
#endif
// Un-qualified name of the executable, stripped of all path information.
// Note that the executable extension is included in this string.
extern o3d::String* g_program_name;


#endif  // O3D_TESTS_COMMON_WIN_TESTING_COMMON_H__
