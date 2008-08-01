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

#include "base/string_util.h"
#ifdef USE_BOOKMARK_CODEC
#include "chrome/browser/bookmark_codec.h"
#endif  // USE_BOOKMARK_CODEC
#include "chrome/browser/bookmark_bar_model.h"
#include "chrome/test/testing_profile.h"
#include "chrome/views/tree_node_model.h"
#include "testing/gtest/include/gtest/gtest.h"

class BookmarkBarModelTest : public testing::Test,
                             public BookmarkBarModelObserver {
 public:
  struct ObserverDetails {
    ObserverDetails() {
      Set(NULL, NULL, -1, -1);
    }

    void Set(BookmarkBarNode* node1,
             BookmarkBarNode* node2,
             int index1,
             int index2) {
      this->node1 = node1;
      this->node2 = node2;
      this->index1 = index1;
      this->index2 = index2;
    }

    void AssertEquals(BookmarkBarNode* node1,
                      BookmarkBarNode* node2,
                      int index1,
                      int index2) {
      ASSERT_TRUE(this->node1 == node1);
      ASSERT_TRUE(this->node2 == node2);
      ASSERT_EQ(index1, this->index1);
      ASSERT_EQ(index2, this->index2);
    }

    BookmarkBarNode* node1;
    BookmarkBarNode* node2;
    int index1;
    int index2;
  };

  BookmarkBarModelTest() : model(NULL) {
    model.AddObserver(this);
    ClearCounts();
  }


  void Loaded(BookmarkBarModel* model) {
    // We never load from the db, so that this should never get invoked.
    NOTREACHED();
  }

  virtual void BookmarkNodeMoved(BookmarkBarModel* model,
                                 BookmarkBarNode* old_parent,
                                 int old_index,
                                 BookmarkBarNode* new_parent,
                                 int new_index) {
    moved_count++;
    observer_details.Set(old_parent, new_parent, old_index, new_index);
  }

  virtual void BookmarkNodeAdded(BookmarkBarModel* model,
                                 BookmarkBarNode* parent,
                                 int index) {
    added_count++;
    observer_details.Set(parent, NULL, index, -1);
  }

  virtual void BookmarkNodeRemoved(BookmarkBarModel* model,
                                   BookmarkBarNode* parent,
                                   int index) {
    removed_count++;
    observer_details.Set(parent, NULL, index, -1);
  }

  virtual void BookmarkNodeChanged(BookmarkBarModel* model,
                                   BookmarkBarNode* node) {
    changed_count++;
    observer_details.Set(node, NULL, -1, -1);
  }

  virtual void BookmarkNodeFavIconLoaded(BookmarkBarModel* model,
                                         BookmarkBarNode* node) {
    // We never attempt to load favicons, so that this method never
    // gets invoked.
  }

  void ClearCounts() {
    moved_count = added_count = removed_count = changed_count = 0;
  }

  void AssertObserverCount(int added_count,
                           int moved_count,
                           int removed_count,
                           int changed_count) {
    ASSERT_EQ(added_count, this->added_count);
    ASSERT_EQ(moved_count, this->moved_count);
    ASSERT_EQ(removed_count, this->removed_count);
    ASSERT_EQ(changed_count, this->changed_count);
  }

  history::UIStarID GetMaxGroupID() {
    return GetMaxGroupID(model.root_node());
  }

  void AssertNodesEqual(BookmarkBarNode* expected, BookmarkBarNode* actual) {
    ASSERT_TRUE(expected);
    ASSERT_TRUE(actual);
    EXPECT_EQ(expected->GetTitle(), actual->GetTitle());
    EXPECT_EQ(expected->GetType(), actual->GetType());
    EXPECT_TRUE(expected->date_added() == actual->date_added());
    if (expected->GetType() == history::StarredEntry::URL) {
      EXPECT_EQ(expected->GetURL(), actual->GetURL());
    } else {
      EXPECT_TRUE(expected->date_group_modified() ==
                  actual->date_group_modified());
      ASSERT_EQ(expected->GetChildCount(), actual->GetChildCount());
      for (int i = 0; i < expected->GetChildCount(); ++i)
        AssertNodesEqual(expected->GetChild(i), actual->GetChild(i));
    }
  }

  void AssertModelsEqual(BookmarkBarModel* expected,
                         BookmarkBarModel* actual) {
    AssertNodesEqual(expected->GetBookmarkBarNode(),
                     actual->GetBookmarkBarNode());
    AssertNodesEqual(expected->other_node(),
                     actual->other_node());
  }

  BookmarkBarModel model;

  int moved_count;

  int added_count;

  int removed_count;

  int changed_count;

  ObserverDetails observer_details;

 private:
  history::UIStarID GetMaxGroupID(BookmarkBarNode* node) {
    history::UIStarID max_id = node->GetGroupID();
    for (int i = 0; i < node->GetChildCount(); ++i) {
      max_id = std::max(max_id, GetMaxGroupID(node->GetChild(i)));
    }
    return max_id;
  }

  DISALLOW_EVIL_CONSTRUCTORS(BookmarkBarModelTest);
};

