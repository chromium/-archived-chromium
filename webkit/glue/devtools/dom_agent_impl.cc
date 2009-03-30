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
#include "NodeList.h"
#include "PlatformString.h"
#include "Text.h"
#include "XPathResult.h"
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#undef LOG

#include "base/string_util.h"
#include "base/values.h"
#include "webkit/glue/devtools/dom_agent_impl.h"
#include "webkit/glue/glue_util.h"

using namespace WebCore;

const char DomAgentImpl::kExactTagNames[] = "//*[name() == '%s')]";
const char DomAgentImpl::kPartialTagNames[] = "//*[contains(name(), '%s')]";
const char DomAgentImpl::kStartOfTagNames[] = "//*[starts-with(name(), '%s')]";
const char DomAgentImpl::kPartialTagNamesAndAttributeValues[] =
    "//*[contains(name(), '%s') or contains(@*, '%s')]";
const char DomAgentImpl::kPartialAttributeValues[] = "//*[contains(@*, '%s')]";
const char DomAgentImpl::kPlainText[] =
    "//text()[contains(., '%s')] | //comment()[contains(., '%s')]";

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
      document_element_requested_(false) {
  event_listener_ = EventListenerWrapper::Create(this);
}

DomAgentImpl::~DomAgentImpl() {
  SetDocument(NULL);
}

