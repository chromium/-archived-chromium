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

#include "chrome/browser/profile.h"
#include "chrome/browser/template_url.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/browser/views/keyword_editor_view.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

// Base class for keyword editor tests. Creates a profile containing an
// empty TemplateURLModel.
class KeywordEditorViewTest : public testing::Test,
                              public ChromeViews::TableModelObserver {
 public:
  virtual void SetUp() {
    model_changed_count_ = items_changed_count_ = added_count_ =
        removed_count_ = 0;

    profile_.reset(new TestingProfile());
    profile_->CreateTemplateURLModel();

    model_ = profile_->GetTemplateURLModel();

    editor_.reset(new KeywordEditorView(profile_.get()));
    editor_->table_model_->SetObserver(this);
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
    return editor_->table_model_.get();
  }

 protected:
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<KeywordEditorView> editor_;
  TemplateURLModel* model_;

  int model_changed_count_;
  int items_changed_count_;
  int added_count_;
  int removed_count_;
};

// Tests adding a TemplateURL.
TEST_F(KeywordEditorViewTest, Add) {
  editor_->AddTemplateURL(L"a", L"b", L"http://c");

  // Verify the observer was notified.
  VerifyChangeCount(0, 0, 1, 0);
  if (HasFatalFailure())
    return;

  // Verify the TableModel has the new data.
  ASSERT_EQ(1, table_model()->RowCount());

  // Verify the TemplateURLModel has the new entry.
  ASSERT_EQ(1, model_->GetTemplateURLs().size());

  // Verify the entry is what we added.
  const TemplateURL* turl = model_->GetTemplateURLs()[0];
  EXPECT_EQ(L"a", turl->short_name());
  EXPECT_EQ(L"b", turl->keyword());
  EXPECT_TRUE(turl->url() != NULL);
  EXPECT_TRUE(turl->url()->url() == L"http://c");
}

// Tests modifying a TemplateURL.
TEST_F(KeywordEditorViewTest, Modify) {
  editor_->AddTemplateURL(L"a", L"b", L"http://c");
  ClearChangeCount();

  // Modify the entry.
  const TemplateURL* turl = model_->GetTemplateURLs()[0];
  editor_->ModifyTemplateURL(turl, L"a1", L"b1", L"http://c1");

  // Make sure it was updated appropriately.
  VerifyChangeCount(0, 1, 0, 0);
  EXPECT_EQ(L"a1", turl->short_name());
  EXPECT_EQ(L"b1", turl->keyword());
  EXPECT_TRUE(turl->url() != NULL);
  EXPECT_TRUE(turl->url()->url() == L"http://c1");
}

// Tests making a TemplateURL the default search provider.
TEST_F(KeywordEditorViewTest, MakeDefault) {
  editor_->AddTemplateURL(L"a", L"b", L"http://c{searchTerms}");
  ClearChangeCount();

  const TemplateURL* turl = model_->GetTemplateURLs()[0];
  editor_->MakeDefaultSearchProvider(0);
  // Making an item the default sends a handful of changes. Which are sent isn't
  // important, what is important is 'something' is sent.
  ASSERT_TRUE(items_changed_count_ > 0 || added_count_ > 0 ||
              removed_count_ > 0);
  ASSERT_TRUE(model_->GetDefaultSearchProvider() == turl);
}

// Mutates the TemplateURLModel and make sure table model is updating
// appropriately.
TEST_F(KeywordEditorViewTest, MutateTemplateURLModel) {
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
