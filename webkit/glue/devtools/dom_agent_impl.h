// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_DOM_AGENT_IMPL_H_
#define WEBKIT_GLUE_DEVTOOLS_DOM_AGENT_IMPL_H_

#include "config.h"

#include <wtf/ListHashSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

#include "webkit/glue/devtools/dom_agent.h"

namespace WebCore {
class Document;
class Element;
class Event;
class Node;
}

class ListValue;
class Value;

// DomAgent implementation.
class DomAgentImpl : public DomAgent {
 public:
  explicit DomAgentImpl(DomAgentDelegate* delegate);
  virtual ~DomAgentImpl();

  // DomAgent implementation.
  void GetDocumentElement();
  void GetChildNodes(int element_id);
  void SetAttribute(
      int element_id,
      const WebCore::String& name,
      const WebCore::String& value);
  void RemoveAttribute(int element_id, const WebCore::String& name);
  void SetTextNodeValue(int element_id, const WebCore::String& value);
  void DiscardBindings();

  // Initializes dom agent with the given document.
  void SetDocument(WebCore::Document* document);

  // Returns node for given id according to the present binding.
  WebCore::Node* GetNodeForId(int id);

  // Returns id for given node according to the present binding.
  int GetIdForNode(WebCore::Node* node);

  // Sends path to a given node to the client. Returns node's id according to
  // the resulting binding.
  int GetPathToNode(WebCore::Node* node);

 private:
  // Convenience EventListner wrapper for cleaner Ref management.
  class EventListenerWrapper : public WebCore::EventListener {
   public:
    static PassRefPtr<EventListenerWrapper> Create(
        DomAgentImpl* dom_agent_impl);
    virtual ~EventListenerWrapper() {}
    virtual void handleEvent(WebCore::Event* event, bool isWindowEvent);
   private:
    explicit EventListenerWrapper(DomAgentImpl* dom_agent_impl);
    DomAgentImpl* dom_agent_impl_;
    DISALLOW_COPY_AND_ASSIGN(EventListenerWrapper);
  };

  void StartListening(WebCore::Document* document);

  void StopListening(WebCore::Document* document);

  // EventListener implementation
  friend class EventListenerWrapper;
  virtual void handleEvent(WebCore::Event* event, bool isWindowEvent);

  // Binds given node and returns its generated id.
  int Bind(WebCore::Node* node);

  // Releases Node to int binding.
  void Unbind(WebCore::Node* node);

  // Serializes given node into the list value.
  ListValue* BuildValueForNode(
      WebCore::Node* node,
      int depth);

  // Serializes given element's attributes into the list value.
  ListValue* BuildValueForElementAttributes(WebCore::Element* elemen);

  // Serializes given elements's children into the list value.
  ListValue* BuildValueForElementChildren(
      WebCore::Element* element,
      int depth);

  // We represent embedded doms as a part of the same hierarchy. Hence we
  // treat children of frame owners differently. Following two methods
  // encapsulate frame owner specifics.
  WebCore::Node* InnerFirstChild(WebCore::Node* node);
  int InnerChildNodeCount(WebCore::Node* node);
  WebCore::Element* InnerParentElement(WebCore::Node* node);

  DomAgentDelegate* delegate_;
  HashMap<WebCore::Node*, int> node_to_id_;
  HashMap<int, WebCore::Node*> id_to_node_;
  HashSet<int> children_requested_;
  int last_node_id_;
  ListHashSet<RefPtr<WebCore::Document> > documents_;
  RefPtr<WebCore::EventListener> event_listener_;
  bool document_element_requested_;

  DISALLOW_COPY_AND_ASSIGN(DomAgentImpl);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DOM_AGENT_IMPL_H_
