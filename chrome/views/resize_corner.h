// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_RESIZE_CORNER_H__
#define CHROME_VIEWS_RESIZE_CORNER_H__

#include "chrome/views/view.h"

namespace views {

// Simple drawing of a resize corner. Has no functionality.
class ResizeCorner : public View {
 public:
  ResizeCorner();
  virtual ~ResizeCorner();

  // View
  virtual void Paint(ChromeCanvas* canvas);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ResizeCorner);
};

}  // namespace views

#endif  // CHROME_VIEWS_RESIZE_CORNER_H__