TEST_F(BookmarkBarModelTest, InitialState) {
  BookmarkBarNode* root = model.GetBookmarkBarNode();
  ASSERT_TRUE(root != NULL);
  ASSERT_EQ(0, root->GetChildCount());
  ASSERT_EQ(history::StarredEntry::BOOKMARK_BAR, root->GetType());
  ASSERT_EQ(HistoryService::kBookmarkBarID, root->GetStarID());
  ASSERT_EQ(HistoryService::kBookmarkBarID, root->GetGroupID());
}

TEST_F(BookmarkBarModelTest, AddURL) {
  BookmarkBarNode* root = model.GetBookmarkBarNode();
  const std::wstring title(L"foo");
  const GURL url("http://foo.com");

  BookmarkBarNode* new_node = model.AddURL(root, 0, title, url);
  AssertObserverCount(1, 0, 0, 0);
  observer_details.AssertEquals(root, NULL, 0, -1);

  ASSERT_EQ(1, root->GetChildCount());
  ASSERT_EQ(title, new_node->GetTitle());
  ASSERT_TRUE(url == new_node->GetURL());
  ASSERT_EQ(history::StarredEntry::URL, new_node->GetType());
  ASSERT_TRUE(new_node == model.GetNodeByURL(url));
  ASSERT_EQ(new_node->GetGroupID(), new_node->GetEntry().group_id);
  ASSERT_EQ(new_node->GetTitle(), new_node->GetEntry().title);
  ASSERT_EQ(new_node->GetParent()->GetGroupID(),
            new_node->GetEntry().parent_group_id);
  ASSERT_EQ(0, new_node->GetEntry().visual_order);
  ASSERT_TRUE(new_node->GetURL() == new_node->GetEntry().url);
  ASSERT_EQ(history::StarredEntry::URL, new_node->GetEntry().type);
}

TEST_F(BookmarkBarModelTest, AddGroup) {
  BookmarkBarNode* root = model.GetBookmarkBarNode();
  const std::wstring title(L"foo");
  const history::UIStarID max_group_id = GetMaxGroupID();

  BookmarkBarNode* new_node = model.AddGroup(root, 0, title);
  AssertObserverCount(1, 0, 0, 0);
  observer_details.AssertEquals(root, NULL, 0, -1);

  ASSERT_EQ(1, root->GetChildCount());
  ASSERT_EQ(title, new_node->GetTitle());
  ASSERT_EQ(history::StarredEntry::USER_GROUP, new_node->GetType());
  ASSERT_EQ(max_group_id + 1, new_node->GetGroupID());
  ASSERT_EQ(new_node->GetGroupID(), new_node->GetEntry().group_id);
  ASSERT_EQ(new_node->GetTitle(), new_node->GetEntry().title);
  ASSERT_EQ(new_node->GetParent()->GetGroupID(),
            new_node->GetEntry().parent_group_id);
  ASSERT_EQ(0, new_node->GetEntry().visual_order);
  ASSERT_EQ(history::StarredEntry::USER_GROUP, new_node->GetEntry().type);

  // Add another group, just to make sure group_ids are incremented correctly.
  ClearCounts();
  BookmarkBarNode* new_node2 = model.AddGroup(root, 0, title);
  AssertObserverCount(1, 0, 0, 0);
  observer_details.AssertEquals(root, NULL, 0, -1);
  ASSERT_EQ(max_group_id + 2, new_node2->GetGroupID());
}

