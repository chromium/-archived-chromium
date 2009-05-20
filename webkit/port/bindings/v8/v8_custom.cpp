/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *  Copyright (C) 2006 James G. Speth (speth@end.com)
 *  Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
 *  Copyright (C) 2007, 2008 Google Inc. All Rights Reserved.
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
#include "v8_binding.h"
#include "V8NPObject.h"

#include "V8CanvasGradient.h"
#include "V8CanvasPattern.h"
#include "V8CustomEventListener.h"
#include "V8Document.h"
#include "V8DOMWindow.h"
#include "V8HTMLCanvasElement.h"
#include "V8HTMLDocument.h"
#include "V8HTMLImageElement.h"
#include "V8NamedNodesCollection.h"
#include "V8Node.h"
#include "V8Proxy.h"
#include "V8XPathNSResolver.h"
#include "V8XPathResult.h"

#include "Attr.h"
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
#include "EventTarget.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "HTMLBodyElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLDocument.h"
#include "HTMLEmbedElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "History.h"
#include "JSXPathNSResolver.h"
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
    imp->clearAttributeEventListener(event_type);
  } else {
    V8Proxy* proxy = V8Proxy::retrieve(imp->frame());
    if (!proxy)
      return;

    RefPtr<EventListener> listener =
      proxy->FindOrCreateV8EventListener(value, true);
    if (listener) {
      imp->setAttributeEventListener(event_type, listener);
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

  EventListener* listener = imp->getAttributeEventListener(event_type);
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
