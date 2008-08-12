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

#include "chrome/browser/printing/page_number.h"
#include "chrome/browser/printing/print_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PageNumberTest, Count) {
  printing::PrintSettings settings;
  printing::PageNumber page;
  EXPECT_EQ(printing::PageNumber::npos(), page);
  page.Init(settings, 3);
  EXPECT_EQ(0, page.ToInt());
  EXPECT_NE(printing::PageNumber::npos(), page);
  ++page;
  EXPECT_EQ(1, page.ToInt());
  EXPECT_NE(printing::PageNumber::npos(), page);
  
  printing::PageNumber page_copy(page);
  EXPECT_EQ(1, page_copy.ToInt());
  EXPECT_EQ(1, page.ToInt());
  ++page;
  EXPECT_EQ(1, page_copy.ToInt());
  EXPECT_EQ(2, page.ToInt());
  ++page;
  EXPECT_EQ(printing::PageNumber::npos(), page);
  ++page;
  EXPECT_EQ(printing::PageNumber::npos(), page);
}
