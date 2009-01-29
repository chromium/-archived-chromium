// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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


TEST_F(RenderProcessTest, TestSharedMemoryAllocOne) {
  size_t size = base::SysInfo::VMAllocationGranularity();
  base::SharedMemory* mem = RenderProcess::AllocSharedMemory(size);
  ASSERT_TRUE(mem);
  RenderProcess::FreeSharedMemory(mem);
}

}  // namespace
