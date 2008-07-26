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

#include "chrome/browser/printing/win_printing_context.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/browser/printing/printing_test.h"
#include "chrome/browser/printing/print_settings.h"

// This test is automatically disabled if no printer is available.
class PrintingContextTest : public PrintingTest<testing::Test> {
};

TEST_F(PrintingContextTest, Base) {
  printing::PrintSettings settings;

  settings.set_device_name(GetDefaultPrinter());
  // Initialize it.
  printing::PrintingContext context;
  EXPECT_EQ(context.InitWithSettings(settings), printing::PrintingContext::OK);

  ;
  // The print may lie to use and may not support world transformation.
  // Verify right now.
  XFORM random_matrix = { 1, 0.1f, 0, 1.5f, 0, 1 };
  EXPECT_TRUE(SetWorldTransform(context.context(), &random_matrix));
  EXPECT_TRUE(ModifyWorldTransform(context.context(), NULL, MWT_IDENTITY));
}
