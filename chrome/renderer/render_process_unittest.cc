// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/rect.h"
#include "base/sys_info.h"
#include "base/string_util.h"
#include "chrome/renderer/render_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class RenderProcessTest : public testing::Test {
 public:
  virtual void SetUp() {
    RenderProcess::GlobalInit(L"render_process_unittest");
  }

  virtual void TearDown() {
    RenderProcess::GlobalCleanup();
  }

  private:
   MessageLoopForIO message_loop_;
};


TEST_F(RenderProcessTest, TestTransportDIBAllocation) {
  // On Mac, we allocate in the browser so this test is invalid.
#if !defined(OS_MACOSX)
  const gfx::Rect rect(0, 0, 100, 100);
  TransportDIB* dib;
  skia::PlatformCanvas* canvas = RenderProcess::GetDrawingCanvas(&dib, rect);
  ASSERT_TRUE(dib);
  ASSERT_TRUE(canvas);
  RenderProcess::ReleaseTransportDIB(dib);
  delete canvas;
#endif
}

}  // namespace
