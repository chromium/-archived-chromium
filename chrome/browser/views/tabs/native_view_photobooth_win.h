// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_NATIVE_VIEW_PHOTOBOOTH_WIN_H_
#define CHROME_BROWSER_VIEWS_TABS_NATIVE_VIEW_PHOTOBOOTH_WIN_H_

#include "chrome/browser/views/tabs/native_view_photobooth.h"

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
class NativeViewPhotoboothWin : public NativeViewPhotobooth {
 public:
  // Creates the photo booth. Constructs a nearly off-screen window, parents
  // the HWND, then shows it. The caller is responsible for destroying this
  // window, since the photo-booth will detach it before it is destroyed.
  // |canvas| is a canvas to paint the contents into, and dest_bounds is the
  // target area in |canvas| to which painted contents will be clipped.
  explicit NativeViewPhotoboothWin(gfx::NativeView initial_view);

  // Destroys the photo booth window.
  virtual ~NativeViewPhotoboothWin();

  // Replaces the view in the photo booth with the specified one.
  virtual void Replace(gfx::NativeView new_view);

  // Paints the current display image of the window into |canvas|, clipped to
  // |target_bounds|.
  virtual void PaintScreenshotIntoCanvas(gfx::Canvas* canvas,
                                         const gfx::Rect& target_bounds);

 private:
  // Creates a mostly off-screen window to contain the HWND to be captured.
  void CreateCaptureWindow(HWND initial_hwnd);

  // The nearly off-screen photo-booth layered window used to hold the HWND.
  views::WidgetWin* capture_window_;

  // The current HWND being captured.
  HWND current_hwnd_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewPhotoboothWin);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_TABS_NATIVE_VIEW_PHOTOBOOTH_WIN_H_
