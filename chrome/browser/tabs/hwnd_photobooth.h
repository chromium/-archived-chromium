// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_HWND_PHOTOBOOTH_H__
#define CHROME_BROWSER_HWND_PHOTOBOOTH_H__

#include "base/basictypes.h"
#include "base/gfx/rect.h"

class ChromeCanvas;
namespace ChromeViews {
class HWNDViewContainer;
}

///////////////////////////////////////////////////////////////////////////////
// HWNDPhotobooth
//
//  An object that a HWND "steps into" to have its picture taken. This is used
//  to generate a full size screen shot of the contents of a HWND including
//  any child windows.
//
//  Implementation note: This causes the HWND to be re-parented to a mostly
//  off-screen layered window.
//
class HWNDPhotobooth {
 public:
  // Creates the photo booth. Constructs a nearly off-screen window, parents
  // the HWND, then shows it. The caller is responsible for destroying this
  // window, since the photo-booth will detach it before it is destroyed.
  // |canvas| is a canvas to paint the contents into, and dest_bounds is the
  // target area in |canvas| to which painted contents will be clipped.
  explicit HWNDPhotobooth(HWND initial_hwnd);

  // Destroys the photo booth window.
  virtual ~HWNDPhotobooth();

  // Replaces the HWND in the photo booth with the specified one. The caller is
  // responsible for destroying this HWND since it will be detached from the
  // capture window before the capture window is destroyed.
  void ReplaceHWND(HWND new_hwnd);

  // Paints the current display image of the window into |canvas|, clipped to
  // |target_bounds|.
  void PaintScreenshotIntoCanvas(ChromeCanvas* canvas,
                                 const gfx::Rect& target_bounds);

 private:
  // Creates a mostly off-screen window to contain the HWND to be captured.
  void CreateCaptureWindow(HWND initial_hwnd);

  // The nearly off-screen photo-booth layered window used to hold the HWND.
  ChromeViews::HWNDViewContainer* capture_window_;

  // The current HWND being captured.
  HWND current_hwnd_;

  DISALLOW_EVIL_CONSTRUCTORS(HWNDPhotobooth);
};

#endif  // #ifndef CHROME_BROWSER_HWND_PHOTOBOOTH_H__
