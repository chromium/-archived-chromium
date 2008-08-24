// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

