// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/screen.h"

#include <windows.h>

namespace views {

// static
gfx::Point Screen::GetCursorScreenPoint() {
  POINT pt;
  GetCursorPos(&pt);
  return gfx::Point(pt);
}

}  // namespace

