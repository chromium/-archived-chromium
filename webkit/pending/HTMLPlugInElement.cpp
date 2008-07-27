/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "config.h"
#include "HTMLPlugInElement.h"

#include "CSSPropertyNames.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "HTMLNames.h"
#include "Page.h"
#include "RenderWidget.h"
#include "Settings.h"
#include "Widget.h"

namespace WebCore {

using namespace HTMLNames;

HTMLPlugInElement::HTMLPlugInElement(const QualifiedName& tagName, Document* doc)
    : HTMLFrameOwnerElement(tagName, doc)
#if USE(NPOBJECT)
    , m_NPObject(0)
#endif
{
}

HTMLPlugInElement::~HTMLPlugInElement()
{
#if USE(JAVASCRIPTCORE_BINDINGS) | USE(V8_BINDING)
    // Should be cleaned up in detach.
    ASSERT(m_instance.IsEmpty());
#endif

#if USE(NPOBJECT)
    if (m_NPObject) {
        // NOTE: mbelshe - can the frame be inaccessible here?  If so,
        // do we leak objects?
        if (document() && document()->frame()) 
            document()->frame()->scriptBridge()->functions()->releaseObject(m_NPObject);
        m_NPObject = 0;
    }
#endif
}

String HTMLPlugInElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLPlugInElement::setAlign(const String& value)
{
    setAttribute(alignAttr, value);
}

String HTMLPlugInElement::height() const
{
    return getAttribute(heightAttr);
}

void HTMLPlugInElement::setHeight(const String& value)
{
    setAttribute(heightAttr, value);
}

String HTMLPlugInElement::name() const
{
    return getAttribute(nameAttr);
}

void HTMLPlugInElement::setName(const String& value)
{
    setAttribute(nameAttr, value);
}

String HTMLPlugInElement::width() const
{
    return getAttribute(widthAttr);
}

void HTMLPlugInElement::setWidth(const String& value)
{
    setAttribute(widthAttr, value);
}

bool HTMLPlugInElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == widthAttr ||
        attrName == heightAttr ||
        attrName == vspaceAttr ||
        attrName == hspaceAttr) {
            result = eUniversal;
            return false;
    }
    
    if (attrName == alignAttr) {
        result = eReplaced; // Share with <img> since the alignment behavior is the same.
        return false;
    }
    
    return HTMLFrameOwnerElement::mapToEntry(attrName, result);
}

void HTMLPlugInElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == widthAttr)
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    else if (attr->name() == heightAttr)
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    else if (attr->name() == vspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
    } else if (attr->name() == hspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
    } else if (attr->name() == alignAttr)
        addHTMLAlignment(attr);
    else
        HTMLFrameOwnerElement::parseMappedAttribute(attr);
}    

bool HTMLPlugInElement::checkDTD(const Node* newChild)
{
    return newChild->hasTagName(paramTag) || HTMLFrameOwnerElement::checkDTD(newChild);
}

void HTMLPlugInElement::defaultEventHandler(Event* event)
{
    RenderObject* r = renderer();
    if (!r || !r->isWidget())
        return;

    if (Widget* widget = static_cast<RenderWidget*>(r)->widget())
        widget->handleEvent(event);
}

#if USE(NPOBJECT)

NPObject* HTMLPlugInElement::createNPObject()
{
    Frame* frame = document()->frame();
    if (!frame) {
        // This shouldn't ever happen, but might as well check anyway.
        ASSERT_NOT_REACHED();
        return frame->scriptBridge()->CreateNoScriptObject();
    }

    Settings* settings = frame->settings();
    if (!settings) {
        // This shouldn't ever happen, but might as well check anyway.
        ASSERT_NOT_REACHED();
      return frame->scriptBridge()->CreateNoScriptObject();
    }

    // Can't create NPObjects when JavaScript is disabled
    if (!frame->scriptBridge()->isEnabled())
        return frame->scriptBridge()->CreateNoScriptObject();
    
    // Create a JSObject bound to this element
    return frame->scriptBridge()->CreateScriptObject(frame, this);
}

NPObject* HTMLPlugInElement::getNPObject()
{
    if (!m_NPObject)
        m_NPObject = createNPObject();
    return m_NPObject;
}

#endif /* USE(NPOBJECT) */

void HTMLPlugInElement::detach()
{
#if USE(JAVASCRIPTCORE_BINDINGS) | USE(V8_BINDING)
    m_instance.Clear();
#endif
    HTMLFrameOwnerElement::detach();
}

void HTMLPlugInElement::updateWidgetCallback(Node* n)
{
    static_cast<HTMLPlugInElement*>(n)->updateWidget();
}

}
