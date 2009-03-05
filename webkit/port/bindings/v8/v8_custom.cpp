/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004-2006 Apple Computer, Inc.
 *  Copyright (C) 2006 James G. Speth (speth@end.com)
 *  Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
 *  Copyright 2007, 2008 Google Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <Assertions.h>
#include <wtf/ASCIICType.h>

#include "v8_proxy.h"
#include "v8_events.h"
#include "v8_binding.h"
#include "V8NPObject.h"
#include "v8_custom.h"

#include "V8Attr.h"
#include "V8CanvasGradient.h"
#include "V8CanvasPattern.h"
#include "V8Document.h"
#include "V8DOMWindow.h"
#include "V8HTMLCanvasElement.h"
#include "V8HTMLDocument.h"
#include "V8HTMLImageElement.h"
#include "V8HTMLOptionElement.h"
#include "V8Node.h"
#include "V8XPathNSResolver.h"
#include "V8XPathResult.h"

#include "Base64.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2D.h"
#include "CanvasStyle.h"
#include "Clipboard.h"
#include "ClipboardEvent.h"
#include "Console.h"
#include "DOMParser.h"
#include "DOMStringList.h"
#include "DOMTimer.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "Event.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "ExceptionCode.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "HTMLBodyElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLDocument.h"
#include "HTMLEmbedElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLFrameSetElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptionsCollection.h"
#include "HTMLSelectElement.h"
#include "History.h"
#include "JSXPathNSResolver.h"
#include "JSDOMBinding.h"
#include "KURL.h"
#include "Location.h"
#include "MessageChannel.h"
#include "MessagePort.h"
#include "MouseEvent.h"
#include "NodeIterator.h"
#include "NodeList.h"
#include "RGBColor.h"
#include "RenderPartObject.h"
#include "RenderWidget.h"
#include "ScheduledAction.h"
#include "ScriptState.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "StyleSheetList.h"
#include "TreeWalker.h"
#include "WebKitCSSMatrix.h"
#include "WebKitPoint.h"
#include "XMLSerializer.h"
#include "XPathEvaluator.h"
#include "XPathResult.h"
#include "XSLTProcessor.h"

#if ENABLE(SVG)
#include "V8SVGPODTypeWrapper.h"
#include "SVGElementInstance.h"
#include "SVGException.h"
#include "SVGPathSeg.h"
#endif

#include "Navigator.h"

