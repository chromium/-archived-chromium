// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/page_setup.h"

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
