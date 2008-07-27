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

#include <map>

#include "webkit/tools/test_shell/layout_test_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// A subclass of LayoutTestController, with additional accessors.
class TestLayoutTestController : public LayoutTestController {
 public:
  TestLayoutTestController() : LayoutTestController(NULL) {
  }

  size_t MethodCount() {
    return methods_.size();
  }

  void Reset() {
    LayoutTestController::Reset();
  }
};

class LayoutTestControllerTest : public testing::Test {
};
} // namespace

TEST(LayoutTestControllerTest, MethodMapIsInitialized) {
  const char* test_methods[] = {
    "dumpAsText",
    "waitUntilDone",
    "notifyDone",
    "dumpEditingCallbacks",
    "queueLoad",
    "windowCount",
    NULL
  };
  TestLayoutTestController controller;
  for (const char** method = test_methods; *method; ++method) {
     EXPECT_TRUE(controller.IsMethodRegistered(*method));
  }

  // One more case, to test our test.
  EXPECT_FALSE(controller.IsMethodRegistered("nonexistent_method"));
}

TEST(LayoutTestControllerTest, DumpAsTextSetAndCleared) {
  TestLayoutTestController controller;
  CppArgumentList empty_args;
  CppVariant ignored_result;
  EXPECT_FALSE(controller.ShouldDumpAsText());
  controller.dumpAsText(empty_args, &ignored_result);
  EXPECT_TRUE(ignored_result.isNull());
  EXPECT_TRUE(controller.ShouldDumpAsText());

  // Don't worry about closing remaining windows when we call reset.
  CppArgumentList args;
  CppVariant bool_false;
  bool_false.Set(false);
  args.push_back(bool_false);
  CppVariant result;
  controller.setCloseRemainingWindowsWhenComplete(args, &result);

  controller.Reset();
  EXPECT_FALSE(controller.ShouldDumpAsText());
}

TEST(LayoutTestControllerTest, DumpChildFramesAsTextSetAndCleared) {
  TestLayoutTestController controller;
  CppArgumentList empty_args;
  CppVariant ignored_result;
  EXPECT_FALSE(controller.ShouldDumpChildFramesAsText());
  controller.dumpChildFramesAsText(empty_args, &ignored_result);
  EXPECT_TRUE(ignored_result.isNull());
  EXPECT_TRUE(controller.ShouldDumpChildFramesAsText());

  // Don't worry about closing remaining windows when we call reset.
  CppArgumentList args;
  CppVariant bool_false;
  bool_false.Set(false);
  args.push_back(bool_false);
  CppVariant result;
  controller.setCloseRemainingWindowsWhenComplete(args, &result);

  controller.Reset();
  EXPECT_FALSE(controller.ShouldDumpChildFramesAsText());
}
