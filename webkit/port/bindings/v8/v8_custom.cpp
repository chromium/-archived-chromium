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
#include "v8_npobject.h"
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

class V8ScheduledAction : public ScheduledAction {
 public:
  V8ScheduledAction(v8::Handle<v8::Function> func, int argc,
                    v8::Handle<v8::Value> argv[]);
  explicit V8ScheduledAction(const WebCore::String& code) : m_argc(0),
      m_argv(0), m_code(code) { }
  virtual ~V8ScheduledAction();
  virtual void execute(ScriptExecutionContext* window);

 private:
  v8::Persistent<v8::Function> m_func;
  int m_argc;
  v8::Persistent<v8::Value>* m_argv;

  ScriptSourceCode m_code;
};

V8ScheduledAction::V8ScheduledAction(v8::Handle<v8::Function> func, int argc,
                                     v8::Handle<v8::Value> argv[])
    : m_code(String(), KURL(), 0) {
  m_func = v8::Persistent<v8::Function>::New(func);

#ifndef NDEBUG
  V8Proxy::RegisterGlobalHandle(SCHEDULED_ACTION, this, m_func);
#endif

  m_argc = argc;
  if (argc > 0) {
    m_argv = new v8::Persistent<v8::Value>[argc];
    for (int i = 0; i < argc; i++) {
      m_argv[i] = v8::Persistent<v8::Value>::New(argv[i]);

#ifndef NDEBUG
      V8Proxy::RegisterGlobalHandle(SCHEDULED_ACTION, this, m_argv[i]);
#endif
    }
  } else {
    m_argv = NULL;
  }
}


V8ScheduledAction::~V8ScheduledAction() {
  if (!m_func.IsEmpty()) {
#ifndef NDEBUG
    V8Proxy::UnregisterGlobalHandle(this, m_func);
#endif
    m_func.Dispose();

    for (int i = 0; i < m_argc; i++) {
#ifndef NDEBUG
      V8Proxy::UnregisterGlobalHandle(this, m_argv[i]);
#endif
      m_argv[i].Dispose();
    }
    if (m_argc > 0) {
      delete[] m_argv;
    }
  }
}


void V8ScheduledAction::execute(ScriptExecutionContext* script_context) {
  // TODO(ager): Timeouts for running the javascript code are not set.
  V8Proxy* proxy = V8Proxy::retrieve(script_context);
  if (!proxy) return;

  v8::HandleScope handle_scope;
  v8::Local<v8::Context> context = proxy->GetContext();
  if (context.IsEmpty()) return;  // JS may not be enabled.

  v8::Context::Scope scope(context);

  proxy->setTimerCallback(true);

  if (!m_func.IsEmpty() && m_func->IsFunction()) {
    proxy->CallFunction(v8::Persistent<v8::Function>::Cast(m_func),
                        context->Global(), m_argc, m_argv);
  } else {
    proxy->Evaluate(m_code.url(), m_code.startLine() - 1, m_code.source(), 0);
  }

  if (script_context->isDocument()) {
    Document* doc = static_cast<Document*>(script_context);
    doc->updateRendering();
  }

  proxy->setTimerCallback(false);
}


CALLBACK_FUNC_DECL(DOMParserConstructor) {
  INC_STATS("DOM.DOMParser.Contructor");
  return V8Proxy::ConstructDOMObject<V8ClassIndex::DOMPARSER,
                                     DOMParser>(args);
}

CALLBACK_FUNC_DECL(MessageChannelConstructor) {
  INC_STATS("DOM.MessageChannel.Constructor");
  if (!args.IsConstructCall()) {
    V8Proxy::ThrowError(V8Proxy::TYPE_ERROR,
        "DOM object constructor cannot be called as a function.");
    return v8::Undefined();
  }

  // Get the document.
  Frame* frame = V8Proxy::retrieveFrame();
  if (!frame)
    return v8::Undefined();
  Document* document = frame->document();

  // Note: it's OK to let this RefPtr go out of scope because we also call
  // SetDOMWrapper(), which effectively holds a reference to obj.
  RefPtr<MessageChannel> obj = MessageChannel::create(document);
  
  // Create wrappers for the two associated MessagePorts.
  v8::Handle<v8::Value> port1_wrapper =
    V8Proxy::ToV8Object(V8ClassIndex::MESSAGEPORT, obj->port1());
  v8::Handle<v8::Value> port2_wrapper =
    V8Proxy::ToV8Object(V8ClassIndex::MESSAGEPORT, obj->port2());
  
  v8::Handle<v8::Object> wrapper_object = args.Holder();

  // Setup the standard wrapper object internal fields.
  V8Proxy::SetDOMWrapper(
      wrapper_object, V8ClassIndex::MESSAGECHANNEL, obj.get());

  obj->ref();
  V8Proxy::SetJSWrapperForDOMObject(
      obj.get(), v8::Persistent<v8::Object>::New(wrapper_object));

  // Create references from the MessageChannel wrapper to the two
  // MessagePort wrappers to make sure that the MessagePort wrappers
  // stay alive as long as the MessageChannel wrapper is around.
  wrapper_object->SetInternalField(kMessageChannelPort1Index, port1_wrapper);
  wrapper_object->SetInternalField(kMessageChannelPort2Index, port2_wrapper);

  // Return the wrapper object which will be the result of the call to
  // new.
  return wrapper_object;
}


