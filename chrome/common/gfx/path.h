// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GFX_CHROME_PATH_H__
#define CHROME_COMMON_GFX_CHROME_PATH_H__

#include <windows.h>

#include "base/basictypes.h"
#include "SkPath.h"

namespace gfx {

class Path : public SkPath {
 public:
  Path();

  // Creates a HRGN from the path. The caller is responsible for freeing
  // resources used by this region.
  HRGN CreateHRGN() const;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(Path);
};

}

#endif  // #ifndef CHROME_COMMON_GFX_CHROME_PATH_H__