TEST_F(BookmarkBarModelTest, RemoveURL) {
  BookmarkBarNode* root = model.GetBookmarkBarNode();
  const std::wstring title(L"foo");
  const GURL url("http://foo.com");
  BookmarkBarNode* new_node = model.AddURL(root, 0, title, url);
  ClearCounts();

  model.Remove(root, 0);
  ASSERT_EQ(0, root->GetChildCount());
  AssertObserverCount(0, 0, 1, 0);
  observer_details.AssertEquals(root, NULL, 0, -1);

  // Make sure there is no mapping for the URL.
  ASSERT_TRUE(model.GetNodeByURL(url) == NULL);
}

TEST_F(BookmarkBarModelTest, RemoveGroup) {
  BookmarkBarNode* root = model.GetBookmarkBarNode();
  BookmarkBarNode* group = model.AddGroup(root, 0, L"foo");

  ClearCounts();

  // Add a URL as a child.
  const std::wstring title(L"foo");
  const GURL url("http://foo.com");
  BookmarkBarNode* new_node = model.AddURL(group, 0, title, url);

  ClearCounts();

  // Now remove the group.
  model.Remove(root, 0);
  ASSERT_EQ(0, root->GetChildCount());
  AssertObserverCount(0, 0, 1, 0);
  observer_details.AssertEquals(root, NULL, 0, -1);

  // Make sure there is no mapping for the URL.
  ASSERT_TRUE(model.GetNodeByURL(url) == NULL);
}

TEST_F(BookmarkBarModelTest, VisualOrderIncrements) {
  BookmarkBarNode* root = model.GetBookmarkBarNode();
  BookmarkBarNode* group1 = model.AddGroup(root, 0, L"foo");
  EXPECT_EQ(0, group1->GetEntry().visual_order);
  BookmarkBarNode* group2 = model.AddGroup(root, 0, L"foo");
  EXPECT_EQ(1, group1->GetEntry().visual_order);
  EXPECT_EQ(0, group2->GetEntry().visual_order);
}

TEST_F(BookmarkBarModelTest, SetTitle) {
  BookmarkBarNode* root = model.GetBookmarkBarNode();
  std::wstring title(L"foo");
  const GURL url("http://foo.com");
  BookmarkBarNode* node = model.AddURL(root, 0, title, url);

  ClearCounts();

  title = L"foo2";
  model.SetTitle(node, title);
  AssertObserverCount(0, 0, 0, 1);
  observer_details.AssertEquals(node, NULL, -1, -1);
  EXPECT_EQ(title, node->GetTitle());
}

TEST_F(BookmarkBarModelTest, Move) {
  BookmarkBarNode* root = model.GetBookmarkBarNode();
  std::wstring title(L"foo");
  const GURL url("http://foo.com");
  BookmarkBarNode* node = model.AddURL(root, 0, title, url);
  BookmarkBarNode* group1 = model.AddGroup(root, 0, L"foo");
  ClearCounts();

  model.Move(node, group1, 0);

  AssertObserverCount(0, 1, 0, 0);
  observer_details.AssertEquals(root, group1, 1, 0);
  EXPECT_TRUE(group1 == node->GetParent());
  EXPECT_EQ(1, root->GetChildCount());
  EXPECT_EQ(group1, root->GetChild(0));
  EXPECT_EQ(1, group1->GetChildCount());
  EXPECT_EQ(node, group1->GetChild(0));

  // And remove the group.
  ClearCounts();
  model.Remove(root, 0);
  AssertObserverCount(0, 0, 1, 0);
  observer_details.AssertEquals(root, NULL, 0, -1);
  EXPECT_TRUE(model.GetNodeByURL(url) == NULL);
  EXPECT_EQ(0, root->GetChildCount());
}

