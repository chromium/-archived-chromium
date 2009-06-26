// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/table_model_observer.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/keyword_editor_controller.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_table_model.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

// Base class for keyword editor tests. Creates a profile containing an
// empty TemplateURLModel.
class KeywordEditorControllerTest : public testing::Test,
                                    public TableModelObserver {
 public:
  virtual void SetUp() {
    model_changed_count_ = items_changed_count_ = added_count_ =
        removed_count_ = 0;

    profile_.reset(new TestingProfile());
    profile_->CreateTemplateURLModel();

    model_ = profile_->GetTemplateURLModel();

    controller_.reset(new KeywordEditorController(profile_.get()));
    controller_->table_model()->SetObserver(this);
  }

  virtual void OnModelChanged() {
    model_changed_count_++;
  }

  virtual void OnItemsChanged(int start, int length) {
    items_changed_count_++;
  }

  virtual void OnItemsAdded(int start, int length) {
    added_count_++;
  }

  virtual void OnItemsRemoved(int start, int length) {
    removed_count_++;
  }

  void VerifyChangeCount(int model_changed_count, int item_changed_count,
                         int added_count, int removed_count) {
    ASSERT_EQ(model_changed_count, model_changed_count_);
    ASSERT_EQ(item_changed_count, items_changed_count_);
    ASSERT_EQ(added_count, added_count_);
    ASSERT_EQ(removed_count, removed_count_);
    ClearChangeCount();
  }

  void ClearChangeCount() {
    model_changed_count_ = items_changed_count_ = added_count_ =
        removed_count_ = 0;
  }

  TemplateURLTableModel* table_model() const {
    return controller_->table_model();
  }

 protected:
  MessageLoopForUI message_loop_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<KeywordEditorController> controller_;
  TemplateURLModel* model_;

  int model_changed_count_;
  int items_changed_count_;
  int added_count_;
  int removed_count_;
};

// Tests adding a TemplateURL.
TEST_F(KeywordEditorControllerTest, Add) {
  controller_->AddTemplateURL(L"a", L"b", L"http://c");

  // Verify the observer was notified.
  VerifyChangeCount(0, 0, 1, 0);
  if (HasFatalFailure())
    return;

  // Verify the TableModel has the new data.
  ASSERT_EQ(1, table_model()->RowCount());

  // Verify the TemplateURLModel has the new entry.
  ASSERT_EQ(1U, model_->GetTemplateURLs().size());

  // Verify the entry is what we added.
  const TemplateURL* turl = model_->GetTemplateURLs()[0];
  EXPECT_EQ(L"a", turl->short_name());
  EXPECT_EQ(L"b", turl->keyword());
  EXPECT_TRUE(turl->url() != NULL);
  EXPECT_TRUE(turl->url()->url() == L"http://c");
}

// Tests modifying a TemplateURL.
TEST_F(KeywordEditorControllerTest, Modify) {
  controller_->AddTemplateURL(L"a", L"b", L"http://c");
  ClearChangeCount();

  // Modify the entry.
  const TemplateURL* turl = model_->GetTemplateURLs()[0];
  controller_->ModifyTemplateURL(turl, L"a1", L"b1", L"http://c1");

  // Make sure it was updated appropriately.
  VerifyChangeCount(0, 1, 0, 0);
  EXPECT_EQ(L"a1", turl->short_name());
  EXPECT_EQ(L"b1", turl->keyword());
  EXPECT_TRUE(turl->url() != NULL);
  EXPECT_TRUE(turl->url()->url() == L"http://c1");
}

// Tests making a TemplateURL the default search provider.
TEST_F(KeywordEditorControllerTest, MakeDefault) {
  controller_->AddTemplateURL(L"a", L"b", L"http://c{searchTerms}");
  ClearChangeCount();

  const TemplateURL* turl = model_->GetTemplateURLs()[0];
  controller_->MakeDefaultTemplateURL(0);
  // Making an item the default sends a handful of changes. Which are sent isn't
  // important, what is important is 'something' is sent.
  ASSERT_TRUE(items_changed_count_ > 0 || added_count_ > 0 ||
              removed_count_ > 0);
  ASSERT_TRUE(model_->GetDefaultSearchProvider() == turl);
}

// Mutates the TemplateURLModel and make sure table model is updating
// appropriately.
TEST_F(KeywordEditorControllerTest, MutateTemplateURLModel) {
  TemplateURL* turl = new TemplateURL();
  turl->set_keyword(L"a");
  turl->set_short_name(L"b");
  model_->Add(turl);

  // Table model should have updated.
  VerifyChangeCount(1, 0, 0, 0);

  // And should contain the newly added TemplateURL.
  ASSERT_EQ(1, table_model()->RowCount());
  ASSERT_EQ(0, table_model()->IndexOfTemplateURL(turl));
}