CALLBACK_FUNC_DECL(WebKitCSSMatrixConstructor) {
  INC_STATS("DOM.WebKitCSSMatrix.Constructor");
  String s;
  if (args.Length() >= 1)
    s = ToWebCoreString(args[0]);
  
  // Create the matrix.
  ExceptionCode ec = 0;
  RefPtr<WebKitCSSMatrix> matrix = WebKitCSSMatrix::create(s, ec);
  if (ec != 0) {
    V8Proxy::SetDOMException(ec);
    return v8::Undefined();
  }

  // Transform the holder into a wrapper object for the matrix.
  V8Proxy::SetDOMWrapper(args.Holder(),
                         V8ClassIndex::ToInt(V8ClassIndex::WEBKITCSSMATRIX),
                         matrix.get());
  // Add the wrapper to the DOM object map.
  matrix->ref();
  V8Proxy::SetJSWrapperForDOMObject(
      matrix.get(),
      v8::Persistent<v8::Object>::New(args.Holder()));
  return args.Holder();
}

CALLBACK_FUNC_DECL(WebKitPointConstructor) {
  INC_STATS("DOM.WebKitPoint.Constructor");
  return V8Proxy::ConstructDOMObject<V8ClassIndex::WEBKITPOINT,
                                     WebKitPoint>(args);
}

CALLBACK_FUNC_DECL(XMLSerializerConstructor) {
  INC_STATS("DOM.XMLSerializer.Constructor");
  return V8Proxy::ConstructDOMObject<V8ClassIndex::XMLSERIALIZER,
                                     XMLSerializer>(args);
}


CALLBACK_FUNC_DECL(XPathEvaluatorConstructor) {
  INC_STATS("DOM.XPathEvaluator.Constructor");
  return V8Proxy::ConstructDOMObject<V8ClassIndex::XPATHEVALUATOR,
                                     XPathEvaluator>(args);
}


CALLBACK_FUNC_DECL(XSLTProcessorConstructor) {
  INC_STATS("DOM.XSLTProcessor.Constructor");
  return V8Proxy::ConstructDOMObject<V8ClassIndex::XSLTPROCESSOR,
                                     XSLTProcessor>(args);
}


CALLBACK_FUNC_DECL(XSLTProcessorImportStylesheet) {
  INC_STATS("DOM.XSLTProcessor.importStylesheet");
  // Return undefined if argument does not have the correct type.
  if (!V8Node::HasInstance(args[0]))
    return v8::Undefined();

  XSLTProcessor* imp = V8Proxy::ToNativeObject<XSLTProcessor>(
      V8ClassIndex::XSLTPROCESSOR, args.Holder());

  Node* node = V8Proxy::DOMWrapperToNode<Node>(args[0]);
  imp->importStylesheet(node);
  return v8::Undefined();
}


CALLBACK_FUNC_DECL(XSLTProcessorTransformToFragment) {
  INC_STATS("DOM.XSLTProcessor.transformToFragment");
  // Return undefined if arguments do not have correct types.
  if (!V8Node::HasInstance(args[0]) || !V8Document::HasInstance(args[1]))
    return v8::Undefined();

  XSLTProcessor* imp = V8Proxy::ToNativeObject<XSLTProcessor>(
      V8ClassIndex::XSLTPROCESSOR, args.Holder());

  Node* source = V8Proxy::DOMWrapperToNode<Node>(args[0]);
  Document* owner =
      V8Proxy::DOMWrapperToNode<Document>(args[1]);
  RefPtr<DocumentFragment> result = imp->transformToFragment(source, owner);
  return V8Proxy::NodeToV8Object(result.get());
}


CALLBACK_FUNC_DECL(XSLTProcessorTransformToDocument) {
  INC_STATS("DOM.XSLTProcessor.transformToDocument");
  // Return undefined if argument does not have the correct type.
  if (!V8Node::HasInstance(args[0]))
    return v8::Undefined();

  XSLTProcessor* imp = V8Proxy::ToNativeObject<XSLTProcessor>(
      V8ClassIndex::XSLTPROCESSOR, args.Holder());

  Node* source = V8Proxy::DOMWrapperToNode<Node>(args[0]);
  if (!source)
      return v8::Undefined();
  RefPtr<Document> result = imp->transformToDocument(source);
  // Return undefined if no result was found.
  if (!result)
      return v8::Undefined();
  return V8Proxy::NodeToV8Object(result.get());
}


CALLBACK_FUNC_DECL(XSLTProcessorSetParameter) {
  INC_STATS("DOM.XSLTProcessor.setParameter");
  // Bail out if localName or value is null or undefined.
  if (args[1]->IsNull() || args[1]->IsUndefined() ||
      args[2]->IsNull() || args[2]->IsUndefined()) {
    return v8::Undefined();
  }

  XSLTProcessor* imp = V8Proxy::ToNativeObject<XSLTProcessor>(
      V8ClassIndex::XSLTPROCESSOR, args.Holder());

  String namespaceURI = ToWebCoreString(args[0]);
  String localName = ToWebCoreString(args[1]);
  String value = ToWebCoreString(args[2]);
  imp->setParameter(namespaceURI, localName, value);
  return v8::Undefined();
}


