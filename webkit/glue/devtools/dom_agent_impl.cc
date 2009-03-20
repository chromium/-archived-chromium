// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "AtomicString.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "HTMLFrameOwnerElement.h"
#include "markup.h"
#include "MutationEvent.h"
#include "Node.h"
#include "PlatformString.h"
#include "Text.h"
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>
#undef LOG

#include "base/json_writer.h"
#include "base/values.h"
#include "webkit/glue/devtools/dom_agent_impl.h"
#include "webkit/glue/glue_util.h"

using namespace WebCore;

// static
PassRefPtr<DomAgentImpl::EventListenerWrapper>
    DomAgentImpl::EventListenerWrapper::Create(
        DomAgentImpl* dom_agent_impl) {
  return adoptRef(new EventListenerWrapper(dom_agent_impl));
}

DomAgentImpl::EventListenerWrapper::EventListenerWrapper(
    DomAgentImpl* dom_agent_impl) : dom_agent_impl_(dom_agent_impl) {
}

void DomAgentImpl::EventListenerWrapper::handleEvent(
    Event* event,
    bool isWindowEvent) {
  dom_agent_impl_->handleEvent(event, isWindowEvent);
}

DomAgentImpl::DomAgentImpl(DomAgentDelegate* delegate)
    : delegate_(delegate),
      last_node_id_(1),
      document_element_call_id_(0) {
  event_listener_ = EventListenerWrapper::Create(this);
}

DomAgentImpl::~DomAgentImpl() {
  SetDocument(NULL);
}

void DomAgentImpl::SetDocument(Document* doc) {
  DiscardBindings();

  ListHashSet<RefPtr<Document> > copy = documents_;
  for (ListHashSet<RefPtr<Document> >::iterator it = copy.begin();
       it != copy.end(); ++it) {
    StopListening((*it).get());
  }
  ASSERT(documents_.size() == 0);

  if (doc) {
    StartListening(doc);
    if (document_element_call_id_) {
      GetDocumentElement(document_element_call_id_);
      document_element_call_id_ = 0;
    }
  }
}

void DomAgentImpl::StartListening(Document* doc) {
  if (documents_.contains(doc))
    return;
  doc->addEventListener(eventNames().DOMContentLoadedEvent, event_listener_,
      false);
  doc->addEventListener(eventNames().DOMNodeInsertedEvent, event_listener_,
      false);
  doc->addEventListener(eventNames().DOMNodeRemovedEvent, event_listener_,
      false);
  doc->addEventListener(eventNames().DOMNodeRemovedFromDocumentEvent,
      event_listener_, true);
  doc->addEventListener(eventNames().DOMAttrModifiedEvent, event_listener_,
      false);
  documents_.add(doc);
}

void DomAgentImpl::StopListening(Document* doc) {
  doc->removeEventListener(eventNames().DOMContentLoadedEvent,
      event_listener_.get(), false);
  doc->removeEventListener(eventNames().DOMNodeInsertedEvent,
      event_listener_.get(), false);
  doc->removeEventListener(eventNames().DOMNodeRemovedEvent,
      event_listener_.get(), false);
  doc->removeEventListener(eventNames().DOMNodeRemovedFromDocumentEvent,
      event_listener_.get(), true);
  doc->removeEventListener(eventNames().DOMAttrModifiedEvent,
      event_listener_.get(), false);
  documents_.remove(doc);
}

int DomAgentImpl::Bind(Node* node) {
  HashMap<Node*, int>::iterator it = node_to_id_.find(node);
  if (it != node_to_id_.end())
    return it->second;
  int id = last_node_id_++;
  node_to_id_.set(node, id);
  id_to_node_.set(id, node);
  return id;
}

void DomAgentImpl::Unbind(Node* node) {
  if (node->isFrameOwnerElement()) {
    const HTMLFrameOwnerElement* frame_owner =
        static_cast<const HTMLFrameOwnerElement*>(node);
    StopListening(frame_owner->contentDocument());
  }

  HashMap<Node*, int>::iterator it = node_to_id_.find(node);
  if (it != node_to_id_.end()) {
    id_to_node_.remove(id_to_node_.find(it->second));
    children_requested_.remove(children_requested_.find(it->second));
    node_to_id_.remove(it);
  }
}

void DomAgentImpl::DiscardBindings() {
  node_to_id_.clear();
  id_to_node_.clear();
  children_requested_.clear();
}

Node* DomAgentImpl::GetNodeForId(int id) {
  HashMap<int, Node*>::iterator it = id_to_node_.find(id);
  if (it != id_to_node_.end()) {
    return it->second;
  }
  return NULL;
}