TEST_F(BookmarkBarModelTest, RemoveFromBookmarkBar) {
  BookmarkBarNode* root = model.GetBookmarkBarNode();
  const std::wstring title(L"foo");
  const GURL url("http://foo.com");

  BookmarkBarNode* new_node = model.AddURL(root, 0, title, url);

  ClearCounts();

  model.RemoveFromBookmarkBar(new_node);
  AssertObserverCount(0, 1, 0, 0);
  observer_details.AssertEquals(root, model.other_node(), 0, 0);

  ASSERT_EQ(0, root->GetChildCount());
}

// Tests that adding a URL to a folder updates the last modified time.
TEST_F(BookmarkBarModelTest, ParentForNewNodes) {
  ASSERT_EQ(model.GetBookmarkBarNode(), model.GetParentForNewNodes());

  const std::wstring title(L"foo");
  const GURL url("http://foo.com");

  BookmarkBarNode* new_node =
      model.AddURL(model.other_node(), 0, title, url);

  ASSERT_EQ(model.other_node(), model.GetParentForNewNodes());
}

// Make sure recently modified stays in sync when adding a URL.
TEST_F(BookmarkBarModelTest, MostRecentlyModifiedGroups) {
  // Add a group.
  BookmarkBarNode* group = model.AddGroup(model.other_node(), 0, L"foo");
  // Add a URL to it.
  model.AddURL(group, 0, L"blah", GURL("http://foo.com"));

  // Make sure group is in the most recently modified.
  std::vector<BookmarkBarNode*> most_recent_groups =
      model.GetMostRecentlyModifiedGroups(1);
  ASSERT_EQ(1, most_recent_groups.size());
  ASSERT_EQ(group, most_recent_groups[0]);

  // Nuke the group and do another fetch, making sure group isn't in the
  // returned list.
  model.Remove(group->GetParent(), 0);
  most_recent_groups = model.GetMostRecentlyModifiedGroups(1);
  ASSERT_EQ(1, most_recent_groups.size());
  ASSERT_TRUE(most_recent_groups[0] != group);
}

namespace {

// See comment in PopulateNodeFromString.
typedef ChromeViews::TreeNodeWithValue<history::StarredEntry::Type> TestNode;

// Does the work of PopulateNodeFromString. index gives the index of the current
// element in description to process.
static void PopulateNodeImpl(const std::vector<std::wstring>& description,
                             size_t* index,
                             TestNode* parent) {
  while (*index < description.size()) {
    const std::wstring& element = description[*index];
    (*index)++;
    if (element == L"[") {
      // Create a new group and recurse to add all the children.
      // Groups are given a unique named by way of an ever increasing integer
      // value. The groups need not have a name, but one is assigned to help
      // in debugging.
      static int next_group_id = 1;
      TestNode* new_node =
          new TestNode(IntToWString(next_group_id++),
                       history::StarredEntry::USER_GROUP);
      parent->Add(parent->GetChildCount(), new_node);
      PopulateNodeImpl(description, index, new_node);
    } else if (element == L"]") {
      // End the current group.
      return;
    } else {
      // Add a new URL.

      // All tokens must be space separated. If there is a [ or ] in the name it
      // likely means a space was forgotten.
      DCHECK(element.find('[') == std::string::npos);
      DCHECK(element.find(']') == std::string::npos);
      parent->Add(parent->GetChildCount(),
                  new TestNode(element, history::StarredEntry::URL));
    }
  }
}

// Creates and adds nodes to parent based on description. description consists
// of the following tokens (all space separated):
//   [ : creates a new USER_GROUP node. All elements following the [ until the
//       next balanced ] is encountered are added as children to the node.
//   ] : closes the last group created by [ so that any further nodes are added
//       to the current groups parent.
//   text: creates a new URL node.
// For example, "a [b] c" creates the following nodes:
//   a 1 c
//     |
//     b
// In words: a node of type URL with the title a, followed by a group node with
// the title 1 having the single child of type url with name b, followed by
// the url node with the title c.
//
// NOTE: each name must be unique, and groups are assigned a unique title by way
// of an increasing integer.
static void PopulateNodeFromString(const std::wstring& description,
                                   TestNode* parent) {
  std::vector<std::wstring> elements;
  size_t index = 0;
  SplitStringAlongWhitespace(description, &elements);
  PopulateNodeImpl(elements, &index, parent);
}

// Populates the BookmarkBarNode with the children of parent.
static void PopulateBookmarkBarNode(TestNode* parent,
                                    BookmarkBarModel* model,
                                    BookmarkBarNode* bb_node) {
  for (int i = 0; i < parent->GetChildCount(); ++i) {
    TestNode* child = parent->GetChild(i);
    if (child->value == history::StarredEntry::USER_GROUP) {
      BookmarkBarNode* new_bb_node =
          model->AddGroup(bb_node, i, child->GetTitle());
      PopulateBookmarkBarNode(child, model, new_bb_node);
    } else {
      model->AddURL(bb_node, i, child->GetTitle(),
                    GURL("http://" + WideToASCII(child->GetTitle())));
    }
  }
}

}  // namespace

