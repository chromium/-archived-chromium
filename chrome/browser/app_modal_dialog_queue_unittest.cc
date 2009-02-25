// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/app_modal_dialog_delegate.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestModalDialogDelegate : public AppModalDialogDelegate {
 public:
  TestModalDialogDelegate() {}
  virtual ~TestModalDialogDelegate() {}

  // Overridden from AppModalDialogDelegate:
  virtual void ShowModalDialog() {}
  virtual void ActivateModalDialog() {}
  virtual AppModalDialogDelegateTesting* GetTestingInterface() { return NULL; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestModalDialogDelegate);
};

}  // namespace

TEST(AppModalDialogQueueTest, MultipleDialogTest) {
  TestModalDialogDelegate modal_dialog1, modal_dialog2;
  AppModalDialogQueue::AddDialog(&modal_dialog1);
  AppModalDialogQueue::AddDialog(&modal_dialog2);

  EXPECT_TRUE(AppModalDialogQueue::HasActiveDialog());
  EXPECT_EQ(&modal_dialog1, AppModalDialogQueue::active_dialog());

  AppModalDialogQueue::ShowNextDialog();

  EXPECT_TRUE(AppModalDialogQueue::HasActiveDialog());
  EXPECT_EQ(&modal_dialog2, AppModalDialogQueue::active_dialog());

  AppModalDialogQueue::ShowNextDialog();

  EXPECT_FALSE(AppModalDialogQueue::HasActiveDialog());
  EXPECT_EQ(NULL, AppModalDialogQueue::active_dialog());
}
