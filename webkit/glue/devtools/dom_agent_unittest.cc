// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CString.h"
#include "Document.h"
#include "EventListener.h"
#include "HTMLFrameOwnerElement.h"
#include "Node.h"
#include "PlatformString.h"
#include "Text.h"
#include <wtf/OwnPtr.h>
#undef LOG

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/values.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/glue/devtools/devtools_mock_rpc.h"
#include "webkit/glue/devtools/devtools_rpc.h"
#include "webkit/glue/devtools/dom_agent_impl.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell_test.h"

using WebCore::Document;
using WebCore::Element;
using WebCore::ExceptionCode;
using WebCore::HTMLFrameOwnerElement;
using WebCore::Node;
using WebCore::String;
using WebCore::Text;

namespace {

class MockDomAgentDelegate : public DomAgentDelegateStub,
                             public DevToolsMockRpc {
 public:
  MockDomAgentDelegate() : DomAgentDelegateStub(NULL) {
    set_delegate(this);
  }
  ~MockDomAgentDelegate() {}
};

class DomAgentTests : public TestShellTest {
 public:
  DomAgentTests() : ec_(0) {}

 protected:
  // testing::Test
  virtual void SetUp() {
    TestShellTest::SetUp();
    test_shell_->ResetTestController();
    GURL file_url = net::FilePathToFileURL(data_dir_);
    WebFrame* main_frame = test_shell_->webView()->GetMainFrame();
    main_frame->LoadHTMLString("<html> <head> </head> <body>  </body> </html>",
        file_url);
    WebFrameImpl* main_frame_impl = static_cast<WebFrameImpl*>(main_frame);

    document_ = main_frame_impl->frame()->document();
    Node* html = document_->documentElement();
    head_ = static_cast<Element*>(html->firstChild());
    body_ = static_cast<Element*>(head_->nextSibling());
    mock_delegate_.set(new MockDomAgentDelegate());
    dom_agent_.set(new DomAgentImpl(mock_delegate_.get()));
    dom_agent_->SetDocument(document_.get());
  }

  virtual void TearDown() {
    TestShellTest::TearDown();
    dom_agent_.set(NULL);
    body_ = NULL;
    document_ = NULL;
  }

  static const int kHtmlElemId = 1;
  static const int kHeadElemId = 2;
  static const int kBodyElemId = 3;
  enum {
    kCallIdAny = 0,
    kCallId1,
    kCallId2,
    kCallId3,
    kCallId4
  };

