// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_POINT_BUFFER_H__
#define CHROME_BROWSER_POINT_BUFFER_H__

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

#endif  // CHROME_BROWSER_PATH_BUFFER_H__
