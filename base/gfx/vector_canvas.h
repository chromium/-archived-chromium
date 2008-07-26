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

#ifndef BASE_GFX_VECTOR_CANVAS_H__
#define BASE_GFX_VECTOR_CANVAS_H__

#include "base/gfx/platform_canvas.h"
#include "base/gfx/vector_device.h"

namespace gfx {

// This class is a specialization of the regular PlatformCanvas. It is designed
// to work with a VectorDevice to manage platform-specific drawing. It allows
// using both Skia operations and platform-specific operations. It *doesn't*
// support reading back from the bitmap backstore since it is not used.
class VectorCanvas : public PlatformCanvas {
 public:
  VectorCanvas();
  VectorCanvas(HDC dc, int width, int height);
  virtual ~VectorCanvas();

  // For two-part init, call if you use the no-argument constructor above
  void initialize(HDC context, int width, int height);

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

  DISALLOW_EVIL_CONSTRUCTORS(VectorCanvas);
};

}  // namespace gfx

#endif  // BASE_GFX_VECTOR_CANVAS_H__
