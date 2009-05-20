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

// --------------- Security Checks -------------------------
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
