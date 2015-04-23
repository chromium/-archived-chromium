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


// File exposing a set functions to capture the direct-X command
// stream and frame-buffer contents.  This class is to be used for
// regression-testing purposes.

#ifndef O3D_TESTS_COMMON_WIN_DXCAPTURE_H__
#define O3D_TESTS_COMMON_WIN_DXCAPTURE_H__

// Singleton class encapsulating functionality to capture DX command streams
// and framebuffer contents.
namespace directx_capture {

// Routine to inform the testing framework that graphics command logs
// should be captured.  The contents of stream_name will be embedded in the
// logs for ease of human readability.
// NOTE:  Requires that the executable be invoked through PIX.  Fn is a no-op
// when the PIX environment is not present.
void StartCommandCapture(const wchar_t* stream_name);

// Invoke to disable stream capture.  Note that the start/end routines
// are NOT re-entrant.  One cannot nest stream captures.
// NOTE:  Requires that the executable be invoked through PIX.  Fn is a no-op
// when the PIX environment is not present.
void EndCommandCapture();

// Invoke to capture the current contents of the framebuffer.  If stream
// capture is active, the contents of buffer_metadata will be written to the
// logs.
// NOTE:  If PIX is present, then the frame-buffer is captured and stored by
// PIX according to the PIXRun file.  Otherwise, the contents of the current
// render target surface are saved explicitly by the code.  The meta-data
// is ignored when PIX is not present.
void CaptureFramebuffer(const wchar_t* buffer_metadata);

}  // end namespace directx_capture

#endif  // O3D_TESTS_COMMON_WIN_DXCAPTURE_H__