int DomAgentImpl::GetIdForNode(Node* node) {
  if (node == NULL) {
    return 0;
  }
  HashMap<Node*, int>::iterator it = node_to_id_.find(node);
  if (it != node_to_id_.end()) {
    return it->second;
  }
  return 0;
}

void DomAgentImpl::handleEvent(Event* event, bool isWindowEvent) {
  AtomicString type = event->type();
  Node* node = event->target()->toNode();

  // Remove mapping entry if necessary.
  if (type == eventNames().DOMNodeRemovedFromDocumentEvent) {
    Unbind(node);
    return;
  }

  if (type == eventNames().DOMAttrModifiedEvent) {
    int id = GetIdForNode(node);
    if (!id) {
      // Node is not mapped yet -> ignore the event.
      return;
    }
    Element* element = static_cast<Element*>(node);
    OwnPtr<Value> attributesValue(BuildValueForElementAttributes(element));
    delegate_->AttributesUpdated(id, *attributesValue.get());
  } else if (type == eventNames().DOMNodeInsertedEvent) {
    Node* parent = static_cast<MutationEvent*>(event)->relatedNode();
    int parent_id = GetIdForNode(parent);
    if (!parent_id) {
      // Parent is not mapped yet -> ignore the event.
      return;
    }
    if (!children_requested_.contains(parent_id)) {
      // No children are mapped yet -> only notify on changes of hasChildren.
      delegate_->HasChildrenUpdated(parent_id, true);
    } else {
      // Children have been requested -> return value of a new child.
      int prev_id = GetIdForNode(node->previousSibling());
      OwnPtr<Value> value(BuildValueForNode(node, 0));
      delegate_->ChildNodeInserted(parent_id, prev_id, *value.get());
    }
  } else if (type == eventNames().DOMNodeRemovedEvent) {
    Node* parent = static_cast<MutationEvent*>(event)->relatedNode();
    int parent_id = GetIdForNode(parent);
    if (!parent_id) {
      // Parent is not mapped yet -> ignore the event.
      return;
    }
    if (!children_requested_.contains(parent_id)) {
      // No children are mapped yet -> only notify on changes of hasChildren.
      if (parent->childNodeCount() == 1)
        delegate_->HasChildrenUpdated(parent_id, false);
    } else {
      int id = GetIdForNode(node);
      delegate_->ChildNodeRemoved(parent_id, id);
    }
  } else if (type == eventNames().DOMContentLoadedEvent) {
    //TODO(pfeldman): handle content load event.
  }
}

void DomAgentImpl::GetDocumentElement(int call_id) {
  if (documents_.size() > 0) {
    OwnPtr<Value> value(
        BuildValueForNode((*documents_.begin())->documentElement(), 0));
    std::string json;
    ToJson(value.get(), &json);
    delegate_->GetDocumentElementResult(call_id, json);
  } else {
    document_element_call_id_ = call_id;
  }
}

void DomAgentImpl::GetChildNodes(int call_id, int element_id) {
  Node* node = GetNodeForId(element_id);
  if (!node || (node->nodeType() != Node::ELEMENT_NODE))
    return;

  Element* element = static_cast<Element*>(node);
  OwnPtr<Value> children(BuildValueForElementChildren(element, 1));
  children_requested_.add(element_id);
  std::string json;
  ToJson(children.get(), &json);
  delegate_->GetChildNodesResult(call_id, json);
}

int DomAgentImpl::GetPathToNode(Node* node_to_select) {
  ASSERT(node_to_select);  // Invalid input

  // Return id in case the node is known.
  int result = GetIdForNode(node_to_select);
  if (result)
    return result;

  Element* element = InnerParentElement(node_to_select);
  ASSERT(element);  // Node is detached or is a document itself

  Vector<Element*> path;
  while (element && !GetIdForNode(element)) {
    path.append(element);
    element = InnerParentElement(element);
  }
  // element is known to the client
  ASSERT(element);
  path.append(element);

  for (int i = path.size() - 1; i >= 0; --i) {
    element = path.at(i);
    OwnPtr<Value> children(BuildValueForElementChildren(element, 1));
    delegate_->ChildNodesUpdated(GetIdForNode(element), *children.get());
  }
  return GetIdForNode(node_to_select);
}

void DomAgentImpl::SetAttribute(
    int element_id,
    const String& name,
    const String& value) {
  Node* node = GetNodeForId(element_id);
  if (node && (node->nodeType() == Node::ELEMENT_NODE)) {
    Element* element = static_cast<Element*>(node);
        ExceptionCode ec = 0;
    element->setAttribute(name, value, ec);
  }
}

