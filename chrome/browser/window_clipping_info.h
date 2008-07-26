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

#ifndef CHROME_BROWSER_WINDOW_CLIPPING_INFO_H__
#define CHROME_BROWSER_WINDOW_CLIPPING_INFO_H__

#include <windows.h>
#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>

#include "base/basictypes.h"

////////////////////////////////////////////////////////////////////////////////
//
// WindowClippingInfo class
//
// A facility to compute the invisible areas of a window. Given a window,
// this helper class computes all the areas which are obscured or clipped
// by the screen bounds and return them within a single HRGN.
//
////////////////////////////////////////////////////////////////////////////////
class WindowClippingInfo {
 public:
  //
  // Create a new WindowClippingInfo instance to compute clipping info for HWND
  // If ignore_wnd is provided, obstruction from that window will be ignored
  //
  WindowClippingInfo(HWND aWindow, HWND ignore_wnd);
  ~WindowClippingInfo();

  //
  // Return a single region containing all the clipped areas
  // of the HWND. The region is in HWND coordinate system
  //
  HRGN GetClippedRegion() const;

  //
  // Convenience to test whether the window is clipped
  //
  BOOL IsClipped() const;

 private:
  // Combine 2 regions
  HRGN CombineRegions(HRGN existing, HRGN additional) const;

  // Compute the clipping caused by the screen boundaries
  void ComputeScreenClipping();

  static BOOL CALLBACK WindowEnumProc(HWND hwnd, LPARAM lParam);

  // Compute the clipping caused by higher level windows
  void ComputeWindowsClipping();

  HWND hwnd_;
  HWND ignore_;
  HRGN rgn_;
  CRect hwnd_rect_;

  DISALLOW_EVIL_CONSTRUCTORS(WindowClippingInfo);
};

#endif  // CHROME_BROWSER_WINDOW_CLIPPING_INFO_H__
