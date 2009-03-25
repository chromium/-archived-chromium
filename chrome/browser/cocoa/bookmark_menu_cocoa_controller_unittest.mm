// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/bookmark_menu_cocoa_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface FakeBookmarkMenuController : BookmarkMenuCocoaController {
 @public
  BrowserTestHelper* helper_;
  BookmarkNode* nodes_[2];
  BOOL opened_[2];
}
@end

@implementation FakeBookmarkMenuController

- (id)init {
  if ((self = [super init])) {
    helper_ = new BrowserTestHelper();
    BookmarkModel* model = helper_->GetBrowser()->profile()->GetBookmarkModel();
    nodes_[0] = new BookmarkNode(model, GURL("http://0.com"));
    nodes_[1] = new BookmarkNode(model, GURL("http://1.com"));
  }
  return self;
}

- (void)dealloc {
  delete nodes_[0];
  delete nodes_[1];
  delete helper_;
  [super dealloc];
}

- (BookmarkNode*)nodeForIdentifier:(int)identifier {
  if ((identifier < 0) || (identifier >= 2))
    return NULL;
  return nodes_[identifier];
}

- (void)openURLForNode:(BookmarkNode*)node {
  std::string url = node->GetURL().possibly_invalid_spec();
  if (url.find("http://0.com") != std::string::npos)
    opened_[0] = YES;
  if (url.find("http://1.com") != std::string::npos)
    opened_[1] = YES;
}

@end  // FakeBookmarkMenuController


TEST(BookmarkMenuCocoaControllerTest, TestOpenItem) {
  FakeBookmarkMenuController *c = [[FakeBookmarkMenuController alloc] init];
  NSMenuItem *item = [[[NSMenuItem alloc] init] autorelease];
  for (int i = 0; i < 2; i++) {
    [item setTag:i];
    ASSERT_EQ(c->opened_[i], NO);
    [c openBookmarkMenuItem:item];
    ASSERT_NE(c->opened_[i], NO);
  }
  [c release];
}


