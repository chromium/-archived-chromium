// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GFX_CHROME_PATH_H_
#define CHROME_COMMON_GFX_CHROME_PATH_H_

#include "base/basictypes.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_LINUX)
typedef struct _GdkRegion GdkRegion;
#endif

#include "SkPath.h"

namespace gfx {

class Path : public SkPath {
 public:
  Path() : SkPath() { moveTo(0, 0); }

#if defined(OS_WIN)
  // Creates a HRGN from the path. The caller is responsible for freeing
  // resources used by this region.  This only supports polygon paths.
  HRGN CreateHRGN() const;
#elif defined(OS_LINUX)
  // Creates a Gdkregion from the path. The caller is responsible for freeing
  // resources used by this region.  This only supports polygon paths.
  GdkRegion* CreateGdkRegion() const;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(Path);
};

}

#endif  // #ifndef CHROME_COMMON_GFX_CHROME_PATH_H_
