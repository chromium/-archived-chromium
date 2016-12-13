// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DOM_OPERATIONS_PRIVATE_H_
#define WEBKIT_GLUE_DOM_OPERATIONS_PRIVATE_H_

namespace WebCore {
class AtomicString;
class Document;
class Element;
class HTMLLinkElement;
class HTMLMetaElement;
class HTMLOptionElement;
class Node;
class QualifiedName;
class String;
}

class WebFrameImpl;

namespace webkit_glue {

// If node is an HTML node with a tag name of name it is casted and returned.
// If node is not an HTML node or the tag name is not name NULL is returned.
WebCore::HTMLLinkElement* CastToHTMLLinkElement(WebCore::Node* node);
WebCore::HTMLMetaElement* CastToHTMLMetaElement(WebCore::Node* node);
WebCore::HTMLOptionElement* CastToHTMLOptionElement(WebCore::Node* node);

// If element is HTML:IFrame or HTML:Frame, then return the WebFrameImpl
// object corresponding to the content frame, otherwise return NULL.
// The parameter is_frame_element indicates whether the input element
// is frame/iframe element or not.
WebFrameImpl* GetWebFrameImplFromElement(WebCore::Element* element,
                                         bool* is_frame_element);

// If element is img, script or input type=image, then return its link refer
// to the "src" attribute. If element is link, then return its link refer to
// the "href" attribute. If element is body, table, tr, td, then return its
// link refer to the "background" attribute. If element is blockquote, q, del,
// ins, then return its link refer to the "cite" attribute. Otherwise return
// NULL.
const WebCore::AtomicString* GetSubResourceLinkFromElement(
    const WebCore::Element* element);

// For img, script, iframe, frame element, when attribute name is src,
// for link, a, area element, when attribute name is href,
// for form element, when attribute name is action,
// for input, type=image, when attribute name is src,
// for body, table, tr, td, when attribute name is background,
// for blockquote, q, del, ins, when attribute name is cite,
// we can consider the attribute value has legal link.
bool ElementHasLegalLinkAttribute(const WebCore::Element* element,
                                  const WebCore::QualifiedName& attr_name);

// Get pointer of WebFrameImpl from webview according to specific URL.
WebFrameImpl* GetWebFrameImplFromWebViewForSpecificURL(WebView* view,
                                                       const GURL& page_url);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_DOM_OPERATIONS_PRIVATE_H_
