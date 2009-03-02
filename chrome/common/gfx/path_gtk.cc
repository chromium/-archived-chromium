// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gfx/path.h"

#include <gdk/gdk.h>

#include "base/scoped_ptr.h"

namespace gfx {

GdkRegion* Path::CreateGdkRegion() const {
  int point_count = getPoints(NULL, 0);
  scoped_array<SkPoint> points(new SkPoint[point_count]);
  getPoints(points.get(), point_count);

  scoped_array<GdkPoint> gdk_points(new GdkPoint[point_count]);
  for (int i = 0; i < point_count; ++i) {
    gdk_points[i].x = SkScalarRound(points[i].fX);
    gdk_points[i].y = SkScalarRound(points[i].fY);
  }

  return gdk_region_polygon(gdk_points.get(), point_count, GDK_EVEN_ODD_RULE);
}

}  // namespace gfx
