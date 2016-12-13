// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/native_view_photobooth_gtk.h"

#include "app/gfx/canvas.h"
#include "base/logging.h"

///////////////////////////////////////////////////////////////////////////////
// NativeViewPhotoboothGtk, public:

// static
NativeViewPhotobooth* NativeViewPhotobooth::Create(
    gfx::NativeView initial_view) {
  return new NativeViewPhotoboothGtk(initial_view);
}

NativeViewPhotoboothGtk::NativeViewPhotoboothGtk(
    gfx::NativeView initial_view) {
}

NativeViewPhotoboothGtk::~NativeViewPhotoboothGtk() {
}

void NativeViewPhotoboothGtk::Replace(gfx::NativeView new_view) {
  NOTIMPLEMENTED();
}

void NativeViewPhotoboothGtk::PaintScreenshotIntoCanvas(
    gfx::Canvas* canvas,
    const gfx::Rect& target_bounds) {
  NOTIMPLEMENTED();
}
