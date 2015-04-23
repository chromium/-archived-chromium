// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
