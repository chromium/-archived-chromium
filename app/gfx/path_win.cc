// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/path.h"

#include "base/scoped_ptr.h"

namespace gfx {

HRGN Path::CreateHRGN() const {
  int point_count = getPoints(NULL, 0);
  scoped_array<SkPoint> points(new SkPoint[point_count]);
  getPoints(points.get(), point_count);
  scoped_array<POINT> windows_points(new POINT[point_count]);
  for (int i = 0; i < point_count; ++i) {
    windows_points[i].x = SkScalarRound(points[i].fX);
    windows_points[i].y = SkScalarRound(points[i].fY);
  }

  return ::CreatePolygonRgn(windows_points.get(), point_count, ALTERNATE);
}

}  // namespace gfx