void DomAgentImpl::SetDocument(Document* doc) {
  if (documents_.size() && doc == documents_.begin()->get()) {
    return;
  }

  ListHashSet<RefPtr<Document> > copy = documents_;
  for (ListHashSet<RefPtr<Document> >::iterator it = copy.begin();
       it != copy.end(); ++it) {
    StopListening((*it).get());
  }
  ASSERT(documents_.size() == 0);

  if (doc) {
    StartListening(doc);
    if (document_element_requested_) {
      GetDocumentElement();
      document_element_requested_ = false;
    }
  } else {
    DiscardBindings();
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

void DomAgentImpl::PushDocumentElementToClient() {
  Element* doc_elem = (*documents_.begin())->documentElement();
  if (!node_to_id_.contains(doc_elem)) {
    OwnPtr<Value> value(BuildValueForNode(doc_elem, 0));
    delegate_->SetDocumentElement(*value.get());
  }
}

void DomAgentImpl::PushChildNodesToClient(int element_id) {
  Node* node = GetNodeForId(element_id);
  if (!node || (node->nodeType() != Node::ELEMENT_NODE))
    return;
  if (children_requested_.contains(element_id))
    return;

  Element* element = static_cast<Element*>(node);
  OwnPtr<Value> children(BuildValueForElementChildren(element, 1));
  children_requested_.add(element_id);
  delegate_->SetChildNodes(element_id, *children.get());
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

void DomAgentImpl::GetDocumentElement() {
  if (documents_.size() > 0) {
    PushDocumentElementToClient();
  } else {
    document_element_requested_ = true;
  }
}

void DomAgentImpl::GetChildNodes(int call_id, int element_id) {
  PushChildNodesToClient(element_id);
  delegate_->DidGetChildNodes(call_id);
}

int DomAgentImpl::PushNodePathToClient(Node* node_to_select) {
  ASSERT(node_to_select);  // Invalid input

  // Return id in case the node is known.
  int result = GetIdForNode(node_to_select);
  if (result)
    return result;

  Element* element = InnerParentElement(node_to_select);
  ASSERT(element);  // Node is detached or is a document itself

  // If we are sending information to the client that is currently being
  // created. Send root node first.
  PushDocumentElementToClient();

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
    delegate_->SetChildNodes(GetIdForNode(element), *children.get());
  }
  return GetIdForNode(node_to_select);
}

void DomAgentImpl::SetAttribute(
    int call_id,
    int element_id,
    const String& name,
    const String& value) {
  Node* node = GetNodeForId(element_id);
  if (node && (node->nodeType() == Node::ELEMENT_NODE)) {
    Element* element = static_cast<Element*>(node);
    ExceptionCode ec = 0;
    element->setAttribute(name, value, ec);
    delegate_->DidApplyDomChange(call_id, ec == 0);
  } else {
    delegate_->DidApplyDomChange(call_id, false);
  }
}

void DomAgentImpl::RemoveAttribute(
    int call_id,
    int element_id,
    const String& name) {
  Node* node = GetNodeForId(element_id);
  if (node && (node->nodeType() == Node::ELEMENT_NODE)) {
    Element* element = static_cast<Element*>(node);
    ExceptionCode ec = 0;
    element->removeAttribute(name, ec);
    delegate_->DidApplyDomChange(call_id, ec == 0);
  } else {
    delegate_->DidApplyDomChange(call_id, false);
  }
}

void DomAgentImpl::SetTextNodeValue(
    int call_id,
    int element_id,
    const String& value) {
  Node* node = GetNodeForId(element_id);
  if (node && (node->nodeType() == Node::TEXT_NODE)) {
    Text* text_node = static_cast<Text*>(node);
    ExceptionCode ec = 0;
    // TODO(pfeldman): Add error handling
    text_node->replaceWholeText(value, ec);
    delegate_->DidApplyDomChange(call_id, ec == 0);
  } else {
    delegate_->DidApplyDomChange(call_id, false);
  }
}

void DomAgentImpl::PerformSearch(int call_id, const String& query) {
  String tag_name_query = query;
  String attribute_name_query = query;

  bool start_tag_found = tag_name_query.startsWith("<", true);
  bool end_tag_found = tag_name_query.endsWith(">", true);

  if (start_tag_found || end_tag_found) {
    int tag_name_query_length = tag_name_query.length();
    int start = start_tag_found ? 1 : 0;
    int end = end_tag_found ? tag_name_query_length - 1 : tag_name_query_length;
    tag_name_query = tag_name_query.substring(start, end - start);
  }

  Vector<String> xpath_queries;
  xpath_queries.append(String::format(kPlainText, query.utf8().data(),
      query.utf8().data()));
  if (tag_name_query.length() && start_tag_found && end_tag_found) {
    xpath_queries.append(String::format(kExactTagNames,
                             tag_name_query.utf8().data()));
  } else if (tag_name_query.length() && start_tag_found) {
    xpath_queries.append(String::format(kStartOfTagNames,
                             tag_name_query.utf8().data()));
  } else if (tag_name_query.length() && end_tag_found) {
    // FIXME(pfeldman): we should have a matchEndOfTagNames search function if
    // endTagFound is true but not startTagFound.
    // This requires ends-with() support in XPath, WebKit only supports
    // starts-with() and contains().
    xpath_queries.append(String::format(kPartialTagNames,
                             tag_name_query.utf8().data()));
  } else if (query == "//*" || query == "*") {
    // These queries will match every node. Matching everything isn't useful
    // and can be slow for large pages, so limit the search functions list to
    // plain text and attribute matching.
    xpath_queries.append(String::format(kPartialAttributeValues,
                             query.utf8().data()));
  } else {
    // TODO(pfeldman): Add more patterns.
    xpath_queries.append(String::format(kPartialTagNamesAndAttributeValues,
                             tag_name_query.utf8().data(),
                             query.utf8().data()));
  }

  ExceptionCode ec = 0;
  Vector<Document*> search_documents;
  Document* main_document = (*documents_.begin()).get();
  search_documents.append(main_document);

  // Find all frames, iframes and object elements to search their documents.
  RefPtr<NodeList> node_list = main_document->querySelectorAll(
      "iframe, frame, object", ec);
  if (ec) {
    ListValue list;
    delegate_->DidPerformSearch(call_id, list);
    return;
  }
  for (unsigned int i = 0; i < node_list->length(); ++i) {
    Node* node = node_list->item(i);
    if (node->isFrameOwnerElement()) {
      const HTMLFrameOwnerElement* frame_owner =
          static_cast<const HTMLFrameOwnerElement*>(node);
      if (frame_owner->contentDocument()) {
        search_documents.append(search_documents);
      }
    }
  }

  HashSet<int> node_ids;
  for (Vector<Document*>::iterator it = search_documents.begin();
       it != search_documents.end(); ++it) {
    for (Vector<String>::iterator qit = xpath_queries.begin();
         qit != xpath_queries.end(); ++qit) {
      String query = *qit;
      RefPtr<XPathResult> result = (*it)->evaluate(query, *it, NULL,
          XPathResult::UNORDERED_NODE_ITERATOR_TYPE, 0, ec);
      if (ec) {
        ListValue list;
        delegate_->DidPerformSearch(call_id, list);
        return;
      }
      Node* node = result->iterateNext(ec);
      while (node && !ec) {
        node_ids.add(PushNodePathToClient(node));
        node = result->iterateNext(ec);
      }
    }
  }

  ListValue list;
  for (HashSet<int>::iterator it = node_ids.begin();
       it != node_ids.end(); ++it) {
    list.Append(Value::CreateIntegerValue(*it));
  }
  delegate_->DidPerformSearch(call_id, list);
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
