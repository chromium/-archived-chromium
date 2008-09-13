// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OLD_FRAMES_POINT_BUFFER_H__
#define CHROME_BROWSER_VIEWS_OLD_FRAMES_POINT_BUFFER_H__

#include <windows.h>

#include "base/basictypes.h"

////////////////////////////////////////////////////////////////////////////////
//
// PointBuffer class
//
// A facility to accumulate 2d points and produce polygon regions.
//
////////////////////////////////////////////////////////////////////////////////
class PointBuffer {
 public:

  //
  // Create an empty path buffer
  //
  PointBuffer();

  ~PointBuffer();

  //
  // Add a point in the buffer
  //
  void AddPoint(const POINT &p);
  void AddPoint(int x, int y);

  //
  // Return new polygon region matching the current points.
  // It is the caller's responsability to delete the returned region by using
  // ::DeleteObject()
  //
  HRGN GetCurrentPolygonRegion() const;

#if 0
  void Log();
#endif

 private:

  //
  // Grow the point buffer if we are out of space
  //
  void GrowPointBufferIfNeeded();

  POINT *points_;
  int next_;
  int max_count_;

  DISALLOW_EVIL_CONSTRUCTORS(PointBuffer);
};

#endif  // CHROME_BROWSER_VIEWS_OLD_FRAMES_POINT_BUFFER_H__