namespace WebCore {

CALLBACK_FUNC_DECL(WebKitPointConstructor) {
  INC_STATS("DOM.WebKitPoint.Constructor");
  return V8Proxy::ConstructDOMObject<V8ClassIndex::WEBKITPOINT,
                                     WebKitPoint>(args);
}

// DOMImplementation is a singleton in WebCore.  If we use our normal
// mapping from DOM objects to V8 wrappers, the same wrapper will be
// shared for all frames in the same process.  This is a major
// security problem.  Therefore, we generate a DOMImplementation
// wrapper per document and store it in an internal field of the
// document.  Since the DOMImplementation object is a singleton, we do
// not have to do anything to keep the DOMImplementation object alive
// for the lifetime of the wrapper.
ACCESSOR_GETTER(DocumentImplementation) {
  ASSERT(info.Holder()->InternalFieldCount() >=
         kDocumentMinimumInternalFieldCount);
  // Check if the internal field already contains a wrapper.
  v8::Local<v8::Value> implementation =
      info.Holder()->GetInternalField(kDocumentImplementationIndex);
  if (!implementation->IsUndefined()) {
    return implementation;
  }
  // Generate a wrapper.
  Document* doc = V8Proxy::DOMWrapperToNative<Document>(info.Holder());
  v8::Handle<v8::Value> wrapper =
      V8Proxy::DOMImplementationToV8Object(doc->implementation());
  // Store the wrapper in the internal field.
  info.Holder()->SetInternalField(kDocumentImplementationIndex, wrapper);

  return wrapper;
}


ACCESSOR_GETTER(DocumentLocation) {
  Document* imp = V8Proxy::DOMWrapperToNative<Document>(info.Holder());
  if (!imp->frame())
    return v8::Null();

  DOMWindow* window = imp->frame()->domWindow();
  return V8Proxy::ToV8Object(V8ClassIndex::LOCATION, window->location());
}


ACCESSOR_SETTER(DocumentLocation) {
  Document* imp = V8Proxy::DOMWrapperToNative<Document>(info.Holder());
  if (!imp->frame())
    return;

  DOMWindow* window = imp->frame()->domWindow();
  // WindowSetLocation does security checks.  // XXXMB- verify!
  WindowSetLocation(window, ToWebCoreString(value));
}


INDEXED_PROPERTY_GETTER(HTMLFormElement) {
  INC_STATS("DOM.HTMLFormElement.IndexedPropertyGetter");
  HTMLFormElement* form =
      V8Proxy::DOMWrapperToNode<HTMLFormElement>(info.Holder());

  RefPtr<Node> result = form->elements()->item(index);
  if (!result) return v8::Handle<v8::Value>();
  return V8Proxy::NodeToV8Object(result.get());
}


INDEXED_PROPERTY_GETTER(HTMLOptionsCollection) {
  INC_STATS("DOM.HTMLOptionsCollection.IndexedPropertyGetter");
  HTMLOptionsCollection* collection =
      V8Proxy::ToNativeObject<HTMLOptionsCollection>(
          V8ClassIndex::HTMLOPTIONSCOLLECTION, info.Holder());

  RefPtr<Node> result = collection->item(index);
  if (!result) return v8::Handle<v8::Value>();

  return V8Proxy::NodeToV8Object(result.get());
}

static v8::Handle<v8::Value> OptionsCollectionSetter(uint32_t index,
    v8::Handle<v8::Value> value, HTMLSelectElement* base) {
  if (value->IsNull() || value->IsUndefined()) {
    base->remove(index);
    return value;
  }

  ExceptionCode ec = 0;

  // Check that the value is an HTMLOptionElement.  If not, throw a
  // TYPE_MISMATCH_ERR DOMException.
  if (!V8HTMLOptionElement::HasInstance(value)) {
    V8Proxy::SetDOMException(TYPE_MISMATCH_ERR);
    return value;
  }

  HTMLOptionElement* element =
      V8Proxy::DOMWrapperToNode<HTMLOptionElement>(
          v8::Handle<v8::Object>::Cast(value));
  base->setOption(index, element, ec);

  V8Proxy::SetDOMException(ec);
  return value;
}


INDEXED_PROPERTY_SETTER(HTMLOptionsCollection) {
  INC_STATS("DOM.HTMLOptionsCollection.IndexedPropertySetter");
  HTMLOptionsCollection* collection =
      V8Proxy::ToNativeObject<HTMLOptionsCollection>(
          V8ClassIndex::HTMLOPTIONSCOLLECTION, info.Holder());
  HTMLSelectElement* base = static_cast<HTMLSelectElement*>(collection->base());
  return OptionsCollectionSetter(index, value, base);
}


INDEXED_PROPERTY_SETTER(HTMLSelectElementCollection) {
  INC_STATS("DOM.HTMLSelectElementCollection.IndexedPropertySetter");
  HTMLSelectElement* select =
      V8Proxy::DOMWrapperToNode<HTMLSelectElement>(info.Holder());
  return OptionsCollectionSetter(index, value, select);
}

// CanvasRenderingContext2D ----------------------------------------------------

// Helper macro for converting v8 values into floats (expected by many of the
// canvas functions).
#define TO_FLOAT(a) static_cast<float>((a)->NumberValue())

// TODO: SetStrokeColor and SetFillColor are similar except function names,
// consolidate them into one.
CALLBACK_FUNC_DECL(CanvasRenderingContext2DSetStrokeColor) {
  INC_STATS("DOM.CanvasRenderingContext2D.setStrokeColor()");
  CanvasRenderingContext2D* context =
      V8Proxy::ToNativeObject<CanvasRenderingContext2D>(
          V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());
  switch (args.Length()) {
    case 1:
      if (args[0]->IsString()) {
        context->setStrokeColor(ToWebCoreString(args[0]));
      } else {
        context->setStrokeColor(TO_FLOAT(args[0]));
      }
      break;
    case 2:
      if (args[0]->IsString()) {
        context->setStrokeColor(ToWebCoreString(args[0]),
                                TO_FLOAT(args[1]));
      } else {
        context->setStrokeColor(TO_FLOAT(args[0]),
                                TO_FLOAT(args[1]));
      }
      break;
    case 4:
      context->setStrokeColor(TO_FLOAT(args[0]),
                              TO_FLOAT(args[1]),
                              TO_FLOAT(args[2]),
                              TO_FLOAT(args[3]));
      break;
    case 5:
      context->setStrokeColor(TO_FLOAT(args[0]),
                              TO_FLOAT(args[1]),
                              TO_FLOAT(args[2]),
                              TO_FLOAT(args[3]),
                              TO_FLOAT(args[4]));
      break;
    default:
      V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR,
                          "setStrokeColor: Invalid number of arguments");
      break;
  }
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DSetFillColor) {
  INC_STATS("DOM.CanvasRenderingContext2D.setFillColor()");
  CanvasRenderingContext2D* context =
      V8Proxy::ToNativeObject<CanvasRenderingContext2D>(
          V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());
  switch (args.Length()) {
    case 1:
      if (args[0]->IsString()) {
        context->setFillColor(ToWebCoreString(args[0]));
      } else {
        context->setFillColor(TO_FLOAT(args[0]));
      }
      break;
    case 2:
      if (args[0]->IsString()) {
        context->setFillColor(ToWebCoreString(args[0]), TO_FLOAT(args[1]));
      } else {
        context->setFillColor(TO_FLOAT(args[0]), TO_FLOAT(args[1]));
      }
      break;
    case 4:
      context->setFillColor(TO_FLOAT(args[0]),
                            TO_FLOAT(args[1]),
                            TO_FLOAT(args[2]),
                            TO_FLOAT(args[3]));
      break;
    case 5:
      context->setFillColor(TO_FLOAT(args[0]),
                            TO_FLOAT(args[1]),
                            TO_FLOAT(args[2]),
                            TO_FLOAT(args[3]),
                            TO_FLOAT(args[4]));
      break;
    default:
      V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR,
                          "setFillColor: Invalid number of arguments");
      break;
  }
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DStrokeRect) {
  INC_STATS("DOM.CanvasRenderingContext2D.strokeRect()");
  CanvasRenderingContext2D* context =
      V8Proxy::ToNativeObject<CanvasRenderingContext2D>(
          V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());
  if (args.Length() == 5) {
    context->strokeRect(TO_FLOAT(args[0]),
                        TO_FLOAT(args[1]),
                        TO_FLOAT(args[2]),
                        TO_FLOAT(args[3]),
                        TO_FLOAT(args[4]));
  } else if (args.Length() == 4) {
    context->strokeRect(TO_FLOAT(args[0]),
                        TO_FLOAT(args[1]),
                        TO_FLOAT(args[2]),
                        TO_FLOAT(args[3]));
  } else {
    V8Proxy::SetDOMException(INDEX_SIZE_ERR);
    return v8::Handle<v8::Value>();
  }

  return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DSetShadow) {
  INC_STATS("DOM.CanvasRenderingContext2D.setShadow()");
  CanvasRenderingContext2D* context =
      V8Proxy::ToNativeObject<CanvasRenderingContext2D>(
          V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

  switch (args.Length()) {
    case 3:
      context->setShadow(TO_FLOAT(args[0]), TO_FLOAT(args[1]),
                         TO_FLOAT(args[2]));
      break;
    case 4:
      if (args[3]->IsString())
        context->setShadow(TO_FLOAT(args[0]), TO_FLOAT(args[1]),
                           TO_FLOAT(args[2]), ToWebCoreString(args[3]));
      else
        context->setShadow(TO_FLOAT(args[0]), TO_FLOAT(args[1]),
                           TO_FLOAT(args[2]), TO_FLOAT(args[3]));
      break;
    case 5:
      if (args[3]->IsString())
        context->setShadow(TO_FLOAT(args[0]), TO_FLOAT(args[1]),
                           TO_FLOAT(args[2]), ToWebCoreString(args[3]),
                           TO_FLOAT(args[4]));
      else
        context->setShadow(TO_FLOAT(args[0]), TO_FLOAT(args[1]),
                           TO_FLOAT(args[2]), TO_FLOAT(args[3]),
                           TO_FLOAT(args[4]));
      break;
    case 7:
      context->setShadow(TO_FLOAT(args[0]), TO_FLOAT(args[1]),
                         TO_FLOAT(args[2]), TO_FLOAT(args[3]),
                         TO_FLOAT(args[4]), TO_FLOAT(args[5]),
                         TO_FLOAT(args[6]));
      break;
    case 8:
      context->setShadow(TO_FLOAT(args[0]), TO_FLOAT(args[1]),
                         TO_FLOAT(args[2]), TO_FLOAT(args[3]),
                         TO_FLOAT(args[4]), TO_FLOAT(args[5]),
                         TO_FLOAT(args[6]), TO_FLOAT(args[7]));
      break;
    default:
      V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR,
                          "setShadow: Invalid number of arguments");
      break;
  }

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DDrawImage) {
  INC_STATS("DOM.CanvasRenderingContext2D.drawImage()");
  CanvasRenderingContext2D* context =
      V8Proxy::ToNativeObject<CanvasRenderingContext2D>(
          V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

  v8::Handle<v8::Value> arg = args[0];

  if (V8HTMLImageElement::HasInstance(arg)) {
    ExceptionCode ec = 0;
    HTMLImageElement* image_element =
        V8Proxy::DOMWrapperToNode<HTMLImageElement>(arg);
    switch (args.Length()) {
    case 3:
      context->drawImage(image_element, TO_FLOAT(args[1]), TO_FLOAT(args[2]));
      break;
    case 5:
      context->drawImage(image_element, TO_FLOAT(args[1]), TO_FLOAT(args[2]),
                         TO_FLOAT(args[3]), TO_FLOAT(args[4]), ec);
      if (ec != 0) {
        V8Proxy::SetDOMException(ec);
        return v8::Handle<v8::Value>();
      }
      break;
    case 9:
      context->drawImage(image_element,
                         FloatRect(TO_FLOAT(args[1]), TO_FLOAT(args[2]),
                                   TO_FLOAT(args[3]), TO_FLOAT(args[4])),
                         FloatRect(TO_FLOAT(args[5]), TO_FLOAT(args[6]),
                                   TO_FLOAT(args[7]), TO_FLOAT(args[8])),
                         ec);
      if (ec != 0) {
        V8Proxy::SetDOMException(ec);
        return v8::Handle<v8::Value>();
      }
      break;
    default:
      V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR,
                          "drawImage: Invalid number of arguments");
      return v8::Undefined();
    }
    return v8::Undefined();
  }

  // HTMLCanvasElement
  if (V8HTMLCanvasElement::HasInstance(arg)) {
    ExceptionCode ec = 0;
    HTMLCanvasElement* canvas_element =
        V8Proxy::DOMWrapperToNode<HTMLCanvasElement>(arg);
    switch (args.Length()) {
    case 3:
      context->drawImage(canvas_element, TO_FLOAT(args[1]), TO_FLOAT(args[2]));
      break;
    case 5:
      context->drawImage(canvas_element, TO_FLOAT(args[1]), TO_FLOAT(args[2]),
                         TO_FLOAT(args[3]), TO_FLOAT(args[4]), ec);
      if (ec != 0) {
        V8Proxy::SetDOMException(ec);
        return v8::Handle<v8::Value>();
      }
      break;
    case 9:
      context->drawImage(canvas_element,
                         FloatRect(TO_FLOAT(args[1]), TO_FLOAT(args[2]),
                                   TO_FLOAT(args[3]), TO_FLOAT(args[4])),
                         FloatRect(TO_FLOAT(args[5]), TO_FLOAT(args[6]),
                                   TO_FLOAT(args[7]), TO_FLOAT(args[8])),
                         ec);
      if (ec != 0) {
        V8Proxy::SetDOMException(ec);
        return v8::Handle<v8::Value>();
      }
      break;
    default:
      V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR,
                          "drawImage: Invalid number of arguments");
      return v8::Undefined();
    }
    return v8::Undefined();
  }

  V8Proxy::SetDOMException(TYPE_MISMATCH_ERR);
  return v8::Handle<v8::Value>();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DDrawImageFromRect) {
  INC_STATS("DOM.CanvasRenderingContext2D.drawImageFromRect()");
  CanvasRenderingContext2D* context =
      V8Proxy::ToNativeObject<CanvasRenderingContext2D>(
          V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

  v8::Handle<v8::Value> arg = args[0];

  if (V8HTMLImageElement::HasInstance(arg)) {
    HTMLImageElement* image_element =
        V8Proxy::DOMWrapperToNode<HTMLImageElement>(arg);
    context->drawImageFromRect(image_element,
                               TO_FLOAT(args[1]), TO_FLOAT(args[2]),
                               TO_FLOAT(args[3]), TO_FLOAT(args[4]),
                               TO_FLOAT(args[5]), TO_FLOAT(args[6]),
                               TO_FLOAT(args[7]), TO_FLOAT(args[8]),
                               ToWebCoreString(args[9]));
  } else {
    V8Proxy::ThrowError(V8Proxy::TYPE_ERROR,
                        "drawImageFromRect: Invalid type of arguments");
  }

  return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DCreatePattern) {
  INC_STATS("DOM.CanvasRenderingContext2D.createPattern()");
  CanvasRenderingContext2D* context =
      V8Proxy::ToNativeObject<CanvasRenderingContext2D>(
          V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

  v8::Handle<v8::Value> arg = args[0];

  if (V8HTMLImageElement::HasInstance(arg)) {
    HTMLImageElement* image_element =
        V8Proxy::DOMWrapperToNode<HTMLImageElement>(arg);
    ExceptionCode ec = 0;
    RefPtr<CanvasPattern> pattern =
        context->createPattern(image_element, valueToStringWithNullCheck(args[1]), ec);
    if (ec != 0) {
      V8Proxy::SetDOMException(ec);
      return v8::Handle<v8::Value>();
    }
    return V8Proxy::ToV8Object(V8ClassIndex::CANVASPATTERN, pattern.get());
  }

  if (V8HTMLCanvasElement::HasInstance(arg)) {
    HTMLCanvasElement* canvas_element =
        V8Proxy::DOMWrapperToNode<HTMLCanvasElement>(arg);
    ExceptionCode ec = 0;
    RefPtr<CanvasPattern> pattern =
        context->createPattern(canvas_element, valueToStringWithNullCheck(args[1]), ec);
    if (ec != 0) {
      V8Proxy::SetDOMException(ec);
      return v8::Handle<v8::Value>();
    }
    return V8Proxy::ToV8Object(V8ClassIndex::CANVASPATTERN, pattern.get());
  }

  V8Proxy::SetDOMException(TYPE_MISMATCH_ERR);
  return v8::Handle<v8::Value>();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DFillText) {
  INC_STATS("DOM.CanvasRenderingContext2D.fillText()");

  CanvasRenderingContext2D* context =
      V8Proxy::ToNativeObject<CanvasRenderingContext2D>(
          V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

  // Two forms:
  // * fillText(text, x, y)
  // * fillText(text, x, y, maxWidth)
  if (args.Length() < 3 || args.Length() > 4) {
    V8Proxy::SetDOMException(SYNTAX_ERR);
    return v8::Handle<v8::Value>();
  }

  String text = ToWebCoreString(args[0]);
  float x = TO_FLOAT(args[1]);
  float y = TO_FLOAT(args[2]);

  if (args.Length() == 4) {
    float maxWidth = TO_FLOAT(args[3]);
    context->fillText(text, x, y, maxWidth);
  } else {
    context->fillText(text, x, y);
  }

  return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DStrokeText) {
  INC_STATS("DOM.CanvasRenderingContext2D.strokeText()");

  CanvasRenderingContext2D* context =
      V8Proxy::ToNativeObject<CanvasRenderingContext2D>(
          V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

  // Two forms:
  // * strokeText(text, x, y)
  // * strokeText(text, x, y, maxWidth)
  if (args.Length() < 3 || args.Length() > 4) {
    V8Proxy::SetDOMException(SYNTAX_ERR);
    return v8::Handle<v8::Value>();
  }

  String text = ToWebCoreString(args[0]);
  float x = TO_FLOAT(args[1]);
  float y = TO_FLOAT(args[2]);

  if (args.Length() == 4) {
    float maxWidth = TO_FLOAT(args[3]);
    context->strokeText(text, x, y, maxWidth);
  } else {
    context->strokeText(text, x, y);
  }

  return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DPutImageData) {
  INC_STATS("DOM.CanvasRenderingContext2D.putImageData()");

  // Two froms:
  // * putImageData(ImageData, x, y)
  // * putImageData(ImageData, x, y, dirtyX, dirtyY, dirtyWidth, dirtyHeight)
  if (args.Length() != 3 && args.Length() != 7) {
    V8Proxy::SetDOMException(SYNTAX_ERR);
    return v8::Handle<v8::Value>();
  }

  CanvasRenderingContext2D* context =
      V8Proxy::ToNativeObject<CanvasRenderingContext2D>(
          V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

  ImageData* imageData = 0;

  // Need to check that the argument is of the correct type, since
  // ToNativeObject() expects it to be correct. If the argument was incorrect
  // we leave it null, and putImageData() will throw the correct exception
  // (TYPE_MISMATCH_ERR).
  if (V8Proxy::IsWrapperOfType(args[0], V8ClassIndex::IMAGEDATA)) {
    imageData = V8Proxy::ToNativeObject<ImageData>(
        V8ClassIndex::IMAGEDATA, args[0]);
  }

  ExceptionCode ec = 0;

  if (args.Length() == 7) {
    context->putImageData(imageData,
        TO_FLOAT(args[1]), TO_FLOAT(args[2]), TO_FLOAT(args[3]),
        TO_FLOAT(args[4]), TO_FLOAT(args[5]), TO_FLOAT(args[6]), ec);
  } else {
    context->putImageData(imageData, TO_FLOAT(args[1]), TO_FLOAT(args[2]), ec);
  }

  if (ec != 0) {
    V8Proxy::SetDOMException(ec);
    return v8::Handle<v8::Value>();
  }

  return v8::Undefined();
}

static bool AllowSettingSrcToJavascriptURL(Element* element, String name,
                                           String value) {
  // Need to parse value as URL first in order to check its protocol.
  // " javascript:", "java\0script:", "javascript\t:", "javascript\1:"
  // are all parsed as "javascript:" url.
  // When changing location in HTMLFrameElement, value is parsed as url.
  // We must match the behavior there.
  // See issue 804099.
  //
  // parseURL is defined in CSSHelper.cpp.
  if ((element->hasTagName(HTMLNames::iframeTag) ||
       element->hasTagName(HTMLNames::frameTag)) &&
      equalIgnoringCase(name, "src") &&
      parseURL(value).startsWith("javascript:", false)) {
    HTMLFrameElementBase* frame = static_cast<HTMLFrameElementBase*>(element);
    Node* contentDoc = frame->contentDocument();
    if (contentDoc && !V8Proxy::CheckNodeSecurity(contentDoc))
      return false;
  }
  return true;
}


static bool AllowSettingFrameSrcToJavascriptUrl(HTMLFrameElementBase* frame,
                                                String value) {
  // See same issue in AllowSettingSrcToJavascriptURL.
  if (parseURL(value).startsWith("javascript:", false)) {
    Node* contentDoc = frame->contentDocument();
    if (contentDoc && !V8Proxy::CheckNodeSecurity(contentDoc))
      return false;
  }
  return true;
}


// Element ---------------------------------------------------------------------

CALLBACK_FUNC_DECL(ElementSetAttribute) {
  INC_STATS("DOM.Element.setAttribute()");
  Element* imp = V8Proxy::DOMWrapperToNode<Element>(args.Holder());
  ExceptionCode ec = 0;
  String name = ToWebCoreString(args[0]);
  String value = ToWebCoreString(args[1]);

  if (!AllowSettingSrcToJavascriptURL(imp, name, value)) {
    return v8::Undefined();
  }

  imp->setAttribute(name, value, ec);
  if (ec != 0) {
    V8Proxy::SetDOMException(ec);
    return v8::Handle<v8::Value>();
  }
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(ElementSetAttributeNode) {
  INC_STATS("DOM.Element.setAttributeNode()");
  if (!V8Attr::HasInstance(args[0])) {
    V8Proxy::SetDOMException(TYPE_MISMATCH_ERR);
    return v8::Handle<v8::Value>();
  }

  Attr* newAttr = V8Proxy::DOMWrapperToNode<Attr>(args[0]);
  Element* imp = V8Proxy::DOMWrapperToNode<Element>(args.Holder());
  ExceptionCode ec = 0;

  if (!AllowSettingSrcToJavascriptURL(imp, newAttr->name(), newAttr->value())) {
    return v8::Undefined();
  }

  RefPtr<Attr> result = imp->setAttributeNode(newAttr, ec);
  if (ec != 0) {
    V8Proxy::SetDOMException(ec);
    return v8::Handle<v8::Value>();
  }
  return V8Proxy::NodeToV8Object(result.get());
}

CALLBACK_FUNC_DECL(ElementSetAttributeNS) {
  INC_STATS("DOM.Element.setAttributeNS()");
  Element* imp = V8Proxy::DOMWrapperToNode<Element>(args.Holder());
  ExceptionCode ec = 0;
  String namespaceURI = valueToStringWithNullCheck(args[0]);
  String qualifiedName = ToWebCoreString(args[1]);
  String value = ToWebCoreString(args[2]);

  if (!AllowSettingSrcToJavascriptURL(imp, qualifiedName, value)) {
    return v8::Undefined();
  }

  imp->setAttributeNS(namespaceURI, qualifiedName, value, ec);
  if (ec != 0) {
    V8Proxy::SetDOMException(ec);
    return v8::Handle<v8::Value>();
  }
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(ElementSetAttributeNodeNS) {
  INC_STATS("DOM.Element.setAttributeNodeNS()");
  if (!V8Attr::HasInstance(args[0])) {
    V8Proxy::SetDOMException(TYPE_MISMATCH_ERR);
    return v8::Handle<v8::Value>();
  }

  Attr* newAttr = V8Proxy::DOMWrapperToNode<Attr>(args[0]);
  Element* imp = V8Proxy::DOMWrapperToNode<Element>(args.Holder());
  ExceptionCode ec = 0;

  if (!AllowSettingSrcToJavascriptURL(imp, newAttr->name(), newAttr->value())) {
    return v8::Undefined();
  }

  RefPtr<Attr> result = imp->setAttributeNodeNS(newAttr, ec);
  if (ec != 0) {
    V8Proxy::SetDOMException(ec);
    return v8::Handle<v8::Value>();
  }
  return V8Proxy::NodeToV8Object(result.get());
}



// Attr ------------------------------------------------------------------------

ACCESSOR_SETTER(AttrValue) {
  Attr* imp =
      V8Proxy::DOMWrapperToNode<Attr>(info.Holder());
  String v = valueToStringWithNullCheck(value);
  Element* ownerElement = imp->ownerElement();

  if (ownerElement &&
      !AllowSettingSrcToJavascriptURL(ownerElement, imp->name(), v))
    return;

  ExceptionCode ec = 0;
  imp->setValue(v, ec);
  V8Proxy::SetDOMException(ec);
}


// HTMLFrameElement ------------------------------------------------------------

ACCESSOR_SETTER(HTMLFrameElementSrc) {
  HTMLFrameElement* imp =
      V8Proxy::DOMWrapperToNode<HTMLFrameElement>(info.Holder());
  String v = valueToStringWithNullCheck(value);

  if (!AllowSettingFrameSrcToJavascriptUrl(imp, v)) return;

  imp->setSrc(v);
}

ACCESSOR_SETTER(HTMLFrameElementLocation) {
  HTMLFrameElement* imp =
      V8Proxy::DOMWrapperToNode<HTMLFrameElement>(info.Holder());
  String v = valueToStringWithNullCheck(value);

  if (!AllowSettingFrameSrcToJavascriptUrl(imp, v)) return;

  imp->setLocation(v);
}


// HTMLIFrameElement -----------------------------------------------------------

ACCESSOR_SETTER(HTMLIFrameElementSrc) {
  HTMLIFrameElement* imp =
      V8Proxy::DOMWrapperToNode<HTMLIFrameElement>(info.Holder());
  String v = valueToStringWithNullCheck(value);

  if (!AllowSettingFrameSrcToJavascriptUrl(imp, v)) return;

  imp->setSrc(v);
}


// TODO(mbelshe): This should move into V8DOMWindowCustom.cpp
// Can't move it right now because it depends on V8ScheduledAction,
// which is private to this file (v8_custom.cpp).
v8::Handle<v8::Value> V8Custom::WindowSetTimeoutImpl(const v8::Arguments& args,
                                                     bool single_shot) {
  int num_arguments = args.Length();

  if (num_arguments < 1) return v8::Undefined();

  DOMWindow* imp = V8Proxy::ToNativeObject<DOMWindow>(
      V8ClassIndex::DOMWINDOW, args.Holder());

  if (!imp->frame())
    return v8::Undefined();

  if (!V8Proxy::CanAccessFrame(imp->frame(), true))
    return v8::Undefined();

  ScriptExecutionContext* script_context =
    static_cast<ScriptExecutionContext*>(imp->frame()->document());

  v8::Handle<v8::Value> function = args[0];

  int32_t timeout = 0;
  if (num_arguments >= 2) timeout = args[1]->Int32Value();

  int id;
  if (function->IsString()) {
    // Don't allow setting timeouts to run empty functions!
    // (Bug 1009597)
    WebCore::String string_function = ToWebCoreString(function);
    if (string_function.length() == 0)
      return v8::Undefined();

    id = DOMTimer::install(script_context,
                           new ScheduledAction(string_function), timeout,
                           single_shot);
  } else if (function->IsFunction()) {
    int param_count = num_arguments >= 2 ? num_arguments - 2 : 0;
    v8::Local<v8::Value>* params = 0;
    if (param_count > 0) {
      params = new v8::Local<v8::Value>[param_count];
      for (int i = 0; i < param_count; i++)
        // parameters must be globalized
        params[i] = args[i+2];
    }

    // params is passed to action, and released in action's destructor
    ScheduledAction* action = new ScheduledAction(
        v8::Handle<v8::Function>::Cast(function), param_count, params);

    delete[] params;

    id = DOMTimer::install(script_context, action, timeout, single_shot);
  } else {
    // TODO(fqian): what's the right return value if failed.
    return v8::Undefined();
  }
  return v8::Integer::New(id);
}


// HTMLDocument ----------------------------------------------------------------

// Concatenates "args" to a string. If args is empty, returns empty string.
// Firefox/Safari/IE support non-standard arguments to document.write, ex:
//   document.write("a", "b", "c") --> document.write("abc")
//   document.write() --> document.write("")
static String WriteHelper_GetString(const v8::Arguments& args) {
  String str = "";
  for (int i = 0; i < args.Length(); ++i) {
    str += ToWebCoreString(args[i]);
  }
  return str;
}

CALLBACK_FUNC_DECL(HTMLDocumentWrite) {
  INC_STATS("DOM.HTMLDocument.write()");
  HTMLDocument* imp = V8Proxy::DOMWrapperToNode<HTMLDocument>(args.Holder());
  Frame* frame = V8Proxy::retrieveActiveFrame();
  ASSERT(frame);
  imp->write(WriteHelper_GetString(args), frame->document());
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(HTMLDocumentWriteln) {
  INC_STATS("DOM.HTMLDocument.writeln()");
  HTMLDocument* imp = V8Proxy::DOMWrapperToNode<HTMLDocument>(args.Holder());
  Frame* frame = V8Proxy::retrieveActiveFrame();
  ASSERT(frame);
  imp->writeln(WriteHelper_GetString(args), frame->document());
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(HTMLDocumentOpen) {
  INC_STATS("DOM.HTMLDocument.open()");
  HTMLDocument* imp = V8Proxy::DOMWrapperToNode<HTMLDocument>(args.Holder());

  if (args.Length() > 2) {
    if (Frame* frame = imp->frame()) {
      // Fetch the global object for the frame.
      v8::Local<v8::Context> context = V8Proxy::GetContext(frame);
      // Bail out if we cannot get the context.
      if (context.IsEmpty()) return v8::Undefined();
      v8::Local<v8::Object> global = context->Global();
      // Get the open property of the global object.
      v8::Local<v8::Value> function = global->Get(v8::String::New("open"));
      // If the open property is not a function throw a type error.
      if (!function->IsFunction()) {
        V8Proxy::ThrowError(V8Proxy::TYPE_ERROR, "open is not a function");
        return v8::Undefined();
      }
      // Wrap up the arguments and call the function.
      v8::Local<v8::Value>* params = new v8::Local<v8::Value>[args.Length()];
      for (int i = 0; i < args.Length(); i++) {
        params[i] = args[i];
      }

      V8Proxy* proxy = V8Proxy::retrieve(frame);
      ASSERT(proxy);

      v8::Local<v8::Value> result =
          proxy->CallFunction(v8::Local<v8::Function>::Cast(function),
                              global, args.Length(), params);
      delete[] params;
      return result;
    }
  }

  Frame* frame = V8Proxy::retrieveActiveFrame();
  imp->open(frame->document());
  // Return the document.
  return args.Holder();
}




// DOMWindow -------------------------------------------------------------------

static bool IsAscii(const String& str) {
  for (size_t i = 0; i < str.length(); i++) {
    if (str[i] > 0xFF)
      return false;
  }
  return true;
}

static v8::Handle<v8::Value> Base64Convert(const String& str, bool encode) {
  if (!IsAscii(str)) {
    V8Proxy::SetDOMException(INVALID_CHARACTER_ERR);
    return v8::Handle<v8::Value>();
  }

  Vector<char> in(str.length());
  for (size_t i = 0; i < str.length(); i++) {
    in[i] = static_cast<char>(str[i]);
  }
  Vector<char> out;

  if (encode) {
    base64Encode(in, out);
  } else {
    if (!base64Decode(in, out)) {
      V8Proxy::ThrowError(V8Proxy::GENERAL_ERROR, "Cannot decode base64");
      return v8::Undefined();
    }
  }

  return v8String(String(out.data(), out.size()));
}

CALLBACK_FUNC_DECL(DOMWindowAtob) {
  INC_STATS("DOM.DOMWindow.atob()");
  DOMWindow* imp = V8Proxy::ToNativeObject<DOMWindow>(
      V8ClassIndex::DOMWINDOW, args.Holder());

  if (!V8Proxy::CanAccessFrame(imp->frame(), true))
    return v8::Undefined();

  if (args.Length() < 1) {
    V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR, "Not enough arguments");
    return v8::Undefined();
  }

  if (args[0]->IsNull()) return v8String("");

  String str = ToWebCoreString(args[0]);
  return Base64Convert(str, false);
}

CALLBACK_FUNC_DECL(DOMWindowBtoa) {
  INC_STATS("DOM.DOMWindow.btoa()");
  DOMWindow* imp = V8Proxy::ToNativeObject<DOMWindow>(
      V8ClassIndex::DOMWINDOW, args.Holder());

  if (!V8Proxy::CanAccessFrame(imp->frame(), true))
    return v8::Undefined();

  if (args.Length() < 1) {
    V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR, "Not enough arguments");
    return v8::Undefined();
  }

  if (args[0]->IsNull()) return v8String("");

  String str = ToWebCoreString(args[0]);
  return Base64Convert(str, true);
}

// TODO(fqian): returning string is cheating, and we should
// fix this by calling toString function on the receiver.
// However, V8 implements toString in JavaScript, which requires
// switching context of receiver. I consider it is dangerous.
CALLBACK_FUNC_DECL(DOMWindowToString)
{
    INC_STATS("DOM.DOMWindow.toString()");
    return args.This()->ObjectProtoToString();
}

CALLBACK_FUNC_DECL(DOMWindowNOP)
{
    INC_STATS("DOM.DOMWindow.nop()");
    return v8::Undefined();
}


CALLBACK_FUNC_DECL(HTMLFormElementSubmit) {
  INC_STATS("DOM.HTMLFormElement.submit()");

  HTMLFormElement* form =
    V8Proxy::DOMWrapperToNative<HTMLFormElement>(args.Holder());

  form->submit(0, false, false);
  return v8::Undefined();
}

static String EventNameFromAttributeName(const String& name) {
  ASSERT(name.startsWith("on"));
  String event_type = name.substring(2);

  if (event_type.startsWith("w")) {
    switch(event_type[event_type.length() - 1]) {
      case 't':
        event_type = "webkitAnimationStart";
        break;
      case 'n':
        event_type = "webkitAnimationIteration";
        break;
      case 'd':
        ASSERT(event_type.length() > 7);
        if (event_type[7] == 'a')
          event_type = "webkitAnimationEnd";
        else
          event_type = "webkitTransitionEnd";
        break;
    }
  }

  return event_type;
}


ACCESSOR_SETTER(DOMWindowEventHandler) {
  v8::Handle<v8::Object> holder = V8Proxy::LookupDOMWrapper(
      V8ClassIndex::DOMWINDOW, info.This());
  if (holder.IsEmpty())
    return;

  DOMWindow* imp = V8Proxy::ToNativeObject<DOMWindow>(
      V8ClassIndex::DOMWINDOW, holder);
  if (!imp->frame())
    return;

  Document* doc = imp->frame()->document();
  if (!doc)
    return;

  String key = ToWebCoreString(name);
  String event_type = EventNameFromAttributeName(key);

  if (value->IsNull()) {
    // Clear the event listener
    doc->removeWindowInlineEventListenerForType(event_type);
  } else {
    V8Proxy* proxy = V8Proxy::retrieve(imp->frame());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateV8EventListener(value, true);
    if (listener) {
      doc->setWindowInlineEventListenerForType(event_type, listener);
    }
  }
}


ACCESSOR_GETTER(DOMWindowEventHandler) {
  v8::Handle<v8::Object> holder = V8Proxy::LookupDOMWrapper(
      V8ClassIndex::DOMWINDOW, info.This());
  if (holder.IsEmpty())
    return v8::Undefined();

  DOMWindow* imp = V8Proxy::ToNativeObject<DOMWindow>(
      V8ClassIndex::DOMWINDOW, holder);
  if (!imp->frame())
    return v8::Undefined();

  Document* doc = imp->frame()->document();
  if (!doc)
    return v8::Undefined();

  String key = ToWebCoreString(name);
  String event_type = EventNameFromAttributeName(key);

  EventListener* listener = doc->windowInlineEventListenerForType(event_type);
  return V8Proxy::EventListenerToV8Object(listener);
}


ACCESSOR_SETTER(ElementEventHandler) {
  Node* node = V8Proxy::DOMWrapperToNode<Node>(info.Holder());

  // Name starts with 'on', remove them.
  String key = ToWebCoreString(name);
  ASSERT(key.startsWith("on"));
  String event_type = key.substring(2);

  // Set handler if the value is a function.  Otherwise, clear the
  // event handler.
  if (value->IsFunction()) {
    V8Proxy* proxy = V8Proxy::retrieve(node->document()->frame());
    // the document might be created using createDocument,
    // which does not have a frame, use the active frame
    if (!proxy)
      proxy = V8Proxy::retrieve(V8Proxy::retrieveActiveFrame());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateV8EventListener(value, true);
    if (listener) {
      node->setInlineEventListenerForType(event_type, listener);
    }
  } else {
    node->removeInlineEventListenerForType(event_type);
  }
}


ACCESSOR_GETTER(ElementEventHandler) {
  Node* node = V8Proxy::DOMWrapperToNode<Node>(info.Holder());

  // Name starts with 'on', remove them.
  String key = ToWebCoreString(name);
  ASSERT(key.startsWith("on"));
  String event_type = key.substring(2);

  EventListener* listener = node->inlineEventListenerForType(event_type);
  return V8Proxy::EventListenerToV8Object(listener);
}

// --------------- Security Checks -------------------------
NAMED_ACCESS_CHECK(DOMWindow) {
  ASSERT(V8ClassIndex::FromInt(data->Int32Value()) == V8ClassIndex::DOMWINDOW);
  v8::Handle<v8::Value> window =
    V8Proxy::LookupDOMWrapper(V8ClassIndex::DOMWINDOW, host);
  if (window.IsEmpty())
      return false;  // the frame is gone.

  DOMWindow* target_win =
    V8Proxy::ToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, window);

  ASSERT(target_win);

  Frame* target = target_win->frame();
  if (!target)
    return false;

  if (key->IsString()) {
    String name = ToWebCoreString(key);

    // Allow access of GET and HAS if index is a subframe.
    if ((type == v8::ACCESS_GET || type == v8::ACCESS_HAS) &&
        target->tree()->child(name)) {
      return true;
    }
  }

  return V8Proxy::CanAccessFrame(target, false);
}


INDEXED_ACCESS_CHECK(DOMWindow) {
  ASSERT(V8ClassIndex::FromInt(data->Int32Value()) == V8ClassIndex::DOMWINDOW);
  v8::Handle<v8::Value> window =
    V8Proxy::LookupDOMWrapper(V8ClassIndex::DOMWINDOW, host);
  if (window.IsEmpty())
      return false;

  DOMWindow* target_win =
    V8Proxy::ToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, window);

  ASSERT(target_win);

  Frame* target = target_win->frame();
  if (!target)
    return false;

  // Allow access of GET and HAS if index is a subframe.
  if ((type == v8::ACCESS_GET || type == v8::ACCESS_HAS) &&
      target->tree()->child(index)) {
    return true;
  }

  return V8Proxy::CanAccessFrame(target, false);
}


INDEXED_ACCESS_CHECK(History) {
  ASSERT(V8ClassIndex::FromInt(data->Int32Value()) == V8ClassIndex::HISTORY);
  // Only allow same origin access
  History* imp =
      V8Proxy::ToNativeObject<History>(V8ClassIndex::HISTORY, host);
  return V8Proxy::CanAccessFrame(imp->frame(), false);
}


NAMED_ACCESS_CHECK(History) {
  ASSERT(V8ClassIndex::FromInt(data->Int32Value()) == V8ClassIndex::HISTORY);
  // Only allow same origin access
  History* imp =
      V8Proxy::ToNativeObject<History>(V8ClassIndex::HISTORY, host);
  return V8Proxy::CanAccessFrame(imp->frame(), false);
}



#undef INDEXED_ACCESS_CHECK
#undef NAMED_ACCESS_CHECK
#undef NAMED_PROPERTY_GETTER
#undef NAMED_PROPERTY_SETTER


// static
Frame* V8Custom::GetTargetFrame(v8::Local<v8::Object> host,
                                v8::Local<v8::Value> data) {
  Frame* target = 0;
  switch (V8ClassIndex::FromInt(data->Int32Value())) {
    case V8ClassIndex::DOMWINDOW: {
      v8::Handle<v8::Value> window =
          V8Proxy::LookupDOMWrapper(V8ClassIndex::DOMWINDOW, host);
      if (window.IsEmpty())
          return target;

      DOMWindow* target_win =
        V8Proxy::ToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, window);
      target = target_win->frame();
      break;
    }
    case V8ClassIndex::LOCATION: {
      History* imp =
          V8Proxy::ToNativeObject<History>(V8ClassIndex::HISTORY, host);
      target = imp->frame();
      break;
    }
    case V8ClassIndex::HISTORY: {
      Location* imp =
          V8Proxy::ToNativeObject<Location>(V8ClassIndex::LOCATION, host);
      target = imp->frame();
      break;
    }
    default:
      break;
  }
  return target;
}

#if ENABLE(SVG)
V8ClassIndex::V8WrapperType V8Custom::DowncastSVGPathSeg(void* path_seg) {
  WebCore::SVGPathSeg *real_path_seg =
      reinterpret_cast<WebCore::SVGPathSeg*>(path_seg);

  switch (real_path_seg->pathSegType()) {
#define MAKE_CASE(svg_val, v8_val) \
  case WebCore::SVGPathSeg::svg_val:           \
    return V8ClassIndex::v8_val;

MAKE_CASE(PATHSEG_CLOSEPATH,                    SVGPATHSEGCLOSEPATH)
MAKE_CASE(PATHSEG_MOVETO_ABS,                   SVGPATHSEGMOVETOABS)
MAKE_CASE(PATHSEG_MOVETO_REL,                   SVGPATHSEGMOVETOREL)
MAKE_CASE(PATHSEG_LINETO_ABS,                   SVGPATHSEGLINETOABS)
MAKE_CASE(PATHSEG_LINETO_REL,                   SVGPATHSEGLINETOREL)
MAKE_CASE(PATHSEG_CURVETO_CUBIC_ABS,            SVGPATHSEGCURVETOCUBICABS)
MAKE_CASE(PATHSEG_CURVETO_CUBIC_REL,            SVGPATHSEGCURVETOCUBICREL)
MAKE_CASE(PATHSEG_CURVETO_QUADRATIC_ABS,        SVGPATHSEGCURVETOQUADRATICABS)
MAKE_CASE(PATHSEG_CURVETO_QUADRATIC_REL,        SVGPATHSEGCURVETOQUADRATICREL)
MAKE_CASE(PATHSEG_ARC_ABS,                      SVGPATHSEGARCABS)
MAKE_CASE(PATHSEG_ARC_REL,                      SVGPATHSEGARCREL)
MAKE_CASE(PATHSEG_LINETO_HORIZONTAL_ABS,        SVGPATHSEGLINETOHORIZONTALABS)
MAKE_CASE(PATHSEG_LINETO_HORIZONTAL_REL,        SVGPATHSEGLINETOHORIZONTALREL)
MAKE_CASE(PATHSEG_LINETO_VERTICAL_ABS,          SVGPATHSEGLINETOVERTICALABS)
MAKE_CASE(PATHSEG_LINETO_VERTICAL_REL,          SVGPATHSEGLINETOVERTICALREL)
MAKE_CASE(PATHSEG_CURVETO_CUBIC_SMOOTH_ABS,     SVGPATHSEGCURVETOCUBICSMOOTHABS)
MAKE_CASE(PATHSEG_CURVETO_CUBIC_SMOOTH_REL,     SVGPATHSEGCURVETOCUBICSMOOTHREL)
MAKE_CASE(PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS, \
          SVGPATHSEGCURVETOQUADRATICSMOOTHABS)
MAKE_CASE(PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL, \
          SVGPATHSEGCURVETOQUADRATICSMOOTHREL)

#undef MAKE_CASE

  default:
    return V8ClassIndex::INVALID_CLASS_INDEX;
  }
}

#endif  // ENABLE(SVG)

}  // namespace WebCore
