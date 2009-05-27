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


// File containing assert-macros for use in O3D rendering system-tests.

#ifndef O3D_TESTS_COMMON_WIN_SYSTEM_TEST_H__
#define O3D_TESTS_COMMON_WIN_SYSTEM_TEST_H__

#ifdef OS_WIN

#include "tests/common/win/dxcapture.h"

// A helper-macro to widen strings.
#define WIDEN_STRING2(x) L##x
#define WIDEN_STRING(x) WIDEN_STRING2(x)

// Initiates capture of the DX command stream.  The start of the captured
// stream will contain a message indicating the values of the line and
// file arguments.
inline void BeginStreamCapture(int line, const wchar_t* file) {
  wchar_t debug_buffer[2048];
  _snwprintf_s(debug_buffer, 2048, L"Stream_Capture: %i : %s", line, file);
  directx_capture::StartCommandCapture(debug_buffer);
}

// Places a debug marker in the graphics command stream containing
// the values of line and file, and requests that the current framebuffer
// contents be captured.
inline void CaptureFrameBuffer(int line, const wchar_t* file) {
  wchar_t debug_buffer[2048];
  _snwprintf_s(debug_buffer, 2048, L"Frame_Capture: %i : %s", line, file);
  directx_capture::CaptureFramebuffer(debug_buffer);
}

// Macros placing stream-capture begin-end waypoints in the testing application.
#define BEGIN_ASSERT_STREAM_CAPTURE() \
    BeginStreamCapture(__LINE__, WIDEN_STRING(__FILE__))

#define END_ASSERT_STREAM_CAPTURE() \
    directx_capture::EndCommandCapture()

#define ASSERT_FRAMEBUFFER() \
    CaptureFrameBuffer(__LINE__, WIDEN_STRING(__FILE__))

#else

#define BEGIN_ASSERT_STREAM_CAPTURE() do {} while (0)
#define END_ASSERT_STREAM_CAPTURE() do {} while (0)
#define ASSERT_FRAMEBUFFER() do {} while (0)

#endif

#endif  // O3D_TESTS_COMMON_WIN_SYSTEM_TEST_H__
