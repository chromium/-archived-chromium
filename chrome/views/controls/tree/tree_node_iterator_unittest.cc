// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/views/controls/tree/tree_node_iterator.h"
#include "chrome/views/controls/tree/tree_node_model.h"

typedef testing::Test TreeNodeIteratorTest;

using views::TreeNodeWithValue;

TEST_F(TreeNodeIteratorTest, Test) {
  TreeNodeWithValue<int> root;
  root.Add(0, new TreeNodeWithValue<int>(1));
  root.Add(1, new TreeNodeWithValue<int>(2));
  TreeNodeWithValue<int>* f3 = new TreeNodeWithValue<int>(3);
  root.Add(2, f3);
  TreeNodeWithValue<int>* f4 = new TreeNodeWithValue<int>(4);
  f3->Add(0, f4);
  f4->Add(0, new TreeNodeWithValue<int>(5));

  views::TreeNodeIterator<TreeNodeWithValue<int> > iterator(&root);
  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(root.GetChild(0), iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(root.GetChild(1), iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(root.GetChild(2), iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(f4, iterator.Next());

  ASSERT_TRUE(iterator.has_next());
  ASSERT_EQ(f4->GetChild(0), iterator.Next());

  ASSERT_FALSE(iterator.has_next());
}
