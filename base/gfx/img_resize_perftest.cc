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

#include <stdlib.h>
#include <time.h>

#include "base/perftimer.h"
#include "base/gfx/convolver.h"
#include "base/gfx/image_operations.h"
#include "base/gfx/image_resizer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void FillRandomData(char* dest, int byte_count) {
  srand(static_cast<unsigned>(time(NULL)));
  for (int i = 0; i < byte_count; i++)
    dest[i] = rand() * 255 / RAND_MAX;
}

}  // namespace

// Old code gives [1521, 1519]ms for this, 4000x4000 -> 2100x2100 lanczos8

TEST(ImageResizePerf, BigFilter) {
  static const int kSrcWidth = 4000;
  static const int kSrcHeight = 4000;
  static const int kSrcByteSize = kSrcWidth * kSrcHeight * 4;

  SkBitmap src_bmp;
  src_bmp.setConfig(SkBitmap::kARGB_8888_Config, kSrcWidth, kSrcHeight);
  src_bmp.allocPixels();
  FillRandomData(reinterpret_cast<char*>(src_bmp.getAddr32(0, 0)),
                 kSrcByteSize);

  // Make the dest size > 1/2 so the 50% optimization doesn't kick in.
  static const int kDestWidth = 1400;
  static const int kDestHeight = 1400;

  PerfTimeLogger resize_timer("resize");
  gfx::ImageResizer resizer(gfx::ImageResizer::LANCZOS3);
  SkBitmap dest = resizer.Resize(src_bmp, kDestWidth, kDestHeight);
}

// The original image filter we were using took 523ms for this test, while this
// one takes 857ms.
// TODO(brettw) make this at least 64% faster.
TEST(ImageOperationPerf, BigFilter) {
  static const int kSrcWidth = 4000;
  static const int kSrcHeight = 4000;
  static const int kSrcByteSize = kSrcWidth * kSrcHeight * 4;

  SkBitmap src_bmp;
  src_bmp.setConfig(SkBitmap::kARGB_8888_Config, kSrcWidth, kSrcHeight);
  src_bmp.allocPixels();
  src_bmp.setIsOpaque(true);
  FillRandomData(reinterpret_cast<char*>(src_bmp.getAddr32(0, 0)),
                 kSrcByteSize);

  // Make the dest size > 1/2 so the 50% optimization doesn't kick in.
  static const int kDestWidth = 1400;
  static const int kDestHeight = 1400;

  PerfTimeLogger resize_timer("resize");
  SkBitmap dest = gfx::ImageOperations::Resize(src_bmp,
      gfx::ImageOperations::RESIZE_LANCZOS3, (float)kDestWidth / (float)kSrcWidth,
      (float)kDestHeight / (float)kSrcHeight);
}
