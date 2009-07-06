// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests the cross platform BlockedPopupContainer model/controller object.
//
// TODO(erg): The unit tests on BlockedPopupContainer need to be greatly
// expanded.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "app/app_paths.h"
#include "base/path_service.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/tab_contents/test_web_contents.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/test/testing_profile.h"
#include "net/base/net_util.h"

namespace {
const std::string host1 = "host1";
}  // namespace

// Mock for our view.
class MockBlockedPopupContainerView : public BlockedPopupContainerView {
 public:
  MOCK_METHOD0(SetPosition, void());
  MOCK_METHOD0(ShowView, void());
  MOCK_METHOD0(UpdateLabel, void());
  MOCK_METHOD0(HideView, void());
  MOCK_METHOD0(Destroy, void());
};

class BlockedPopupContainerTest : public RenderViewHostTestHarness {
 public:
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

 protected:
  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    container_ = new BlockedPopupContainer(contents(), profile()->GetPrefs());
    container_->set_view(&mock);

    contents_->set_blocked_popup_container(container_);
  }

  // Our blocked popup container that we are testing. WARNING: If you are
  // trying to test destruction issues, make sure to remove |container_| from
  // |contents_|.
  BlockedPopupContainer* container_;

  // The mock that we're using.
  MockBlockedPopupContainerView mock;
};

// Destroying the container should tell the View to destroy itself.
TEST_F(BlockedPopupContainerTest, TestDestroy) {
  EXPECT_CALL(mock, Destroy()).Times(1);
}

// Make sure TabContents::RepositionSupressedPopupsToFit() filters to the view.
TEST_F(BlockedPopupContainerTest, TestReposition) {
  EXPECT_CALL(mock, SetPosition()).Times(1);
  // Always need this to shut gmock up. :-/
  EXPECT_CALL(mock, Destroy()).Times(1);

  contents_->RepositionSupressedPopupsToFit();
}

// Test the basic blocked popup case.
TEST_F(BlockedPopupContainerTest, BasicCase) {
  EXPECT_CALL(mock, UpdateLabel()).Times(1);
  EXPECT_CALL(mock, ShowView()).Times(1);
  EXPECT_CALL(mock, Destroy()).Times(1);

  // Create another TabContents representing the blocked popup case.
  TabContents* popup = BuildTabContents();
  popup->controller().LoadURLLazily(GetTestCase("error"), GURL(),
                                    PageTransition::LINK,
                                    L"", NULL);
  container_->AddTabContents(popup, gfx::Rect(), host1);

  EXPECT_EQ(container_->GetBlockedPopupCount(), static_cast<size_t>(1));
  EXPECT_EQ(container_->GetTabContentsAt(0), popup);
  EXPECT_FALSE(container_->IsHostWhitelisted(0));
  EXPECT_THAT(container_->GetHosts(), testing::Contains(host1));
}
