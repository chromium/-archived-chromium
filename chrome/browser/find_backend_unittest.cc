// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/common/url_constants.h"

typedef RenderViewHostTestHarness FindBackendTest;

// This test takes two TabContents objects, searches in both of them and
// tests the internal state for find_text and find_prepopulate_text.
TEST_F(FindBackendTest, InternalState) {
  // Initial state for the TabContents is blank strings.
  EXPECT_EQ(string16(), contents()->find_prepopulate_text());
  EXPECT_EQ(string16(), contents()->find_text());

  // Get another TabContents object ready.
  TestTabContents contents2(profile_.get(), NULL);

  // No search has still been issued, strings should be blank.
  EXPECT_EQ(string16(), contents()->find_prepopulate_text());
  EXPECT_EQ(string16(), contents()->find_text());
  EXPECT_EQ(string16(), contents2.find_prepopulate_text());
  EXPECT_EQ(string16(), contents2.find_text());

  string16 search_term1 = ASCIIToUTF16(" I had a 401K    ");
  string16 search_term2 = ASCIIToUTF16(" but the economy ");
  string16 search_term3 = ASCIIToUTF16(" eated it.       ");

  // Start searching in the first TabContents, searching forwards but not case
  // sensitive (as indicated by the last two params).
  contents()->StartFinding(search_term1, true, false);

  // Pre-populate string should always match between the two, but find_text
  // should not.
  EXPECT_EQ(search_term1, contents()->find_prepopulate_text());
  EXPECT_EQ(search_term1, contents()->find_text());
  EXPECT_EQ(search_term1, contents2.find_prepopulate_text());
  EXPECT_EQ(string16(), contents2.find_text());

  // Now search in the other TabContents, searching forwards but not case
  // sensitive (as indicated by the last two params).
  contents2.StartFinding(search_term2, true, false);

  // Again, pre-populate string should always match between the two, but
  // find_text should not.
  EXPECT_EQ(search_term2, contents()->find_prepopulate_text());
  EXPECT_EQ(search_term1, contents()->find_text());
  EXPECT_EQ(search_term2, contents2.find_prepopulate_text());
  EXPECT_EQ(search_term2, contents2.find_text());

  // Search again in the first TabContents, searching forwards but not case
  // sensitive (as indicated by the last two params).
  contents()->StartFinding(search_term3, true, false);

  // Once more, pre-populate string should always match between the two, but
  // find_text should not.
  EXPECT_EQ(search_term3, contents()->find_prepopulate_text());
  EXPECT_EQ(search_term3, contents()->find_text());
  EXPECT_EQ(search_term3, contents2.find_prepopulate_text());
  EXPECT_EQ(search_term2, contents2.find_text());
}