CALLBACK_FUNC_DECL(XSLTProcessorGetParameter) {
  INC_STATS("DOM.XSLTProcessor.getParameter");
  // Bail out if localName is null or undefined.
  if (args[1]->IsNull() || args[1]->IsUndefined()) {
    return v8::Undefined();
  }

  XSLTProcessor* imp = V8Proxy::ToNativeObject<XSLTProcessor>(
      V8ClassIndex::XSLTPROCESSOR, args.Holder());

  String namespaceURI = ToWebCoreString(args[0]);
  String localName = ToWebCoreString(args[1]);
  String result = imp->getParameter(namespaceURI, localName);
  // Return undefined if the string is null.
  if (result.isNull()) return v8::Undefined();
  return v8String(result);
}


CALLBACK_FUNC_DECL(XSLTProcessorRemoveParameter) {
  INC_STATS("DOM.XSLTProcessor.removeParameter");
  // Bail out if localName is null or undefined.
  if (args[1]->IsNull() || args[1]->IsUndefined())
    return v8::Undefined();

  XSLTProcessor* imp = V8Proxy::ToNativeObject<XSLTProcessor>(
      V8ClassIndex::XSLTPROCESSOR, args.Holder());

  String namespaceURI = ToWebCoreString(args[0]);
  String localName = ToWebCoreString(args[1]);
  imp->removeParameter(namespaceURI, localName);
  return v8::Undefined();
}


// ---- Canvas support ----
static v8::Handle<v8::Value> CanvasStyleToV8Object(CanvasStyle* style) {
  if (style->canvasGradient()) {
    return V8Proxy::ToV8Object(V8ClassIndex::CANVASGRADIENT,
                               style->canvasGradient());
  }
  if (style->canvasPattern()) {
    return V8Proxy::ToV8Object(V8ClassIndex::CANVASPATTERN,
                               style->canvasPattern());
  }
  return v8String(style->color());
}

static PassRefPtr<CanvasStyle> V8ObjectToCanvasStyle(v8::Handle<v8::Value> value)
{
  if (value->IsString())
      return CanvasStyle::create(ToWebCoreString(value));

  if (V8CanvasGradient::HasInstance(value)) {
    CanvasGradient* gradient =
        V8Proxy::DOMWrapperToNative<CanvasGradient>(value);
    return CanvasStyle::create(gradient);
  }

  if (V8CanvasPattern::HasInstance(value)) {
    CanvasPattern* pattern =
        V8Proxy::DOMWrapperToNative<CanvasPattern>(value);
    return CanvasStyle::create(pattern);
  }

  return 0;
}


ACCESSOR_GETTER(CanvasRenderingContext2DStrokeStyle) {
  CanvasRenderingContext2D* impl =
    V8Proxy::DOMWrapperToNative<CanvasRenderingContext2D>(info.Holder());
  CanvasStyle* strokeStyle = impl->strokeStyle();
  return CanvasStyleToV8Object(strokeStyle);
}


ACCESSOR_SETTER(CanvasRenderingContext2DStrokeStyle) {
  CanvasRenderingContext2D* impl =
    V8Proxy::DOMWrapperToNative<CanvasRenderingContext2D>(info.Holder());
  impl->setStrokeStyle(V8ObjectToCanvasStyle(value));
}

ACCESSOR_GETTER(CanvasRenderingContext2DFillStyle) {
  CanvasRenderingContext2D* impl =
    V8Proxy::DOMWrapperToNative<CanvasRenderingContext2D>(info.Holder());
  CanvasStyle* fillStyle = impl->fillStyle();
  return CanvasStyleToV8Object(fillStyle);
}


