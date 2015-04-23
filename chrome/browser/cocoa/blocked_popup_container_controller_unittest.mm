// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "app/app_paths.h"
#include "base/path_service.h"
#include "base/scoped_nsautorelease_pool.h"
#import "chrome/browser/cocoa/blocked_popup_container_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {
const std::string host1 = "host1";
}  // namespace

class BlockedPopupContainerControllerTest : public RenderViewHostTestHarness {
 public:
  virtual void SetUp() {
    // This is all a bit convoluted because the standard factory Create() call
    // doesn't give us access to the cocoa controller for testing (since it's
    // an internal implementation detail). As a result, we need to create one
    // separately and inject the bridge with a test-only Create() call.
    // Unfortunate, but no way around it.
    RenderViewHostTestHarness::SetUp();
    cocoa_controller_ = [[BlockedPopupContainerController alloc]
                          initWithContainer:nil];
    EXPECT_TRUE([cocoa_controller_ bridge]);
    container_ = BlockedPopupContainer::Create(contents(), profile(),
                                               [cocoa_controller_ bridge]);
    [cocoa_controller_ setContainer:container_];
    contents_->set_blocked_popup_container(container_);
  }

  virtual void TearDown() {
    // This will also signal the Cocoa controller to delete itself with a
    // Destroy() mesage to the bridge. It also clears out the association with
    // |contents_|.
    container_->Destroy();
    RenderViewHostTestHarness::TearDown();
  }

  TabContents* BuildTabContents() {
    // This will be deleted when the TabContents goes away.
    SiteInstance* instance = SiteInstance::CreateSiteInstance(profile_.get());

    // Set up and use TestTabContents here.
    return new TestTabContents(profile_.get(), instance);
  }

  GURL GetTestCase(const std::string& file) {
    FilePath filename;
    PathService::Get(app::DIR_TEST_DATA, &filename);
    filename = filename.AppendASCII("constrained_files");
    filename = filename.AppendASCII(file);
    return net::FilePathToFileURL(filename);
  }

  base::ScopedNSAutoreleasePool pool;
  BlockedPopupContainer* container_;
  BlockedPopupContainerController* cocoa_controller_;
};

TEST_F(BlockedPopupContainerControllerTest, BasicPopupBlock) {
  // This is taken from the popup blocker unit test.
  TabContents* popup = BuildTabContents();
  popup->controller().LoadURLLazily(GetTestCase("error"), GURL(),
                                    PageTransition::LINK,
                                    L"", NULL);
  container_->AddTabContents(popup, gfx::Rect(), host1);
  EXPECT_EQ(container_->GetBlockedPopupCount(), static_cast<size_t>(1));
  EXPECT_EQ(container_->GetTabContentsAt(0), popup);
  EXPECT_FALSE(container_->IsHostWhitelisted(0));

  // Ensure the view has been displayed. If it has a superview, then ShowView()
  // has been called on the bridge. If the button has a string, then
  // UpdateLabel() has been called.
  EXPECT_TRUE([cocoa_controller_ view]);
  EXPECT_TRUE([[cocoa_controller_ view] superview]);
  EXPECT_TRUE([[[cocoa_controller_ popupButton] title] length] > 0);

  // Validate the menu. It should have 4 items (the dummy title item, 1 poupup,
  // a separator, 1 host).
  NSMenu* menu = [cocoa_controller_ buildMenu];
  EXPECT_TRUE(menu);
  EXPECT_EQ([menu numberOfItems], 4);

  // Change the whitelisting and make sure the host is checked.
  container_->ToggleWhitelistingForHost(0);
  menu = [cocoa_controller_ buildMenu];
  EXPECT_TRUE(menu);
  EXPECT_EQ([menu numberOfItems], 2);
  EXPECT_EQ([[menu itemAtIndex:1] state], NSOnState);

  // Close the popup and verify it's no longer in the view hierarchy. This
  // means HideView() has been called.
  [cocoa_controller_ closePopup:nil];
  EXPECT_FALSE([[cocoa_controller_ view] superview]);
}
