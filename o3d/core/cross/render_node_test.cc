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


// Tests functionality defined in render_node.cc/h

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

// Basic test fixture.  Simply creates a Client object
// before each test and deletes it after
class RenderNodeBasicTest : public testing::Test {
 protected:

  RenderNodeBasicTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  ServiceDependency<ObjectManager> object_manager_;
  ErrorStatus* error_status_;
  Pack *pack_;
};

void RenderNodeBasicTest::SetUp() {
  error_status_ = new ErrorStatus(g_service_locator);
  pack_ = object_manager_->CreatePack();
}

void RenderNodeBasicTest::TearDown() {
  object_manager_->DestroyPack(pack_);
  delete error_status_;
}

// A class deriving from RenderNode with trivial implementations of the pure
// virtual methods of the RenderNode
class FakeRenderNode : public RenderNode {
 public:
  explicit FakeRenderNode(ServiceLocator* service_locator)
      : RenderNode(service_locator) {
  }

  const String& type() {
    static String kFakeRenderNodeType("FakeRenderNodeType");
    return kFakeRenderNodeType;
  }

  void Render(RenderContext *render_context) {  }
};

// Tests RenderNode::SetParent()
TEST_F(RenderNodeBasicTest, SetParent) {
  // Note: This test assumes that an id of -1 will not alias with ids of nodes
  // created internally by the client.
  FakeRenderNode::Ref render_node(new FakeRenderNode(g_service_locator));

  EXPECT_TRUE(NULL == render_node->parent());

  RenderNode *parent = pack_->Create<RenderNode>();
  render_node->SetParent(parent);

  // Check that the parent pointer was set properly
  EXPECT_EQ(parent, render_node->parent());

  // Check that the node was added as a child to the parent transform
  const RenderNodeArray children = parent->GetChildren();
  EXPECT_EQ(1, children.size());
  EXPECT_EQ(render_node, children[0]);

  // Check that SetParent(NULL) works properly too
  render_node->SetParent(NULL);
  EXPECT_EQ(0, parent->GetChildren().size());
}

TEST_F(RenderNodeBasicTest, SetParentCyclic) {
  // Note: This test assumes that an id of -1 will not alias with ids of nodes
  // created internally by the client.
  FakeRenderNode::Ref render_node(new FakeRenderNode(g_service_locator));

  // Parenting a node to itself must fail.
  error_status_->ClearLastError();
  render_node->SetParent(render_node);
  EXPECT_NE(String(), error_status_->GetLastError());

  // Parenting a node creating a cycle containing one or more nodes must fail.
  RenderNode *parent1 = pack_->Create<RenderNode>();
  error_status_->ClearLastError();
  render_node->SetParent(parent1);
  EXPECT_EQ(String(), error_status_->GetLastError());
  parent1->SetParent(render_node);
  EXPECT_NE(String(), error_status_->GetLastError());

  RenderNode *parent2 = pack_->Create<RenderNode>();
  error_status_->ClearLastError();
  parent1->SetParent(parent2);
  EXPECT_EQ(String(), error_status_->GetLastError());
  parent2->SetParent(render_node);
  EXPECT_NE(String(), error_status_->GetLastError());
}
}
