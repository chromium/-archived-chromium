// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_shelf_model.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/test_extension_loader.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/test/in_process_browser_test.h"

namespace {

// The extension we're using as our test case.
const char* kExtensionId = "fc6f6ba6693faf6773c13701019f2e7a12f0febe";

};  // namespace


// An InProcessBrowserTest for testing the ExtensionShelfModel.
// TODO(erikkay) It's unfortunate that this needs to be an in-proc browser test.
// It would be nice to refactor things so that ExtensionShelfModel,
// ExtensionHost and ExtensionsService could run without so much of the browser
// in place.
class ExtensionShelfModelTest : public InProcessBrowserTest,
                                public ExtensionShelfModelObserver {
 public:
  virtual void SetUp() {
    // Initialize the error reporter here, or BrowserMain will create it with
    // the wrong MessageLoop.
    ExtensionErrorReporter::Init(false);
    inserted_count_ = 0;
    removed_count_ = 0;
    moved_count_ = 0;

    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() {
    // Tear down |model_| manually here rather than in the destructor or with
    // a scoped_ptr.  Since it uses NotificationRegistrar, it needs to clean up
    // before the rest of InProcessBrowserTest.
    model_->RemoveObserver(this);
    delete model_;
    model_ = NULL;
    InProcessBrowserTest::TearDown();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableExtensions);
  }

  virtual Browser* CreateBrowser(Profile* profile) {
    Browser* b = InProcessBrowserTest::CreateBrowser(profile);
    model_ = new ExtensionShelfModel(b);
    model_->AddObserver(this);
    return b;
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
  // Get the path to our extension.
  FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions").
      AppendASCII("good").AppendASCII("extension1").AppendASCII("1");
  ASSERT_TRUE(file_util::DirectoryExists(path));  // sanity check

  // Wait for the extension to load and grab a pointer to it.
  TestExtensionLoader loader(browser()->profile());
  Extension* extension = loader.Load(kExtensionId, path);
  ASSERT_TRUE(extension);

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