  RefPtr<Document> document_;
  RefPtr<Element> head_;
  RefPtr<Element> body_;
  OwnPtr<DomAgentImpl> dom_agent_;
  ExceptionCode ec_;
  OwnPtr<MockDomAgentDelegate> mock_delegate_;
};

// Requests document node and tests that the callback with the serialized
// version is called.
TEST_F(DomAgentTests, GetDocumentElement) {
  OwnPtr<Value> v(DevToolsRpc::ParseMessage("[1,1,\"HTML\",\"\",[],2]"));
  mock_delegate_->SetDocumentElement(*v.get());
  mock_delegate_->Replay();

  dom_agent_->GetDocumentElement();
  mock_delegate_->Verify();
}

// Requests element's children and tests that the callback with the serialized
// version is called.
TEST_F(DomAgentTests, GetChildNodes) {
  dom_agent_->GetDocumentElement();
  mock_delegate_->Reset();

  OwnPtr<Value> v(DevToolsRpc::ParseMessage(
      "[[2,1,\"HEAD\",\"\",[],0],[3,1,\"BODY\",\"\",[],0]]"));
  mock_delegate_->SetChildNodes(kHtmlElemId, *v.get());
  mock_delegate_->DidGetChildNodes(kCallId2);
  mock_delegate_->Replay();

  dom_agent_->GetChildNodes(kCallId2, kHtmlElemId);
  mock_delegate_->Verify();
}

// Tests that "child node inserted" event is being fired.
TEST_F(DomAgentTests, ChildNodeInsertedUnknownParent) {
  dom_agent_->GetDocumentElement();
  mock_delegate_->Reset();

  // There should be no events fired until parent node is known to client.
  RefPtr<Element> div = document_->createElement("DIV", ec_);
  body_->appendChild(div, ec_);
  mock_delegate_->Replay();
  mock_delegate_->Verify();
}

// Tests that "child node inserted" event is being fired.
TEST_F(DomAgentTests, ChildNodeInsertedKnownParent) {
  dom_agent_->GetDocumentElement();
  dom_agent_->GetChildNodes(kCallId2, kHtmlElemId);
  mock_delegate_->Reset();

  // There should be an event fired in case parent node is known to client,
  // but the event should not be specific.
  mock_delegate_->HasChildrenUpdated(kBodyElemId, true);
  mock_delegate_->Replay();

  RefPtr<Element> div = document_->createElement("DIV", ec_);
  body_->appendChild(div, ec_);
  mock_delegate_->Verify();
}

// Tests that "child node inserted" event is being fired.
TEST_F(DomAgentTests, ChildNodeInsertedKnownChildren) {
  dom_agent_->GetDocumentElement();
  dom_agent_->GetChildNodes(kCallId2, kHtmlElemId);
  dom_agent_->GetChildNodes(kCallId3, kBodyElemId);
  mock_delegate_->Reset();

  // There should be an event fired in case parent node is known to client,
  // Since children were already requested, event should have all the
  // new child data.
  OwnPtr<Value> v(DevToolsRpc::ParseMessage("[4,1,\"DIV\",\"\",[],0]"));
  mock_delegate_->ChildNodeInserted(kBodyElemId, 0, *v.get());
  mock_delegate_->Replay();

  // Blank text should be transparent.
  RefPtr<Text> text = document_->createTextNode("    ");
  body_->appendChild(text, ec_);

  RefPtr<Element> div = document_->createElement("DIV", ec_);
  body_->appendChild(div, ec_);
  mock_delegate_->Verify();
}

// Tests that "child node inserted" event is being fired after push path to
// node request.
TEST_F(DomAgentTests, ChildNodeInsertedAfterPushPathToNode) {
  RefPtr<Element> div = document_->createElement("DIV", ec_);
  body_->appendChild(div, ec_);

  dom_agent_->GetDocumentElement();
  dom_agent_->PushNodePathToClient(div.get());
  mock_delegate_->Reset();

  // Since children were already requested via path to node, event should have
  // all the new child data.
  OwnPtr<Value> v(DevToolsRpc::ParseMessage("[5,1,\"DIV\",\"\",[],0]"));
  mock_delegate_->ChildNodeInserted(kBodyElemId, 4, *v.get());
  mock_delegate_->Replay();

  RefPtr<Element> div2 = document_->createElement("DIV", ec_);
  body_->appendChild(div2, ec_);
  mock_delegate_->Verify();
}

// Tests that "child node inserted" event is being fired.
TEST_F(DomAgentTests, ChildNodePrepend) {
  RefPtr<Element> div = document_->createElement("DIV", ec_);
  body_->appendChild(div, ec_);

  dom_agent_->GetDocumentElement();
  dom_agent_->GetChildNodes(kCallId2, kHtmlElemId);
  dom_agent_->GetChildNodes(kCallId3, kBodyElemId);
  mock_delegate_->Reset();

  // There should be an event fired in case parent node is known to client,
  // Since children were already requested, event should have all the
  // new child data.
  OwnPtr<Value> v(DevToolsRpc::ParseMessage("[5,1,\"DIV\",\"\",[],0]"));
  mock_delegate_->ChildNodeInserted(kBodyElemId, 0, *v.get());
  mock_delegate_->Replay();

  RefPtr<Element> new_div = document_->createElement("DIV", ec_);
  body_->insertBefore(new_div, div.get(), ec_, false);
  mock_delegate_->Verify();
}

// Tests that "child node inserted" event is being fired.
TEST_F(DomAgentTests, ChildNodeAppend) {
  RefPtr<Element> div = document_->createElement("DIV", ec_);
  body_->appendChild(div, ec_);

  dom_agent_->GetDocumentElement();
  dom_agent_->GetChildNodes(kCallId2, kHtmlElemId);
  dom_agent_->GetChildNodes(kCallId3, kBodyElemId);
  mock_delegate_->Reset();

  // There should be an event fired in case parent node is known to client,
  // Since children were already requested, event should have all the
  // new child data.
  OwnPtr<Value> v(DevToolsRpc::ParseMessage("[5,1,\"DIV\",\"\",[],0]"));
  mock_delegate_->ChildNodeInserted(kBodyElemId, 4, *v.get());
  mock_delegate_->Replay();

  RefPtr<Element> new_div = document_->createElement("DIV", ec_);
  body_->appendChild(new_div, ec_, false);
  mock_delegate_->Verify();
}

// Tests that "child node inserted" event is being fired.
TEST_F(DomAgentTests, ChildNodeInsert) {
  RefPtr<Element> div1 = document_->createElement("DIV", ec_);
  body_->appendChild(div1, ec_);
  RefPtr<Element> div2 = document_->createElement("DIV", ec_);
  body_->appendChild(div2, ec_);

  dom_agent_->GetDocumentElement();
  dom_agent_->GetChildNodes(kCallId2, kHtmlElemId);
  dom_agent_->GetChildNodes(kCallId3, kBodyElemId);
  mock_delegate_->Reset();

  // There should be an event fired in case parent node is known to client,
  // Since children were already requested, event should have all the
  // new child data.
  OwnPtr<Value> v(DevToolsRpc::ParseMessage("[6,1,\"DIV\",\"\",[],0]"));
  mock_delegate_->ChildNodeInserted(kBodyElemId, 4, *v.get());
  mock_delegate_->Replay();

  RefPtr<Element> new_div = document_->createElement("DIV", ec_);
  body_->insertBefore(new_div, div2.get(), ec_, false);
  mock_delegate_->Verify();
}

// Tests that "child node inserted" event is being fired.
TEST_F(DomAgentTests, ChildNodeRemovedUnknownParent) {
  RefPtr<Element> div = document_->createElement("DIV", ec_);
  body_->appendChild(div, ec_);

  dom_agent_->GetDocumentElement();
  mock_delegate_->Reset();

  // There should be no events fired until parent node is known to client.
  mock_delegate_->Replay();
  body_->removeChild(div.get(), ec_);
  mock_delegate_->Verify();
}

// Tests that "child node inserted" event is being fired.
TEST_F(DomAgentTests, ChildNodeRemovedKnownParent) {
  RefPtr<Element> div = document_->createElement("DIV", ec_);
  body_->appendChild(div, ec_);

  dom_agent_->GetDocumentElement();
  dom_agent_->GetChildNodes(kCallId2, kHtmlElemId);
  mock_delegate_->Reset();

  // There should be an event fired in case parent node is known to client,
  // but the event should not be specific.
  mock_delegate_->HasChildrenUpdated(kBodyElemId, false);
  mock_delegate_->Replay();

  body_->removeChild(div.get(), ec_);
  mock_delegate_->Verify();
}

// Tests that "child node inserted" event is being fired.
TEST_F(DomAgentTests, ChildNodeRemovedKnownChildren) {
  RefPtr<Element> div = document_->createElement("DIV", ec_);
  body_->appendChild(div, ec_);

  dom_agent_->GetDocumentElement();
  dom_agent_->GetChildNodes(kCallId2, kHtmlElemId);
  dom_agent_->GetChildNodes(kCallId3, kBodyElemId);
  mock_delegate_->Reset();

  // There should be an event fired in case parent node is known to client,
  // Since children were already requested, event should have removed child id.
  mock_delegate_->ChildNodeRemoved(kBodyElemId, 4);
  mock_delegate_->Replay();

  body_->removeChild(div.get(), ec_);
  mock_delegate_->Verify();
}

// Tests that "PushNodePathToClient" sends all missing events in path.
TEST_F(DomAgentTests, PushPathToKnownNode) {
  RefPtr<Element> div1 = document_->createElement("DIV", ec_);
  body_->appendChild(div1, ec_);

  dom_agent_->GetDocumentElement();
  dom_agent_->GetChildNodes(kCallId2, kHtmlElemId);
  dom_agent_->GetChildNodes(kCallId3, kBodyElemId);
  mock_delegate_->Reset();

  // We expect no messages - node is already known.
  mock_delegate_->Replay();

  int id = dom_agent_->PushNodePathToClient(div1.get());
  mock_delegate_->Verify();
  EXPECT_EQ(4, id);
}

// Tests that "PushNodePathToClient" sends all missing events in path.
TEST_F(DomAgentTests, PushPathToKnownParent) {
  RefPtr<Element> div1 = document_->createElement("DIV", ec_);
  body_->appendChild(div1, ec_);

  dom_agent_->GetDocumentElement();
  dom_agent_->GetChildNodes(kCallId2, kHtmlElemId);
  mock_delegate_->Reset();

  OwnPtr<Value> v1(DevToolsRpc::ParseMessage("[[4,1,\"DIV\",\"\",[],0]]"));
  mock_delegate_->SetChildNodes(kBodyElemId, *v1.get());
  mock_delegate_->Replay();

  int id = dom_agent_->PushNodePathToClient(div1.get());
  mock_delegate_->Verify();
  EXPECT_EQ(4, id);
}

// Tests that "PushNodePathToClient" sends all missing events in path.
TEST_F(DomAgentTests, PushPathToUnknownNode) {
  RefPtr<Element> div1 = document_->createElement("DIV", ec_);
  RefPtr<Element> div2 = document_->createElement("DIV", ec_);
  RefPtr<Element> div3 = document_->createElement("DIV", ec_);
  RefPtr<Element> div4 = document_->createElement("DIV", ec_);
  body_->appendChild(div1, ec_);
  div1->appendChild(div2, ec_);
  div2->appendChild(div3, ec_);
  div3->appendChild(div4, ec_);

  dom_agent_->GetDocumentElement();
  dom_agent_->GetChildNodes(kCallId2, kHtmlElemId);
  mock_delegate_->Reset();

  OwnPtr<Value> v1(DevToolsRpc::ParseMessage("[[4,1,\"DIV\",\"\",[],1]]"));
  OwnPtr<Value> v2(DevToolsRpc::ParseMessage("[[5,1,\"DIV\",\"\",[],1]]"));
  OwnPtr<Value> v3(DevToolsRpc::ParseMessage("[[6,1,\"DIV\",\"\",[],1]]"));
  OwnPtr<Value> v4(DevToolsRpc::ParseMessage("[[7,1,\"DIV\",\"\",[],0]]"));
  mock_delegate_->SetChildNodes(kBodyElemId, *v1.get());
  mock_delegate_->SetChildNodes(4, *v2.get());
  mock_delegate_->SetChildNodes(5, *v3.get());
  mock_delegate_->SetChildNodes(6, *v4.get());
  mock_delegate_->Replay();

  int id = dom_agent_->PushNodePathToClient(div4.get());
  mock_delegate_->Verify();
  EXPECT_EQ(7, id);
}

// Tests that "GetChildNodes" crosses frame owner boundaries.
TEST_F(DomAgentTests, GetChildNodesOfFrameOwner) {
  RefPtr<Element> iframe = document_->createElement("IFRAME", ec_);
  body_->appendChild(iframe, ec_);

  dom_agent_->GetDocumentElement();
  dom_agent_->GetChildNodes(kCallId2, kHtmlElemId);
  dom_agent_->GetChildNodes(kCallId3, kBodyElemId);
  mock_delegate_->Reset();

  // Expecting HTML child with two (head, body) children.
  OwnPtr<Value> v(DevToolsRpc::ParseMessage("[[5,1,\"HTML\",\"\",[],2]]"));
  mock_delegate_->SetChildNodes(4, *v.get());
  mock_delegate_->DidGetChildNodes(kCallId4);
  mock_delegate_->Replay();

  dom_agent_->GetChildNodes(kCallId4, 4);
  mock_delegate_->Verify();
}

// Tests that "PushNodePathToClient" crosses frame owner boundaries.
TEST_F(DomAgentTests, SendPathToNodeOverFrameOwner) {
  RefPtr<Element> iframe = document_->createElement("IFRAME", ec_);
  body_->appendChild(iframe, ec_);
  HTMLFrameOwnerElement* frame_owner =
      static_cast<HTMLFrameOwnerElement*>(iframe.get());
  Node* inner_body = frame_owner->contentDocument()->firstChild()->
      firstChild();

  dom_agent_->GetDocumentElement();
  dom_agent_->GetChildNodes(kCallId2, kHtmlElemId);
  mock_delegate_->Reset();

  OwnPtr<Value> v1(DevToolsRpc::ParseMessage("[[4,1,\"IFRAME\",\"\",[],1]]"));
  OwnPtr<Value> v2(DevToolsRpc::ParseMessage("[[5,1,\"HTML\",\"\",[],2]]"));
  OwnPtr<Value> v3(DevToolsRpc::ParseMessage(
      "[[6,1,\"HEAD\",\"\",[],0],[7,1,\"BODY\",\"\",[],0]]"));
  mock_delegate_->SetChildNodes(3, *v1.get());
  mock_delegate_->SetChildNodes(4, *v2.get());
  mock_delegate_->SetChildNodes(5, *v3.get());
  mock_delegate_->Replay();

  dom_agent_->PushNodePathToClient(inner_body);
  mock_delegate_->Verify();
}

// Tests that "child node inserted" event is being fired.
TEST_F(DomAgentTests, ChildNodeInsertUnderFrameOwner) {
  RefPtr<Element> iframe = document_->createElement("IFRAME", ec_);
  body_->appendChild(iframe, ec_);
  HTMLFrameOwnerElement* frame_owner =
      static_cast<HTMLFrameOwnerElement*>(iframe.get());
  Node* inner_body = frame_owner->contentDocument()->firstChild()->
      firstChild()->nextSibling();

  dom_agent_->GetDocumentElement();
  dom_agent_->GetChildNodes(kCallIdAny, kHtmlElemId);
  dom_agent_->GetChildNodes(kCallIdAny, kBodyElemId);
  dom_agent_->GetChildNodes(kCallIdAny, 4);  // IFrame children
  dom_agent_->GetChildNodes(kCallIdAny, 5);  // IFrame html's children
  dom_agent_->GetChildNodes(kCallIdAny, 7);  // IFrame body's children
  mock_delegate_->Reset();

  // There should be an event fired in case parent node is known to client,
  // Since children were already requested, event should have all the
  // new child data.
  OwnPtr<Value> v(DevToolsRpc::ParseMessage("[8,1,\"DIV\",\"\",[],0]"));
  mock_delegate_->ChildNodeInserted(7, 0, *v.get());
  mock_delegate_->Replay();

  RefPtr<Element> new_div = document_->createElement("DIV", ec_);
  inner_body->appendChild(new_div.get(), ec_, false);
  mock_delegate_->Verify();
}

}  // namespace
