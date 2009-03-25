// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_DOM_AGENT_H_
#define WEBKIT_GLUE_DEVTOOLS_DOM_AGENT_H_

#include "webkit/glue/devtools/devtools_rpc.h"

// DomAgent is a utility object that covers DOM-related functionality of the
// WebDevToolsAgent. It is capable of sending DOM tree to the client as well
// as providing DOM notifications for the nodes known to the client.
// DomAgent's environment is represented with the DomAgentDelegate interface.
#define DOM_AGENT_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3) \
  /* Requests that the document root element is sent to the delegate. */ \
  METHOD0(GetDocumentElement) \
  \
  /* Requests that the element's children are sent to the client. */ \
  METHOD2(GetChildNodes, int /* call_id */, int /* id */) \
  \
  /* Sets attribute value in the element with given id. */ \
  METHOD3(SetAttribute, int /* id */, String /* name */, String /* value */) \
  \
  /* Removes attribute from the element with given id. */ \
  METHOD2(RemoveAttribute, int /* id */, String /* name */) \
  \
  /* Sets text node value in the node with given id. */ \
  METHOD2(SetTextNodeValue, int /* id */, String /* text */) \
  \
  /* Perform search. */ \
  METHOD2(PerformSearch, int /* call_id */, String /* query */) \
  \
  /* Tells dom agent that the client has lost all of the dom-related
   information and is no longer interested in the notifications related to the
   nodes issued earlier. */ \
  METHOD0(DiscardBindings)

DEFINE_RPC_CLASS(DomAgent, DOM_AGENT_STRUCT)

#define DOM_AGENT_DELEGATE_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3) \
  /* Response to GetChildNodes. */ \
  METHOD1(DidGetChildNodes, int /* call_id */) \
  \
  /* Perform search. */ \
  METHOD2(DidPerformSearch, int /* call_id */, Value /* results */) \
  \
  /* Notifies the delegate that element's attributes are updated. */ \
  METHOD2(AttributesUpdated, int /* id */, Value /* attributes */) \
  \
  /* Sends document element to the delegate. */ \
  METHOD1(SetDocumentElement, Value /* root */) \
  \
  /* Notifies the delegate that element's child nodes have been updated. */ \
  METHOD2(SetChildNodes, int /* parent_id */, Value /* nodes */) \
  \
  /* Notifies the delegate that element's 'has children' state has been
     updated */ \
  METHOD2(HasChildrenUpdated, int /* id */, bool /* new_value */) \
  \
  /* Notifies the delegate that child node has been inserted. */ \
  METHOD3(ChildNodeInserted, int /* parent_id */ , int /* prev_id */, \
      Value /* node */) \
  \
  /* Notifies the delegate that child node has been deleted. */ \
  METHOD2(ChildNodeRemoved, int /* parent_id */, int /* id */)

DEFINE_RPC_CLASS(DomAgentDelegate, DOM_AGENT_DELEGATE_STRUCT)

#endif  // WEBKIT_GLUE_DEVTOOLS_DOM_AGENT_H_
