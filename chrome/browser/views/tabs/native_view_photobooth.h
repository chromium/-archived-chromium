// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_NATIVE_VIEW_PHOTOBOOTH_H_
#define CHROME_BROWSER_VIEWS_TABS_NATIVE_VIEW_PHOTOBOOTH_H_

#include "base/gfx/native_widget_types.h"

namespace gfx {
class Canvas;
class Rect;
}
///////////////////////////////////////////////////////////////////////////////
// NativeViewPhotobooth
//
//  An object that a NativeView "steps into" to have its picture taken. This is
//  used to generate a full size screen shot of the contents of a NativeView
//  including any child windows.
//
//  Implementation note: This causes the NativeView to be re-parented to a
//  mostly off-screen layered window.
//
class NativeViewPhotobooth {
 public:
  // Creates the photo booth. Constructs a nearly off-screen window, parents
  // the view, then shows it. The caller is responsible for destroying this
  // photo-booth, since the photo-booth will detach it before it is destroyed.
  static NativeViewPhotobooth* Create(gfx::NativeView initial_view);

  // Destroys the photo booth window.
  virtual ~NativeViewPhotobooth() {}

  // Replaces the view in the photo booth with the specified one.
  virtual void Replace(gfx::NativeView new_view) = 0;

  // Paints the current display image of the window into |canvas|, clipped to
  // |target_bounds|.
  virtual void PaintScreenshotIntoCanvas(gfx::Canvas* canvas,
                                         const gfx::Rect& target_bounds) = 0;
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_TABS_NATIVE_VIEW_PHOTOBOOTH_H_
