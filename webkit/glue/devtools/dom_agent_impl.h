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
class NameNodeMap;
class Node;
}

class DictionaryValue;
class ListValue;
class Value;

// DomAgent implementation.
class DomAgentImpl : public DomAgent {
 public:
  explicit DomAgentImpl(DomAgentDelegate* delegate);
  virtual ~DomAgentImpl();

  // DomAgent implementation.
  void GetDocumentElement();
  void GetChildNodes(int call_id, int element_id);
  void SetAttribute(
      int call_id,
      int element_id,
      const WebCore::String& name,
      const WebCore::String& value);
  void RemoveAttribute(
      int call_id,
      int element_id,
      const WebCore::String& name);
  void SetTextNodeValue(
      int call_id,
      int element_id,
      const WebCore::String& value);
  void PerformSearch(int call_id, const String& query);
  void DiscardBindings();

  // Initializes dom agent with the given document.
  void SetDocument(WebCore::Document* document);

  // Returns node for given id according to the present binding.
  WebCore::Node* GetNodeForId(int id);

  // Returns id for given node according to the present binding.
  int GetIdForNode(WebCore::Node* node);

  // Sends path to a given node to the client. Returns node's id according to
  // the resulting binding. Only sends nodes that are missing on the client.
  int PushNodePathToClient(WebCore::Node* node);

 private:
  static const char kExactTagNames[];
  static const char kPartialTagNames[];
  static const char kStartOfTagNames[];
  static const char kPartialTagNamesAndAttributeValues[];
  static const char kPartialAttributeValues[];
  static const char kPlainText[];

// Convenience EventListner wrapper for cleaner Ref management.
  class EventListenerWrapper : public WebCore::EventListener {
   public:
    static PassRefPtr<EventListenerWrapper> Create(
        DomAgentImpl* dom_agent_impl);
    virtual ~EventListenerWrapper() {}
    virtual void handleEvent(WebCore::Event* event, bool is_window_event);
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

  // Pushes document element to the client.
  void PushDocumentElementToClient();

  // Pushes child nodes to the client.
  void PushChildNodesToClient(int element_id);

  // Serializes given node into the list value.
  ListValue* BuildValueForNode(
      WebCore::Node* node,
      int depth);

  // Serializes given element's attributes into the list value.
  static ListValue* BuildValueForElementAttributes(WebCore::Element* elemen);

  // Serializes given elements's children into the list value.
  ListValue* BuildValueForElementChildren(
      WebCore::Element* element,
      int depth);

  // Serializes CSSStyleDeclaration into a list of properties
  // where aeach property represented as an array as an
  // [name, important, implicit, shorthand, value]
  static ListValue* BuildValueForStyle(
      const WebCore::CSSStyleDeclaration& style);

  // We represent embedded doms as a part of the same hierarchy. Hence we
  // treat children of frame owners differently. We also skip whitespace
  // text nodes conditionally. Following four methods encapsulate these
  // specifics.
  WebCore::Node* InnerFirstChild(WebCore::Node* node);
  WebCore::Node* InnerNextSibling(WebCore::Node* node);
  WebCore::Node* InnerPreviousSibling(WebCore::Node* node);
  int InnerChildNodeCount(WebCore::Node* node);
  WebCore::Element* InnerParentElement(WebCore::Node* node);
  bool IsWhitespace(WebCore::Node* node);

  WebCore::Document* GetMainFrameDocument();

  DomAgentDelegate* delegate_;
  HashMap<WebCore::Node*, int> node_to_id_;
  HashMap<int, WebCore::Node*> id_to_node_;
  HashSet<int> children_requested_;
  int last_node_id_;
  ListHashSet<RefPtr<WebCore::Document> > documents_;
  RefPtr<WebCore::EventListener> event_listener_;
  // Captures pending document element request's call id.
  // Defaults to 0 meaning no pending request.
  bool document_element_requested_;

  DISALLOW_COPY_AND_ASSIGN(DomAgentImpl);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DOM_AGENT_IMPL_H_