void DomAgentImpl::RemoveAttribute(int element_id, const String& name) {
  Node* node = GetNodeForId(element_id);
  if (node && (node->nodeType() == Node::ELEMENT_NODE)) {
    Element* element = static_cast<Element*>(node);
    ExceptionCode ec = 0;
    element->removeAttribute(name, ec);
  }
}

void DomAgentImpl::SetTextNodeValue(int element_id, const String& value) {
  Node* node = GetNodeForId(element_id);
  if (node && (node->nodeType() == Node::TEXT_NODE)) {
    Text* text_node = static_cast<Text*>(node);
    ExceptionCode ec = 0;
    // TODO(pfeldman): Add error handling
    text_node->replaceWholeText(value, ec);
  }
}

ListValue* DomAgentImpl::BuildValueForNode(Node* node, int depth) {
  OwnPtr<ListValue> value(new ListValue());
  int id = Bind(node);
  String nodeName;
  String nodeValue;

  switch (node->nodeType()) {
    case Node::TEXT_NODE:
    case Node::COMMENT_NODE:
      nodeValue = node->nodeValue();
      break;
    case Node::ATTRIBUTE_NODE:
    case Node::DOCUMENT_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
      break;
    case Node::ELEMENT_NODE:
    default: {
      nodeName = node->nodeName();
      break;
    }
  }

  value->Append(Value::CreateIntegerValue(id));
  value->Append(Value::CreateIntegerValue(node->nodeType()));
  value->Append(Value::CreateStringValue(
      webkit_glue::StringToStdWString(nodeName)));
  value->Append(Value::CreateStringValue(
      webkit_glue::StringToStdWString(nodeValue)));

  if (node->nodeType() == Node::ELEMENT_NODE) {
    Element* element = static_cast<Element*>(node);
    value->Append(BuildValueForElementAttributes(element));
    int nodeCount = InnerChildNodeCount(element);
    value->Append(Value::CreateIntegerValue(nodeCount));
    OwnPtr<ListValue> children(BuildValueForElementChildren(element, depth));
    if (children->GetSize() > 0) {
      value->Append(children.release());
    }
  }
  return value.release();
}

ListValue* DomAgentImpl::BuildValueForElementAttributes(Element* element) {
  OwnPtr<ListValue> attributesValue(new ListValue());
  // Go through all attributes and serialize them.
  const NamedAttrMap *attrMap = element->attributes(true);
  if (!attrMap) {
    return attributesValue.release();
  }
  unsigned numAttrs = attrMap->length();
  for (unsigned i = 0; i < numAttrs; i++) {
    // Add attribute pair
    const Attribute *attribute = attrMap->attributeItem(i);
    OwnPtr<Value> name(Value::CreateStringValue(
        webkit_glue::StringToStdWString(attribute->name().toString())));
    OwnPtr<Value> value(Value::CreateStringValue(
        webkit_glue::StringToStdWString(attribute->value())));
    attributesValue->Append(name.release());
    attributesValue->Append(value.release());
  }
  return attributesValue.release();
}

ListValue* DomAgentImpl::BuildValueForElementChildren(
    Element* element,
    int depth) {
  OwnPtr<ListValue> children(new ListValue());
  if (depth == 0) {
    // Special case the_only text child.
    if (element->childNodeCount() == 1) {
      Node *child = element->firstChild();
      if (child->nodeType() == Node::TEXT_NODE) {
        children->Append(BuildValueForNode(child, 0));
      }
    }
    return children.release();
  } else if (depth > 0) {
    depth--;
  }

  for (Node *child = InnerFirstChild(element); child != NULL;
       child = child->nextSibling()) {
    children->Append(BuildValueForNode(child, depth));
  }
  return children.release();
}

Node* DomAgentImpl::InnerFirstChild(Node* node) {
  if (node->isFrameOwnerElement()) {
    HTMLFrameOwnerElement* frame_owner =
        static_cast<HTMLFrameOwnerElement*>(node);
    Document* doc = frame_owner->contentDocument();
    StartListening(doc);
    return doc->firstChild();
  } else {
    return node->firstChild();
  }
}

int DomAgentImpl::InnerChildNodeCount(Node* node) {
  if (node->isFrameOwnerElement()) {
    return 1;
  } else {
    return node->childNodeCount();
  }
}

Element* DomAgentImpl::InnerParentElement(Node* node) {
  Element* element = node->parentElement();
  if (!element) {
    return node->ownerDocument()->ownerElement();
  }
  return element;
}

void DomAgentImpl::ToJson(const Value* value, std::string* json) {
  JSONWriter::Write(value, false, json);
}