ACCESSOR_SETTER(CanvasRenderingContext2DFillStyle) {
  CanvasRenderingContext2D* impl =
    V8Proxy::DOMWrapperToNative<CanvasRenderingContext2D>(info.Holder());
  impl->setFillStyle(V8ObjectToCanvasStyle(value));
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


ACCESSOR_GETTER(EventSrcElement) {
  Event* event = V8Proxy::DOMWrapperToNative<Event>(info.Holder());
  EventTarget* target = event->target();
  return V8Proxy::EventTargetToV8Object(target);
}


ACCESSOR_GETTER(EventReturnValue) {
  Event* event = V8Proxy::DOMWrapperToNative<Event>(info.Holder());
  return event->defaultPrevented() ? v8::False() : v8::True();
}


ACCESSOR_SETTER(EventReturnValue) {
  Event* event = V8Proxy::DOMWrapperToNative<Event>(info.Holder());
  bool v = value->BooleanValue();
  event->setDefaultPrevented(!v);
}


ACCESSOR_GETTER(EventDataTransfer) {
  Event* event = V8Proxy::DOMWrapperToNative<Event>(info.Holder());

  if (event->isDragEvent()) {
    MouseEvent* impl = static_cast<MouseEvent*>(event);
    Clipboard* clipboard = impl->clipboard();
    return V8Proxy::ToV8Object(V8ClassIndex::CLIPBOARD, clipboard);
  }

  return v8::Undefined();
}


ACCESSOR_GETTER(EventClipboardData) {
  Event* event = V8Proxy::DOMWrapperToNative<Event>(info.Holder());

  if (event->isClipboardEvent()) {
    ClipboardEvent* impl = static_cast<ClipboardEvent*>(event);
    Clipboard* clipboard = impl->clipboard();
    return V8Proxy::ToV8Object(V8ClassIndex::CLIPBOARD, clipboard);
  }

  return v8::Undefined();
}


INDEXED_PROPERTY_GETTER(DOMStringList) {
  INC_STATS("DOM.DOMStringList.IndexedPropertyGetter");
  DOMStringList* imp =
    V8Proxy::DOMWrapperToNative<DOMStringList>(info.Holder());
  return v8String(imp->item(index));
}


CALLBACK_FUNC_DECL(DOMStringListItem) {
  INC_STATS("DOM.DOMStringListItem()");
  if (args.Length() == 0)
    return v8::Null();
  uint32_t index = args[0]->Uint32Value();

  DOMStringList* imp =
    V8Proxy::DOMWrapperToNative<DOMStringList>(args.Holder());
  if (index >= imp->length())
    return v8::Null();

  return v8String(imp->item(index));
}


NAMED_PROPERTY_DELETER(HTMLDocument) {
  // Only handle document.all.  Insert the marker object into the
  // shadow internal field to signal that document.all is no longer
  // shadowed.
  String key = ToWebCoreString(name);
  if (key == "all") {
    ASSERT(info.Holder()->InternalFieldCount() ==
           kHTMLDocumentInternalFieldCount);
    v8::Local<v8::Value> marker =
        info.Holder()->GetInternalField(kHTMLDocumentMarkerIndex);
    info.Holder()->SetInternalField(kHTMLDocumentShadowIndex, marker);
    return v8::True();
  }
  return v8::Handle<v8::Boolean>();
}


NAMED_PROPERTY_SETTER(HTMLDocument)
{
  INC_STATS("DOM.HTMLDocument.NamedPropertySetter");
  // Only handle document.all.  We insert the value into the shadow
  // internal field from which the getter will retrieve it.
  String key = ToWebCoreString(name);
  if (key == "all") {
    ASSERT(info.Holder()->InternalFieldCount() ==
           kHTMLDocumentInternalFieldCount);
    info.Holder()->SetInternalField(kHTMLDocumentShadowIndex, value);
  }
  return v8::Handle<v8::Value>();
}


NAMED_PROPERTY_GETTER(HTMLDocument)
{
    INC_STATS("DOM.HTMLDocument.NamedPropertyGetter");
    AtomicString key = ToWebCoreString(name);

    // Special case for document.all.  If the value in the shadow
    // internal field is not the marker object, then document.all has
    // been temporarily shadowed and we return the value.
    if (key == "all") {
        ASSERT(info.Holder()->InternalFieldCount() == kHTMLDocumentInternalFieldCount);
        v8::Local<v8::Value> marker = info.Holder()->GetInternalField(kHTMLDocumentMarkerIndex);
        v8::Local<v8::Value> value = info.Holder()->GetInternalField(kHTMLDocumentShadowIndex);
        if (marker != value)
            return value;
    }

    HTMLDocument* imp = V8Proxy::DOMWrapperToNode<HTMLDocument>(info.Holder());

    // Fast case for named elements that are not there.
    if (!imp->hasNamedItem(key.impl()) && !imp->hasExtraNamedItem(key.impl()))
        return v8::Handle<v8::Value>();

    RefPtr<HTMLCollection> items = imp->documentNamedItems(key);
    if (items->length() == 0)
        return v8::Handle<v8::Value>();
    if (items->length() == 1) {
        Node* node = items->firstItem();
        Frame* frame = 0;
        if (node->hasTagName(HTMLNames::iframeTag) &&
           (frame = static_cast<HTMLIFrameElement*>(node)->contentFrame()))
            return V8Proxy::ToV8Object(V8ClassIndex::DOMWINDOW, frame->domWindow());
        return V8Proxy::NodeToV8Object(node);
    }
    return V8Proxy::ToV8Object(V8ClassIndex::HTMLCOLLECTION, items.get());
}


NAMED_PROPERTY_GETTER(HTMLFrameSetElement)
{
  INC_STATS("DOM.HTMLFrameSetElement.NamedPropertyGetter");
  HTMLFrameSetElement* imp =
      V8Proxy::DOMWrapperToNode<HTMLFrameSetElement>(info.Holder());
  String key = ToWebCoreString(name);
  Node* frame = imp->children()->namedItem(key);
  if (frame && frame->hasTagName(HTMLNames::frameTag)) {
    Document* doc = static_cast<HTMLFrameElement*>(frame)->contentDocument();
    if (doc) {
      Frame* content_frame = doc->frame();
      if (content_frame)
        return V8Proxy::ToV8Object(V8ClassIndex::DOMWINDOW, content_frame->domWindow());
    }
    return v8::Undefined();
  }
  return v8::Handle<v8::Value>();
}


INDEXED_PROPERTY_GETTER(NamedNodeMap) {
  INC_STATS("DOM.NamedNodeMap.IndexedPropertyGetter");
  NamedNodeMap* imp = V8Proxy::ToNativeObject<NamedNodeMap>(
      V8ClassIndex::NAMEDNODEMAP, info.Holder());
  RefPtr<Node> result = imp->item(index);
  if (!result) return v8::Handle<v8::Value>();

  return V8Proxy::NodeToV8Object(result.get());
}

NAMED_PROPERTY_GETTER(NamedNodeMap) {
  INC_STATS("DOM.NamedNodeMap.NamedPropertyGetter");
  // Search the prototype chain first.
  v8::Handle<v8::Value> value =
      info.Holder()->GetRealNamedPropertyInPrototypeChain(name);
  if (!value.IsEmpty())
    return value;

  // Then look for IDL defined properties on the object itself.
  if (info.Holder()->HasRealNamedCallbackProperty(name))
    return v8::Handle<v8::Value>();

  // Finally, search the DOM.
  NamedNodeMap* imp = V8Proxy::ToNativeObject<NamedNodeMap>(
      V8ClassIndex::NAMEDNODEMAP, info.Holder());
  String prop_name = ToWebCoreString(name);
  RefPtr<Node> result = imp->getNamedItem(prop_name);
  if (!result) return v8::Handle<v8::Value>();

  return V8Proxy::NodeToV8Object(result.get());
}


NAMED_PROPERTY_GETTER(NodeList) {
  INC_STATS("DOM.NodeList.NamedPropertyGetter");
  NodeList* list = V8Proxy::ToNativeObject<NodeList>(
      V8ClassIndex::NODELIST, info.Holder());
  String prop_name = ToWebCoreString(name);

  // Length property cannot be overridden.
  if (prop_name == "length")
    return v8::Number::New(list->length());

  RefPtr<Node> result = list->itemWithName(prop_name);
  if (result)
    return V8Proxy::NodeToV8Object(result.get());

  return v8::Handle<v8::Value>();
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

// Check for a CSS prefix.
// Passed prefix is all lowercase.
// First character of the prefix within the property name may be upper or lowercase.
// Other characters in the prefix within the property name must be lowercase.
// The prefix within the property name must be followed by a capital letter.
static bool hasCSSPropertyNamePrefix(const String& propertyName, const char* prefix)
{
#ifndef NDEBUG
    ASSERT(*prefix);
    for (const char* p = prefix; *p; ++p)
        ASSERT(WTF::isASCIILower(*p));
    ASSERT(propertyName.length());
#endif

    if (WTF::toASCIILower(propertyName[0]) != prefix[0])
        return false;

    unsigned length = propertyName.length();
    for (unsigned i = 1; i < length; ++i) {
        if (!prefix[i])
            return WTF::isASCIIUpper(propertyName[i]);
        if (propertyName[i] != prefix[i])
            return false;
    }
    return false;
}

// When getting properties on CSSStyleDeclarations, the name used from
// Javascript and the actual name of the property are not the same, so
// we have to do the following translation.  The translation turns upper
// case characters into lower case characters and inserts dashes to
// separate words.
//
// Example: 'backgroundPositionY' -> 'background-position-y'
//
// Also, certain prefixes such as 'pos', 'css-' and 'pixel-' are stripped
// and the pixel_or_pos_prefix out parameter is used to indicate whether or
// not the property name was prefixed with 'pos-' or 'pixel-'.
static String cssPropertyName(const String& propertyName, bool* hadPixelOrPosPrefix = 0)
{
    if (hadPixelOrPosPrefix)
        *hadPixelOrPosPrefix = false;

    unsigned length = propertyName.length();
    if (!length)
        return String();

    Vector<UChar> name;
    name.reserveCapacity(length);

    unsigned i = 0;

    if (hasCSSPropertyNamePrefix(propertyName, "css"))
        i += 3;
    else if (hasCSSPropertyNamePrefix(propertyName, "pixel")) {
        i += 5;
        if (hadPixelOrPosPrefix)
            *hadPixelOrPosPrefix = true;
    } else if (hasCSSPropertyNamePrefix(propertyName, "pos")) {
        i += 3;
        if (hadPixelOrPosPrefix)
            *hadPixelOrPosPrefix = true;
    } else if (hasCSSPropertyNamePrefix(propertyName, "webkit")
            || hasCSSPropertyNamePrefix(propertyName, "khtml")
            || hasCSSPropertyNamePrefix(propertyName, "apple"))
        name.append('-');
    else {
        if (WTF::isASCIIUpper(propertyName[0]))
            return String();
    }

    name.append(WTF::toASCIILower(propertyName[i++]));

    for (; i < length; ++i) {
        UChar c = propertyName[i];
        if (!WTF::isASCIIUpper(c))
            name.append(c);
        else {
            name.append('-');
            name.append(WTF::toASCIILower(c));
        }
    }

    return String::adopt(name);
}

NAMED_PROPERTY_GETTER(CSSStyleDeclaration) {
  INC_STATS("DOM.CSSStyleDeclaration.NamedPropertyGetter");
  // First look for API defined attributes on the style declaration
  // object.
  if (info.Holder()->HasRealNamedCallbackProperty(name))
    return v8::Handle<v8::Value>();

  // Search the style declaration.
  CSSStyleDeclaration* imp = V8Proxy::ToNativeObject<CSSStyleDeclaration>(
      V8ClassIndex::CSSSTYLEDECLARATION, info.Holder());

  bool pixel_or_pos;
  String p = ToWebCoreString(name);
  String prop = cssPropertyName(p, &pixel_or_pos);

  // Do not handle non-property names.
  if (!CSSStyleDeclaration::isPropertyName(prop)) {
    return v8::Handle<v8::Value>();
  }

  RefPtr<CSSValue> v = imp->getPropertyCSSValue(prop);
  if (v) {
    if (pixel_or_pos && v->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE) {
      RefPtr<CSSPrimitiveValue> primitive_value =
          static_pointer_cast<CSSPrimitiveValue>(v);
      return v8::Number::New(
          primitive_value->getFloatValue(CSSPrimitiveValue::CSS_PX));
    }
    return v8StringOrNull(v->cssText());
  }

  String result = imp->getPropertyValue(prop);
  if (result.isNull())
    result = "";  // convert null to empty string.

  // The 'filter' attribute is made undetectable in KJS/WebKit
  // to avoid confusion with IE's filter extension.
  if (prop == "filter") {
    return v8UndetectableString(result);
  }
  return v8String(result);
}


NAMED_PROPERTY_SETTER(CSSStyleDeclaration) {
  INC_STATS("DOM.CSSStyleDeclaration.NamedPropertySetter");
  CSSStyleDeclaration* imp = V8Proxy::ToNativeObject<CSSStyleDeclaration>(
      V8ClassIndex::CSSSTYLEDECLARATION, info.Holder());
  String property_name = ToWebCoreString(name);
  int ec = 0;

  bool pixel_or_pos;
  String prop = cssPropertyName(property_name, &pixel_or_pos);
  if (!CSSStyleDeclaration::isPropertyName(prop)) {
    return v8::Handle<v8::Value>();  // do not block the call
  }

  String prop_value = valueToStringWithNullCheck(value);
  if (pixel_or_pos) prop_value += "px";
  imp->setProperty(prop, prop_value, ec);

  V8Proxy::SetDOMException(ec);
  return value;
}


NAMED_PROPERTY_GETTER(StyleSheetList) {
  INC_STATS("DOM.StyleSheetList.NamedPropertyGetter");
  // Look for local properties first.
  if (info.Holder()->HasRealNamedProperty(name)) {
    return v8::Handle<v8::Value>();
  }

  // Search style sheet.
  StyleSheetList* imp = V8Proxy::ToNativeObject<StyleSheetList>(
      V8ClassIndex::STYLESHEETLIST, info.Holder());
  String key = ToWebCoreString(name);
  HTMLStyleElement* item = imp->getNamedItem(key);
  if (item) {
    return V8Proxy::ToV8Object(V8ClassIndex::HTMLSTYLEELEMENT, item);
  }
  return v8::Handle<v8::Value>();
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


// Clipboard -------------------------------------------------------------------


ACCESSOR_GETTER(ClipboardTypes) {
  INC_STATS("DOM.Clipboard.types()");
  Clipboard* imp =
    V8Proxy::ToNativeObject<Clipboard>(V8ClassIndex::CLIPBOARD,
                                       info.Holder());

  HashSet<String> types = imp->types();
  if (types.isEmpty())
    return v8::Null();

  v8::Local<v8::Array> result = v8::Array::New(types.size());
  HashSet<String>::const_iterator end = types.end();
  int index = 0;
  for (HashSet<String>::const_iterator it = types.begin();
       it != end;
       ++it, ++index) {
    result->Set(v8::Integer::New(index), v8String(*it));
  }
  return result;
}


CALLBACK_FUNC_DECL(ClipboardClearData) {
  INC_STATS("DOM.Clipboard.clearData()");
  Clipboard* imp = V8Proxy::ToNativeObject<Clipboard>(
      V8ClassIndex::CLIPBOARD, args.Holder());

  if (args.Length() == 0) {
    imp->clearAllData();
    return v8::Undefined();
  } 

  if (args.Length() == 1) {
    String v = ToWebCoreString(args[0]);
    imp->clearData(v);
    return v8::Undefined();
  }
  
  V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR,
                      "clearData: Invalid number of arguments");
  return v8::Undefined();
}


CALLBACK_FUNC_DECL(ClipboardGetData) {
  INC_STATS("DOM.Clipboard.getData()");
  Clipboard* imp = V8Proxy::ToNativeObject<Clipboard>(
      V8ClassIndex::CLIPBOARD, args.Holder());
  
  if (args.Length() != 1) {
    V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR,
                        "getData: Invalid number of arguments");
    return v8::Undefined();
  }

  bool success;
  String v = ToWebCoreString(args[0]);
  String result = imp->getData(v, success);
  if (success) return v8String(result);
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(ClipboardSetData) {
  INC_STATS("DOM.Clipboard.setData()");
  Clipboard* imp = V8Proxy::ToNativeObject<Clipboard>(
      V8ClassIndex::CLIPBOARD, args.Holder());

  if (args.Length() != 2) {
    V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR,
                        "setData: Invalid number of arguments");
    return v8::Undefined();
  }

  String type = ToWebCoreString(args[0]);
  String data = ToWebCoreString(args[1]);
  bool result = imp->setData(type, data);
  return result ? v8::True() : v8::False();
}


CALLBACK_FUNC_DECL(ClipboardSetDragImage) {
  INC_STATS("DOM.Clipboard.setDragImage()");
  Clipboard* imp = V8Proxy::ToNativeObject<Clipboard>(
      V8ClassIndex::CLIPBOARD, args.Holder());
  
  if (!imp->isForDragging())
    return v8::Undefined();

  if (args.Length() != 3) {
    V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR,
                        "setDragImage: Invalid number of arguments");
    return v8::Undefined();
  }

  int x = ToInt32(args[1]);
  int y = ToInt32(args[2]);

  Node* node = 0;
  if (V8Node::HasInstance(args[0]))
    node = V8Proxy::DOMWrapperToNode<Node>(args[0]);
  if (!node) {
    V8Proxy::ThrowError(V8Proxy::TYPE_ERROR,
                        "setDragImageFromElement: Invalid first argument");
    return v8::Undefined();
  }

  if (!node->isElementNode()) {
    V8Proxy::ThrowError(V8Proxy::SYNTAX_ERROR,
                        "setDragImageFromElement: Invalid first argument");
    return v8::Undefined();
  }
   
  if (static_cast<Element*>(node)->hasLocalName(HTMLNames::imgTag) &&
      !node->inDocument())
    imp->setDragImage(static_cast<HTMLImageElement*>(node)->cachedImage(),
                      IntPoint(x, y));
  else
    imp->setDragImageElement(node, IntPoint(x, y));

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
                           new V8ScheduledAction(string_function), timeout,
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
    ScheduledAction* action = new V8ScheduledAction(
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

// Document --------------------------------------------------------------------

CALLBACK_FUNC_DECL(DocumentEvaluate) {
  INC_STATS("DOM.Document.evaluate()");

  Document* imp = V8Proxy::DOMWrapperToNode<Document>(args.Holder());
  ExceptionCode ec = 0;
  String expression = ToWebCoreString(args[0]);
  Node* contextNode = NULL;
  if (V8Node::HasInstance(args[1])) {
    contextNode = V8Proxy::DOMWrapperToNode<Node>(args[1]);
  }
  // Find the XPath
  XPathNSResolver* resolver = NULL;
  if (V8XPathNSResolver::HasInstance(args[2])) {
    resolver = V8Proxy::ToNativeObject<XPathNSResolver>(
        V8ClassIndex::XPATHNSRESOLVER, args[2]);
  } else if (args[2]->IsObject()) {
    v8::Handle<v8::Object> obj = args[2]->ToObject();
    resolver = new JSXPathNSResolver(obj);
  } else if (!args[2]->IsNull() && !args[2]->IsUndefined()) {
    V8Proxy::SetDOMException(TYPE_MISMATCH_ERR);
    return v8::Handle<v8::Value>();
  }
  int type = ToInt32(args[3]);
  XPathResult* inResult = NULL;
  if (V8XPathResult::HasInstance(args[4])) {
    inResult = V8Proxy::ToNativeObject<XPathResult>(
        V8ClassIndex::XPATHRESULT, args[4]);
  }

  v8::TryCatch try_catch;
  RefPtr<XPathResult> result =
      imp->evaluate(expression, contextNode, resolver, type, inResult, ec);
  if (try_catch.HasCaught() || ec != 0) {
    if (!try_catch.HasCaught())
      V8Proxy::SetDOMException(ec);
    return v8::Handle<v8::Value>();
  }
  return V8Proxy::ToV8Object(V8ClassIndex::XPATHRESULT, result.get());
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


// Node -------------------------------------------------------------

CALLBACK_FUNC_DECL(NodeAddEventListener) {
  INC_STATS("DOM.Node.addEventListener()");
  Node* node = V8Proxy::DOMWrapperToNode<Node>(args.Holder());

  V8Proxy* proxy = V8Proxy::retrieve(node->document()->frame());
  if (!proxy)
    return v8::Undefined();

  RefPtr<EventListener> listener =
    proxy->FindOrCreateV8EventListener(args[1], false);
  if (listener) {
    String type = ToWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();
    node->addEventListener(type, listener, useCapture);
  }
  return v8::Undefined();
}

CALLBACK_FUNC_DECL(NodeRemoveEventListener) {
  INC_STATS("DOM.Node.removeEventListener()");
  Node* node = V8Proxy::DOMWrapperToNode<Node>(args.Holder());

  V8Proxy* proxy = V8Proxy::retrieve(node->document()->frame());
  // It is possbile that the owner document of the node is detached
  // from the frame, return immediately in this case.
  // See issue 878909
  if (!proxy)
    return v8::Undefined();

  RefPtr<EventListener> listener =
    proxy->FindV8EventListener(args[1], false);
  if (listener) {
    String type = ToWebCoreString(args[0]);
    bool useCapture = args[2]->BooleanValue();
    node->removeEventListener(type, listener.get(), useCapture);
  }

  return v8::Undefined();
}


// Navigator ------------------------------------------------------------------
ACCESSOR_GETTER(NavigatorAppVersion) {
  INC_STATS("DOM.Navigator.appVersion");
  v8::Handle<v8::Object> holder = info.Holder();
  Navigator* imp = V8Proxy::ToNativeObject<Navigator>(V8ClassIndex::NAVIGATOR,
                                                      holder);
  String v = ToString(imp->appVersion());
  return v8StringOrUndefined(v);
}


// TreeWalker ------------------------------------------------------------------

CALLBACK_FUNC_DECL(TreeWalkerParentNode) {
  INC_STATS("DOM.TreeWalker.parentNode()");
  TreeWalker* treeWalker = V8Proxy::ToNativeObject<TreeWalker>(
      V8ClassIndex::TREEWALKER, args.Holder());

  ScriptState state;
  RefPtr<Node> result = treeWalker->parentNode(&state);
  if (state.hadException()) {
    v8::ThrowException(state.exception());
    return v8::Undefined();
  }
  if (!result) return v8::Null();
  return V8Proxy::NodeToV8Object(result.get());
}

CALLBACK_FUNC_DECL(TreeWalkerFirstChild) {
  INC_STATS("DOM.TreeWalker.firstChild()");
  TreeWalker* treeWalker = V8Proxy::ToNativeObject<TreeWalker>(
      V8ClassIndex::TREEWALKER, args.Holder());

  ScriptState state;
  RefPtr<Node> result = treeWalker->firstChild(&state);
  if (state.hadException()) {
    v8::ThrowException(state.exception());
    return v8::Undefined();
  }
  if (!result) return v8::Null();
  return V8Proxy::NodeToV8Object(result.get());
}

CALLBACK_FUNC_DECL(TreeWalkerLastChild) {
  INC_STATS("DOM.TreeWalker.lastChild()");
  TreeWalker* treeWalker = V8Proxy::ToNativeObject<TreeWalker>(
      V8ClassIndex::TREEWALKER, args.Holder());

  ScriptState state;
  RefPtr<Node> result = treeWalker->lastChild(&state);
  if (state.hadException()) {
    v8::ThrowException(state.exception());
    return v8::Undefined();
  }
  if (!result) return v8::Null();
  return V8Proxy::NodeToV8Object(result.get());
}

CALLBACK_FUNC_DECL(TreeWalkerNextNode) {
  INC_STATS("DOM.TreeWalker.nextNode()");
  TreeWalker* treeWalker = V8Proxy::ToNativeObject<TreeWalker>(
      V8ClassIndex::TREEWALKER, args.Holder());

  ScriptState state;
  RefPtr<Node> result = treeWalker->nextNode(&state);
  if (state.hadException()) {
    v8::ThrowException(state.exception());
    return v8::Undefined();
  }
  if (!result) return v8::Null();
  return V8Proxy::NodeToV8Object(result.get());
}

CALLBACK_FUNC_DECL(TreeWalkerPreviousNode) {
  INC_STATS("DOM.TreeWalker.previousNode()");
  TreeWalker* treeWalker = V8Proxy::ToNativeObject<TreeWalker>(
      V8ClassIndex::TREEWALKER, args.Holder());

  ScriptState state;
  RefPtr<Node> result = treeWalker->previousNode(&state);
  if (state.hadException()) {
    v8::ThrowException(state.exception());
    return v8::Undefined();
  }
  if (!result) return v8::Null();
  return V8Proxy::NodeToV8Object(result.get());
}

CALLBACK_FUNC_DECL(TreeWalkerNextSibling) {
  INC_STATS("DOM.TreeWalker.nextSibling()");
  TreeWalker* treeWalker = V8Proxy::ToNativeObject<TreeWalker>(
      V8ClassIndex::TREEWALKER, args.Holder());

  ScriptState state;
  RefPtr<Node> result = treeWalker->nextSibling(&state);
  if (state.hadException()) {
    v8::ThrowException(state.exception());
    return v8::Undefined();
  }
  if (!result) return v8::Null();
  return V8Proxy::NodeToV8Object(result.get());
}

CALLBACK_FUNC_DECL(TreeWalkerPreviousSibling) {
  INC_STATS("DOM.TreeWalker.previousSibling()");
  TreeWalker* treeWalker = V8Proxy::ToNativeObject<TreeWalker>(
      V8ClassIndex::TREEWALKER, args.Holder());

  ScriptState state;
  RefPtr<Node> result = treeWalker->previousSibling(&state);
  if (state.hadException()) {
    v8::ThrowException(state.exception());
    return v8::Undefined();
  }
  if (!result) return v8::Null();
  return V8Proxy::NodeToV8Object(result.get());
}

CALLBACK_FUNC_DECL(NodeIteratorNextNode) {
  INC_STATS("DOM.NodeIterator.nextNode()");
  NodeIterator* nodeIterator = V8Proxy::ToNativeObject<NodeIterator>(
      V8ClassIndex::NODEITERATOR, args.Holder());

  ExceptionCode ec = 0;
  ScriptState state;
  RefPtr<Node> result = nodeIterator->nextNode(&state, ec);
  if (ec != 0) {
      V8Proxy::SetDOMException(ec);
      return v8::Null();
  }
  if (state.hadException()) {
    v8::ThrowException(state.exception());
    return v8::Undefined();
  }
  if (!result) return v8::Null();
  return V8Proxy::NodeToV8Object(result.get());
}

CALLBACK_FUNC_DECL(NodeIteratorPreviousNode) {
  INC_STATS("DOM.NodeIterator.previousNode()");
  NodeIterator* nodeIterator = V8Proxy::ToNativeObject<NodeIterator>(
      V8ClassIndex::NODEITERATOR, args.Holder());

  ExceptionCode ec = 0;
  ScriptState state;
  RefPtr<Node> result = nodeIterator->previousNode(&state, ec);
  if (ec != 0) {
      V8Proxy::SetDOMException(ec);
      return v8::Null();
  }
  if (state.hadException()) {
    v8::ThrowException(state.exception());
    return v8::Undefined();
  }
  if (!result) return v8::Null();
  return V8Proxy::NodeToV8Object(result.get());
}

CALLBACK_FUNC_DECL(NodeFilterAcceptNode) {
  INC_STATS("DOM.NodeFilter.acceptNode()");
  V8Proxy::SetDOMException(NOT_SUPPORTED_ERR);  
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
