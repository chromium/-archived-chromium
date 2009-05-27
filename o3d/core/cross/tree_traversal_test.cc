/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file implements unit tests for class TreeTraveral.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/tree_traversal.h"

namespace o3d {

class TreeTraversalTest : public testing::Test {
 protected:
  TreeTraversalTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  TransformationContext* transformation_context_;
  DrawListManager* draw_list_manager_;
  Pack *pack_;
};

void TreeTraversalTest::SetUp() {
  transformation_context_ = new TransformationContext(g_service_locator);
  draw_list_manager_ = new DrawListManager(g_service_locator);
  pack_ = object_manager_->CreatePack();
}

void TreeTraversalTest::TearDown() {
  pack_->Destroy();
  delete draw_list_manager_;
  delete transformation_context_;
}

TEST_F(TreeTraversalTest, Basic) {
  TreeTraversal* tree_traversal = pack()->Create<TreeTraversal>();
  // Check that tree_traversal got created.
  ASSERT_TRUE(tree_traversal != NULL);

  DrawList* draw_list1 = pack()->Create<DrawList>();
  DrawList* draw_list2 = pack()->Create<DrawList>();
  DrawList* draw_list3 = pack()->Create<DrawList>();
  DrawContext* draw_context = pack()->Create<DrawContext>();
  ASSERT_TRUE(draw_list1 != NULL);
  ASSERT_TRUE(draw_list2 != NULL);
  ASSERT_TRUE(draw_list3 != NULL);
  ASSERT_TRUE(draw_context != NULL);

  // Check that we can add and remove them
  tree_traversal->RegisterDrawList(draw_list1, draw_context, true);
  tree_traversal->RegisterDrawList(draw_list2, draw_context, true);

  EXPECT_FALSE(tree_traversal->UnregisterDrawList(draw_list3));
  EXPECT_TRUE(tree_traversal->UnregisterDrawList(draw_list1));
  EXPECT_FALSE(tree_traversal->UnregisterDrawList(draw_list1));
  EXPECT_TRUE(tree_traversal->UnregisterDrawList(draw_list2));
  EXPECT_FALSE(tree_traversal->UnregisterDrawList(draw_list3));
}

}  // namespace o3d