// Test class that creates a BookmarkBarModel with a real history backend.
class BookmarkBarModelTestWithProfile : public testing::Test,
                                        public BookmarkBarModelObserver {
 public:
  BookmarkBarModelTestWithProfile() {}

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->CreateHistoryService();
  }

  virtual void TearDown() {
    profile_.reset(NULL);
  }

  // The profile.
  scoped_ptr<TestingProfile> profile_;

 protected:
  // Verifies the contents of the bookmark bar node match the contents of the
  // TestNode.
  void VerifyModelMatchesNode(TestNode* expected, BookmarkBarNode* actual) {
    EXPECT_EQ(expected->GetChildCount(), actual->GetChildCount());
    for (int i = 0; i < expected->GetChildCount(); ++i) {
      TestNode* expected_child = expected->GetChild(i);
      BookmarkBarNode* actual_child = actual->GetChild(i);
      EXPECT_EQ(expected_child->GetTitle(), actual_child->GetTitle());
      if (expected_child->value == history::StarredEntry::USER_GROUP) {
        EXPECT_TRUE(actual_child->GetType() ==
                    history::StarredEntry::USER_GROUP);
        // Recurse throught children.
        VerifyModelMatchesNode(expected_child, actual_child);
      } else {
        // No need to check the URL, just the title is enough.
        EXPECT_TRUE(actual_child->GetType() ==
                    history::StarredEntry::URL);
      }
    }
  }

  // Creates the bookmark bar model. If the bookmark bar model has already been
  // created a new one is created and the old one destroyed.
  void CreateBookmarkBarModel() {
    // NOTE: this order is important. We need to make sure the backend has
    // processed all pending requests from the current BookmarkBarModel before
    // destroying it. By creating a new BookmarkBarModel first and blocking
    // until done, we ensure all requests from the old model have completed.
    BookmarkBarModel* new_model = new BookmarkBarModel(profile_.get());
    BlockTillLoaded(new_model);
    bb_model_.reset(new_model);
  }

  // Destroys the bookmark bar model, then the profile, then creates a new
  // profile.
  void RecreateProfile() {
    bb_model_.reset(NULL);
    // Need to shutdown the old one before creating a new one.
    profile_.reset(NULL);
    profile_.reset(new TestingProfile());
    profile_->CreateHistoryService();
  }

  scoped_ptr<BookmarkBarModel> bb_model_;

 private:
  // Blocks until the BookmarkBarModel has finished loading.
  void BlockTillLoaded(BookmarkBarModel* model) {
    model->AddObserver(this);
    MessageLoop::current()->Run();
  }

  // BookmarkBarModelObserver methods.
  virtual void Loaded(BookmarkBarModel* model) {
    // Balances the call in BlockTillLoaded.
    MessageLoop::current()->Quit();
  }
  virtual void BookmarkNodeMoved(BookmarkBarModel* model,
                                 BookmarkBarNode* old_parent,
                                 int old_index,
                                 BookmarkBarNode* new_parent,
                                 int new_index) {}
  virtual void BookmarkNodeAdded(BookmarkBarModel* model,
                                 BookmarkBarNode* parent,
                                 int index) {}
  virtual void BookmarkNodeRemoved(BookmarkBarModel* model,
                                   BookmarkBarNode* parent,
                                   int index) {}
  virtual void BookmarkNodeChanged(BookmarkBarModel* model,
                                   BookmarkBarNode* node) {}
  virtual void BookmarkNodeFavIconLoaded(BookmarkBarModel* model,
                                         BookmarkBarNode* node) {}

  DISALLOW_EVIL_CONSTRUCTORS(BookmarkBarModelTestWithProfile);
};

