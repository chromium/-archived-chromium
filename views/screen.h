// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_SCREEN_H_
#define VIEWS_SCREEN_H_

#include "base/gfx/point.h"

namespace views {

// A utility class for getting various info about screen size, monitors,
// cursor position, etc.
// TODO(erikkay) add more of those methods here
class Screen {
 public:
  static gfx::Point GetCursorScreenPoint();
};

}  // namespace views

#endif  // VIEWS_SCREEN_H_
