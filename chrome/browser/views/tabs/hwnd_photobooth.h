// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_HWND_PHOTOBOOTH_H__
#define CHROME_BROWSER_VIEWS_TABS_HWND_PHOTOBOOTH_H__

#include "base/basictypes.h"
#include "base/gfx/rect.h"

class ChromeCanvas;
namespace views {
class WidgetWin;
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
  views::WidgetWin* capture_window_;

  // The current HWND being captured.
  HWND current_hwnd_;

  DISALLOW_EVIL_CONSTRUCTORS(HWNDPhotobooth);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_TABS_HWND_PHOTOBOOTH_H__