// Creates a set of nodes in the bookmark bar model, then recreates the
// bookmark bar model which triggers loading from the db and checks the loaded
// structure to make sure it is what we first created.
TEST_F(BookmarkBarModelTestWithProfile, CreateAndRestore) {
  struct TestData {
    // Structure of the children of the bookmark bar model node.
    const std::wstring bbn_contents;
    // Structure of the children of the other node.
    const std::wstring other_contents;
  } data[] = {
    // See PopulateNodeFromString for a description of these strings.
    { L"", L"" },
    { L"a", L"b" },
    { L"a [ b ]", L"" },
    { L"", L"[ b ] a [ c [ d e [ f ] ] ]" },
    { L"a [ b ]", L"" },
    { L"a b c [ d e [ f ] ]", L"g h i [ j k [ l ] ]"},
  };
  for (int i = 0; i < arraysize(data); ++i) {
    RecreateProfile();

    CreateBookmarkBarModel();

    TestNode bbn;
    PopulateNodeFromString(data[i].bbn_contents, &bbn);
    PopulateBookmarkBarNode(&bbn, bb_model_.get(),
                            bb_model_->GetBookmarkBarNode());

    TestNode other;
    PopulateNodeFromString(data[i].other_contents, &other);
    PopulateBookmarkBarNode(&other, bb_model_.get(), bb_model_->other_node());

    CreateBookmarkBarModel();

    VerifyModelMatchesNode(&bbn, bb_model_->GetBookmarkBarNode());
    VerifyModelMatchesNode(&other, bb_model_->other_node());
  }
}

#ifdef USE_BOOKMARK_CODEC
// Creates a set of nodes in the bookmark bar model, then recreates the
// bookmark bar model which triggers loading from the db and checks the loaded
// structure to make sure it is what we first created.
TEST_F(BookmarkBarModelTest, TestJSONCodec) {
  struct TestData {
    // Structure of the children of the bookmark bar model node.
    const std::wstring bbn_contents;
    // Structure of the children of the other node.
    const std::wstring other_contents;
  } data[] = {
    // See PopulateNodeFromString for a description of these strings.
    { L"", L"" },
    { L"a", L"b" },
    { L"a [ b ]", L"" },
    { L"", L"[ b ] a [ c [ d e [ f ] ] ]" },
    { L"a [ b ]", L"" },
    { L"a b c [ d e [ f ] ]", L"g h i [ j k [ l ] ]"},
  };
  for (int i = 0; i < arraysize(data); ++i) {
    BookmarkBarModel expected_model(NULL);

    TestNode bbn;
    PopulateNodeFromString(data[i].bbn_contents, &bbn);
    PopulateBookmarkBarNode(&bbn, &expected_model,
                            expected_model.GetBookmarkBarNode());

    TestNode other;
    PopulateNodeFromString(data[i].other_contents, &other);
    PopulateBookmarkBarNode(&other, &expected_model,
                            expected_model.other_node());

    BookmarkBarModel actual_model(NULL);
    BookmarkCodec codec;
    scoped_ptr<Value> encoded_value(codec.Encode(&expected_model));
    codec.Decode(&actual_model, *(encoded_value.get()));

    AssertModelsEqual(&expected_model, &actual_model);
  }
}
#endif  // USE_BOOKMARK_CODEC
