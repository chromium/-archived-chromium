// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_WINDOW_RESOURCES_H_
#define VIEWS_WINDOW_WINDOW_RESOURCES_H_

class SkBitmap;

namespace views {

typedef int FramePartBitmap;

///////////////////////////////////////////////////////////////////////////////
// WindowResources
//
//  An interface implemented by an object providing bitmaps to render the
//  contents of a window frame. The Window may swap in different
//  implementations of this interface to render different modes. The definition
//  of FramePartBitmap depends on the implementation.
//
class WindowResources {
 public:
  virtual ~WindowResources() { }
  virtual SkBitmap* GetPartBitmap(FramePartBitmap part) const = 0;
};

}  // namespace views

#endif  // VIEWS_WINDOW_WINDOW_RESOURCES_H_
