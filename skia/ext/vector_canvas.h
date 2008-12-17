// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_VECTOR_CANVAS_H_
#define SKIA_EXT_VECTOR_CANVAS_H_

#include "skia/ext/platform_canvas_win.h"
#include "skia/ext/vector_device.h"

namespace skia {

// This class is a specialization of the regular PlatformCanvas. It is designed
// to work with a VectorDevice to manage platform-specific drawing. It allows
// using both Skia operations and platform-specific operations. It *doesn't*
// support reading back from the bitmap backstore since it is not used.
class VectorCanvas : public PlatformCanvasWin {
 public:
  VectorCanvas();
  VectorCanvas(HDC dc, int width, int height);
  virtual ~VectorCanvas();

  // For two-part init, call if you use the no-argument constructor above
  bool initialize(HDC context, int width, int height);

  virtual SkBounder* setBounder(SkBounder*);
  virtual SkDevice* createDevice(SkBitmap::Config config,
                                 int width, int height,
                                 bool is_opaque, bool isForLayer);
  virtual SkDrawFilter* setDrawFilter(SkDrawFilter* filter);

 private:
  // |is_opaque| is unused. |shared_section| is in fact the HDC used for output.
  virtual SkDevice* createPlatformDevice(int width, int height, bool is_opaque,
                                         HANDLE shared_section);

  // Returns true if the top device is vector based and not bitmap based.
  bool IsTopDeviceVectorial() const;

  DISALLOW_COPY_AND_ASSIGN(VectorCanvas);
};

}  // namespace skia

#endif  // SKIA_EXT_VECTOR_CANVAS_H_

