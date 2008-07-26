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

#include "chrome/browser/printing/page_setup.h"

#include <stdlib.h>
#include <time.h>

#include "testing/gtest/include/gtest/gtest.h"

TEST(PageSetupTest, Random) {
  time_t seed = time(NULL);
  int kMax = 10;
  srand(static_cast<unsigned>(seed));

  // Margins.
  printing::PageMargins margins;
  margins.header = rand() % kMax;
  margins.footer = rand() % kMax;
  margins.left = rand() % kMax;
  margins.top = rand() % kMax;
  margins.right = rand() % kMax;
  margins.bottom = rand() % kMax;
  int kTextHeight = rand() % kMax;

  // Page description.
  gfx::Size page_size(100 + rand() % kMax, 200 + rand() % kMax);
  gfx::Rect printable_area(rand() % kMax, rand() % kMax, 0, 0);
  printable_area.set_width(page_size.width() - (rand() % kMax) -
                           printable_area.x());
  printable_area.set_height(page_size.height() - (rand() % kMax) -
                            printable_area.y());

  // Make the calculations.
  printing::PageSetup setup;
  setup.SetRequestedMargins(margins);
  setup.Init(page_size, printable_area, kTextHeight);

  // Calculate the effective margins.
  printing::PageMargins effective_margins;
  effective_margins.header = std::max(margins.header, printable_area.y());
  effective_margins.left = std::max(margins.left, printable_area.x());
  effective_margins.top = std::max(margins.top,
                                   effective_margins.header + kTextHeight);
  effective_margins.footer = std::max(margins.footer,
                                      page_size.height() -
                                          printable_area.bottom());
  effective_margins.right = std::max(margins.right,
                                      page_size.width() -
                                          printable_area.right());
  effective_margins.bottom = std::max(margins.bottom,
                                      effective_margins.footer  + kTextHeight);

  // Calculate the overlay area.
  gfx::Rect overlay_area(effective_margins.left, effective_margins.header,
                         page_size.width() - effective_margins.right -
                            effective_margins.left,
                         page_size.height() - effective_margins.footer -
                            effective_margins.header);

  // Calculate the content area.
  gfx::Rect content_area(overlay_area.x(),
                         effective_margins.top,
                         overlay_area.width(),
                         page_size.height() - effective_margins.bottom -
                             effective_margins.top);

  // Test values.
  EXPECT_EQ(page_size, setup.physical_size()) << seed << " " << page_size <<
      " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(overlay_area, setup.overlay_area()) << seed << " " << page_size <<
      " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(content_area, setup.content_area()) << seed << " " << page_size <<
      " " << printable_area << " " << kTextHeight;

  EXPECT_EQ(effective_margins.header, setup.effective_margins().header) <<
      seed << " " << page_size << " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(effective_margins.footer, setup.effective_margins().footer) <<
      seed << " " << page_size << " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(effective_margins.left, setup.effective_margins().left) << seed <<
      " " << page_size << " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(effective_margins.top, setup.effective_margins().top) << seed <<
      " " << page_size << " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(effective_margins.right, setup.effective_margins().right) << seed <<
      " " << page_size << " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(effective_margins.bottom, setup.effective_margins().bottom) <<
      seed << " " << page_size << " " << printable_area << " " << kTextHeight;
}

TEST(PageSetupTest, HardCoded) {
  // Margins.
  printing::PageMargins margins;
  margins.header = 2;
  margins.footer = 2;
  margins.left = 4;
  margins.top = 4;
  margins.right = 4;
  margins.bottom = 4;
  int kTextHeight = 3;

  // Page description.
  gfx::Size page_size(100, 100);
  gfx::Rect printable_area(3, 3, 94, 94);

  // Make the calculations.
  printing::PageSetup setup;
  setup.SetRequestedMargins(margins);
  setup.Init(page_size, printable_area, kTextHeight);

  // Calculate the effective margins.
  printing::PageMargins effective_margins;
  effective_margins.header = 3;
  effective_margins.left = 4;
  effective_margins.top = 6;
  effective_margins.footer = 3;
  effective_margins.right = 4;
  effective_margins.bottom = 6;

  // Calculate the overlay area.
  gfx::Rect overlay_area(4, 3, 92, 94);

  // Calculate the content area.
  gfx::Rect content_area(4, 6, 92, 88);

  // Test values.
  EXPECT_EQ(page_size, setup.physical_size()) << " " << page_size <<
      " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(overlay_area, setup.overlay_area()) << " " << page_size <<
      " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(content_area, setup.content_area()) << " " << page_size <<
      " " << printable_area << " " << kTextHeight;

  EXPECT_EQ(effective_margins.header, setup.effective_margins().header) <<
      " " << page_size << " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(effective_margins.footer, setup.effective_margins().footer) <<
      " " << page_size << " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(effective_margins.left, setup.effective_margins().left) <<
      " " << page_size << " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(effective_margins.top, setup.effective_margins().top) <<
      " " << page_size << " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(effective_margins.right, setup.effective_margins().right) <<
      " " << page_size << " " << printable_area << " " << kTextHeight;
  EXPECT_EQ(effective_margins.bottom, setup.effective_margins().bottom) <<
      " " << page_size << " " << printable_area << " " << kTextHeight;
}
