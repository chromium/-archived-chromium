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

#ifndef CHROME_BROWSER_FRAME_UTIL_H_
#define CHROME_BROWSER_FRAME_UTIL_H_

#include <windows.h>

class Browser;
class BrowserWindow;
namespace ChromeViews {
class AcceleratorTarget;
}
namespace gfx {
class Rect;
}

// Static helpers for frames. Basically shared code until Magic Browzr lands.
class FrameUtil {
 public:
  // Mark the frame such as it can be retrieved using GetChromeFrameForWindow()
  static void RegisterBrowserWindow(BrowserWindow* frame);

  // Return a ChromeFrame instance given an hwnd.
  static BrowserWindow* GetBrowserWindowForHWND(HWND hwnd);

  // Create a ChromeFrame for the given browser.
  static BrowserWindow* CreateBrowserWindow(const gfx::Rect& bounds,
                                            Browser* browser);

  // Initialize the accelerators for that frame.
  static bool LoadAccelerators(
      BrowserWindow* frame,
      HACCEL accelerator_table,
      ChromeViews::AcceleratorTarget* accelerator_target);

  // Activate any app modal dialog that might be present. Returns true if one
  // was present.
  static bool ActivateAppModalDialog(Browser* browser);

  // Invoked when windows is shutting down (or the user is logging off). When
  // this method returns windows is going to kill our process. As such, this
  // blocks until the shut-down has been marked as clean.
  static void EndSession();
};

#endif  // #ifndef CHROME_BROWSER_FRAME_UTIL_H__
