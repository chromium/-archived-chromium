// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

#include "printing/printing_test.h"
#include "printing/print_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

// This test is automatically disabled if no printer is available.
class PrintingContextTest : public PrintingTest<testing::Test> {
};

TEST_F(PrintingContextTest, Base) {
  printing::PrintSettings settings;

  settings.set_device_name(GetDefaultPrinter());
  // Initialize it.
  printing::PrintingContext context;
  EXPECT_EQ(context.InitWithSettings(settings), printing::PrintingContext::OK);

  // The print may lie to use and may not support world transformation.
  // Verify right now.
  XFORM random_matrix = { 1, 0.1f, 0, 1.5f, 0, 1 };
  EXPECT_TRUE(SetWorldTransform(context.context(), &random_matrix));
  EXPECT_TRUE(ModifyWorldTransform(context.context(), NULL, MWT_IDENTITY));
}
