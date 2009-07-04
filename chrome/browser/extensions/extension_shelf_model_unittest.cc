// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_shelf_model.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/extensions/extension_shelf.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/test/in_process_browser_test.h"

namespace {

// The extension we're using as our test case.
const char* kExtensionId = "behllobkkfkfnphdnhnkndlbkcpglgmj";

};  // namespace


// An InProcessBrowserTest for testing the ExtensionShelfModel.
// TODO(erikkay) It's unfortunate that this needs to be an in-proc browser test.
// It would be nice to refactor things so that ExtensionShelfModel,
// ExtensionHost and ExtensionsService could run without so much of the browser
// in place.
class ExtensionShelfModelTest : public ExtensionBrowserTest,
                                public ExtensionShelfModelObserver {
 public:
  virtual void SetUp() {
    inserted_count_ = 0;
    removed_count_ = 0;
    moved_count_ = 0;
    InProcessBrowserTest::SetUp();
  }

  virtual Browser* CreateBrowser(Profile* profile) {
    Browser* b = InProcessBrowserTest::CreateBrowser(profile);
    BrowserView* browser_view = static_cast<BrowserView*>(b->window());
    model_ = browser_view->extension_shelf()->model();
    model_->AddObserver(this);
    return b;
  }

  virtual void CleanUpOnMainThread() {
    model_->RemoveObserver(this);
  }

  virtual void ToolstripInsertedAt(ExtensionHost* toolstrip, int index) {
    inserted_count_++;
  }

  virtual void ToolstripRemovingAt(ExtensionHost* toolstrip, int index) {
    removed_count_++;
  }

  virtual void ToolstripMoved(ExtensionHost* toolstrip,
                              int from_index,
                              int to_index) {
    moved_count_++;
  }

 protected:
  ExtensionShelfModel* model_;

  int inserted_count_;
  int removed_count_;
  int moved_count_;
};

IN_PROC_BROWSER_TEST_F(ExtensionShelfModelTest, Basic) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("good")
                                          .AppendASCII("Extensions")
                                          .AppendASCII(kExtensionId)
                                          .AppendASCII("1.0.0.0")));

  // extension1 has two toolstrips
  EXPECT_EQ(inserted_count_, 2);
  ExtensionHost* one = model_->ToolstripAt(0);
  ExtensionHost* two = model_->ToolstripAt(1);
  EXPECT_EQ(one->GetURL().path(), "/toolstrip1.html");
  EXPECT_EQ(two->GetURL().path(), "/toolstrip2.html");

  model_->MoveToolstripAt(0, 1);
  EXPECT_EQ(two, model_->ToolstripAt(0));
  EXPECT_EQ(one, model_->ToolstripAt(1));
  EXPECT_EQ(moved_count_, 1);

  model_->RemoveToolstripAt(0);
  EXPECT_EQ(one, model_->ToolstripAt(0));
  EXPECT_EQ(1, model_->count());
  EXPECT_EQ(removed_count_, 1);
}
