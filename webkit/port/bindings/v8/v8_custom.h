// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CUSTOM_H__
#define V8_CUSTOM_H__

#include <v8.h>
#include "v8_index.h"

struct NPObject;

#define CALLBACK_FUNC_DECL(NAME)                \
v8::Handle<v8::Value> V8Custom::v8##NAME##Callback(const v8::Arguments& args)

#define ACCESSOR_GETTER(NAME) \
v8::Handle<v8::Value> V8Custom::v8##NAME##AccessorGetter(\
    v8::Local<v8::String> name, const v8::AccessorInfo& info)

#define ACCESSOR_SETTER(NAME) \
void V8Custom::v8##NAME##AccessorSetter(v8::Local<v8::String> name, \
                                        v8::Local<v8::Value> value, \
                                        const v8::AccessorInfo& info)

#define INDEXED_PROPERTY_GETTER(NAME)  \
v8::Handle<v8::Value> V8Custom::v8##NAME##IndexedPropertyGetter(\
    uint32_t index, const v8::AccessorInfo& info)

#define INDEXED_PROPERTY_SETTER(NAME)  \
v8::Handle<v8::Value> V8Custom::v8##NAME##IndexedPropertySetter(\
    uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info)

#define INDEXED_PROPERTY_DELETER(NAME) \
v8::Handle<v8::Boolean> V8Custom::v8##NAME##IndexedPropertyDeleter(\
    uint32_t index, const v8::AccessorInfo& info)

#define NAMED_PROPERTY_GETTER(NAME)  \
    v8::Handle<v8::Value> V8Custom::v8##NAME##NamedPropertyGetter(\
    v8::Local<v8::String> name, const v8::AccessorInfo& info)

#define NAMED_PROPERTY_SETTER(NAME)  \
    v8::Handle<v8::Value> V8Custom::v8##NAME##NamedPropertySetter(\
    v8::Local<v8::String> name, v8::Local<v8::Value> value, \
    const v8::AccessorInfo& info)

#define NAMED_PROPERTY_DELETER(NAME) \
    v8::Handle<v8::Boolean> V8Custom::v8##NAME##NamedPropertyDeleter(\
    v8::Local<v8::String> name, const v8::AccessorInfo& info)

#define NAMED_ACCESS_CHECK(NAME) \
    bool V8Custom::v8##NAME##NamedSecurityCheck(v8::Local<v8::Object> host, \
    v8::Local<v8::Value> key, \
    v8::AccessType type, \
    v8::Local<v8::Value> data)

#define INDEXED_ACCESS_CHECK(NAME) \
    bool V8Custom::v8##NAME##IndexedSecurityCheck(v8::Local<v8::Object> host, \
    uint32_t index, \
    v8::AccessType type, \
    v8::Local<v8::Value> data)

namespace WebCore {

class Frame;
class V8Proxy;
class String;
class HTMLCollection;
class DOMWindow;

class V8Custom {
 public:

  // Constants.
  static const int kDOMWrapperTypeIndex = 0;
  static const int kDOMWrapperObjectIndex = 1;
  static const int kDefaultWrapperInternalFieldCount = 2;

  static const int kNPObjectInternalFieldCount =
                      kDefaultWrapperInternalFieldCount + 0;

  static const int kDocumentImplementationIndex =
                      kDefaultWrapperInternalFieldCount + 0;
  static const int kDocumentMinimumInternalFieldCount =
                      kDefaultWrapperInternalFieldCount + 1;

  static const int kHTMLDocumentMarkerIndex =
                      kDocumentMinimumInternalFieldCount + 0;
  static const int kHTMLDocumentShadowIndex =
                      kDocumentMinimumInternalFieldCount + 1;
  static const int kHTMLDocumentInternalFieldCount =
                      kDocumentMinimumInternalFieldCount + 2;

  static const int kXMLHttpRequestCacheIndex =
                      kDefaultWrapperInternalFieldCount + 0;
  static const int kXMLHttpRequestInternalFieldCount =
                      kDefaultWrapperInternalFieldCount + 1;

  static const int kMessageChannelPort1Index = 
                      kDefaultWrapperInternalFieldCount + 0;
  static const int kMessageChannelPort2Index = 
                      kDefaultWrapperInternalFieldCount + 1;
  static const int kMessageChannelInternalFieldCount =
                      kDefaultWrapperInternalFieldCount + 2;

  static const int kMessagePortRequestCacheIndex =
                      kDefaultWrapperInternalFieldCount + 0;
  static const int kMessagePortEntangledPortIndex =
                      kDefaultWrapperInternalFieldCount + 1;
  static const int kMessagePortInternalFieldCount =
                      kDefaultWrapperInternalFieldCount + 2;

#if ENABLE(WORKERS)
  static const int kWorkerRequestCacheIndex =
                      kDefaultWrapperInternalFieldCount + 0;
  static const int kWorkerInternalFieldCount =
                      kDefaultWrapperInternalFieldCount + 1;

  static const int kWorkerContextRequestCacheIndex =
                      kDefaultWrapperInternalFieldCount + 0;
  static const int kWorkerContextInternalFieldCount =
                      kDefaultWrapperInternalFieldCount + 1;
#endif

  static const int kDOMWindowLocationIndex =
                      kDefaultWrapperInternalFieldCount + 0;
  static const int kDOMWindowNavigatorIndex =
                      kDefaultWrapperInternalFieldCount + 1;
  static const int kDOMWindowInternalFieldCount =
                      kDefaultWrapperInternalFieldCount + 2;

  static const int kStyleSheetOwnerNodeIndex =
                      kDefaultWrapperInternalFieldCount + 0;
  static const int kStyleSheetInternalFieldCount =
                      kDefaultWrapperInternalFieldCount + 1;

#define DECLARE_PROPERTY_ACCESSOR_GETTER(NAME) \
static v8::Handle<v8::Value> v8##NAME##AccessorGetter(\
    v8::Local<v8::String> name, const v8::AccessorInfo& info);

#define DECLARE_PROPERTY_ACCESSOR_SETTER(NAME)  \
static void v8##NAME##AccessorSetter(v8::Local<v8::String> name, \
                                     v8::Local<v8::Value> value, \
                                     const v8::AccessorInfo& info);

#define DECLARE_PROPERTY_ACCESSOR(NAME) \
  DECLARE_PROPERTY_ACCESSOR_GETTER(NAME) \
  DECLARE_PROPERTY_ACCESSOR_SETTER(NAME)


#define DECLARE_NAMED_PROPERTY_GETTER(NAME)  \
static v8::Handle<v8::Value> v8##NAME##NamedPropertyGetter(\
    v8::Local<v8::String> name, const v8::AccessorInfo& info);

#define DECLARE_NAMED_PROPERTY_SETTER(NAME)  \
static v8::Handle<v8::Value> v8##NAME##NamedPropertySetter(\
    v8::Local<v8::String> name, \
    v8::Local<v8::Value> value, \
    const v8::AccessorInfo& info);

#define DECLARE_NAMED_PROPERTY_DELETER(NAME)  \
static v8::Handle<v8::Boolean> v8##NAME##NamedPropertyDeleter(\
    v8::Local<v8::String> name, const v8::AccessorInfo& info);

#define USE_NAMED_PROPERTY_GETTER(NAME) \
  V8Custom::v8##NAME##NamedPropertyGetter

#define USE_NAMED_PROPERTY_SETTER(NAME) \
  V8Custom::v8##NAME##NamedPropertySetter

#define USE_NAMED_PROPERTY_DELETER(NAME) \
  V8Custom::v8##NAME##NamedPropertyDeleter

#define DECLARE_INDEXED_PROPERTY_GETTER(NAME)  \
static v8::Handle<v8::Value> v8##NAME##IndexedPropertyGetter(\
    uint32_t index, const v8::AccessorInfo& info);

#define DECLARE_INDEXED_PROPERTY_SETTER(NAME)  \
static v8::Handle<v8::Value> v8##NAME##IndexedPropertySetter(\
    uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info);

#define DECLARE_INDEXED_PROPERTY_DELETER(NAME)  \
static v8::Handle<v8::Boolean> v8##NAME##IndexedPropertyDeleter(\
    uint32_t index, const v8::AccessorInfo& info);

#define USE_INDEXED_PROPERTY_GETTER(NAME) \
  V8Custom::v8##NAME##IndexedPropertyGetter

#define USE_INDEXED_PROPERTY_SETTER(NAME) \
  V8Custom::v8##NAME##IndexedPropertySetter

#define USE_INDEXED_PROPERTY_DELETER(NAME) \
  V8Custom::v8##NAME##IndexedPropertyDeleter

#define DECLARE_CALLBACK(NAME)                \
static v8::Handle<v8::Value> v8##NAME##Callback(const v8::Arguments& args);

#define USE_CALLBACK(NAME) \
  V8Custom::v8##NAME##Callback

#define DECLARE_NAMED_ACCESS_CHECK(NAME) \
static bool v8##NAME##NamedSecurityCheck(v8::Local<v8::Object> host, \
                                         v8::Local<v8::Value> key, \
                                         v8::AccessType type, \
                                         v8::Local<v8::Value> data);

#define DECLARE_INDEXED_ACCESS_CHECK(NAME) \
static bool v8##NAME##IndexedSecurityCheck(v8::Local<v8::Object> host, \
                                           uint32_t index, \
                                           v8::AccessType type, \
                                           v8::Local<v8::Value> data);

DECLARE_PROPERTY_ACCESSOR(CanvasRenderingContext2DStrokeStyle)
DECLARE_PROPERTY_ACCESSOR(CanvasRenderingContext2DFillStyle)
// Customized getter&setter of DOMWindow.location
DECLARE_PROPERTY_ACCESSOR_SETTER(DOMWindowLocation)
// Customized setter of DOMWindow.opener
DECLARE_PROPERTY_ACCESSOR_SETTER(DOMWindowOpener)

DECLARE_PROPERTY_ACCESSOR(DocumentLocation)
DECLARE_PROPERTY_ACCESSOR(DocumentImplementation)
DECLARE_PROPERTY_ACCESSOR_GETTER(EventSrcElement)
DECLARE_PROPERTY_ACCESSOR(EventReturnValue)
DECLARE_PROPERTY_ACCESSOR_GETTER(EventDataTransfer)
DECLARE_PROPERTY_ACCESSOR_GETTER(EventClipboardData)

// Getter/Setter for window event handlers
DECLARE_PROPERTY_ACCESSOR(DOMWindowEventHandler)
// Getter/Setter for Element event handlers
DECLARE_PROPERTY_ACCESSOR(ElementEventHandler)

// HTMLCanvasElement
DECLARE_CALLBACK(HTMLCanvasElementGetContext)

// Customized setter of src and location on HTMLFrameElement
DECLARE_PROPERTY_ACCESSOR_SETTER(HTMLFrameElementSrc)
DECLARE_PROPERTY_ACCESSOR_SETTER(HTMLFrameElementLocation)
// Customized setter of src on HTMLIFrameElement
DECLARE_PROPERTY_ACCESSOR_SETTER(HTMLIFrameElementSrc)
// Customized setter of Attr.value
DECLARE_PROPERTY_ACCESSOR_SETTER(AttrValue)

// Customized setter of HTMLOptionsCollection length
DECLARE_PROPERTY_ACCESSOR(HTMLOptionsCollectionLength)

DECLARE_CALLBACK(HTMLInputElementSetSelectionRange)

// Customized accessors for HTMLInputElement
DECLARE_PROPERTY_ACCESSOR(HTMLInputElementSelectionStart)
DECLARE_PROPERTY_ACCESSOR(HTMLInputElementSelectionEnd)

DECLARE_NAMED_ACCESS_CHECK(Location)
DECLARE_INDEXED_ACCESS_CHECK(History)

DECLARE_NAMED_ACCESS_CHECK(History)
DECLARE_INDEXED_ACCESS_CHECK(Location)

// HTMLCollection customized functions.
DECLARE_CALLBACK(HTMLCollectionItem)
DECLARE_CALLBACK(HTMLCollectionNamedItem)
// HTMLCollections are callable as functions.
DECLARE_CALLBACK(HTMLCollectionCallAsFunction)

// HTMLSelectElement customized functions.
DECLARE_CALLBACK(HTMLSelectElementRemove)

// HTMLOptionsCollection customized functions.
DECLARE_CALLBACK(HTMLOptionsCollectionRemove)
DECLARE_CALLBACK(HTMLOptionsCollectionAdd)

// HTMLDocument customized functions
DECLARE_CALLBACK(HTMLDocumentWrite)
DECLARE_CALLBACK(HTMLDocumentWriteln)
DECLARE_CALLBACK(HTMLDocumentOpen)

// Document customized functions
DECLARE_CALLBACK(DocumentEvaluate)
DECLARE_CALLBACK(DocumentGetCSSCanvasContext)

// Window customized functions
DECLARE_CALLBACK(DOMWindowAddEventListener)
DECLARE_CALLBACK(DOMWindowRemoveEventListener)
DECLARE_CALLBACK(DOMWindowPostMessage)
DECLARE_CALLBACK(DOMWindowSetTimeout)
DECLARE_CALLBACK(DOMWindowSetInterval)
DECLARE_CALLBACK(DOMWindowAtob)
DECLARE_CALLBACK(DOMWindowBtoa)
DECLARE_CALLBACK(DOMWindowNOP)
DECLARE_CALLBACK(DOMWindowToString)
DECLARE_CALLBACK(DOMWindowShowModalDialog)
DECLARE_CALLBACK(DOMWindowOpen)
DECLARE_CALLBACK(DOMWindowClearTimeout)
DECLARE_CALLBACK(DOMWindowClearInterval)

DECLARE_CALLBACK(DOMParserConstructor)
DECLARE_CALLBACK(MessageChannelConstructor)
DECLARE_CALLBACK(WebKitCSSMatrixConstructor)
DECLARE_CALLBACK(WebKitPointConstructor)
DECLARE_CALLBACK(XMLHttpRequestConstructor)
DECLARE_CALLBACK(XMLSerializerConstructor)
DECLARE_CALLBACK(XPathEvaluatorConstructor)
DECLARE_CALLBACK(XSLTProcessorConstructor)

// Implementation of custom XSLTProcessor methods.
DECLARE_CALLBACK(XSLTProcessorImportStylesheet)
DECLARE_CALLBACK(XSLTProcessorTransformToFragment)
DECLARE_CALLBACK(XSLTProcessorTransformToDocument)
DECLARE_CALLBACK(XSLTProcessorSetParameter)
DECLARE_CALLBACK(XSLTProcessorGetParameter)
DECLARE_CALLBACK(XSLTProcessorRemoveParameter)

// CSSPrimitiveValue customized functions
DECLARE_CALLBACK(CSSPrimitiveValueGetRGBColorValue)

// Canvas 2D customized functions
DECLARE_CALLBACK(CanvasRenderingContext2DSetStrokeColor)
DECLARE_CALLBACK(CanvasRenderingContext2DSetFillColor)
DECLARE_CALLBACK(CanvasRenderingContext2DStrokeRect)
DECLARE_CALLBACK(CanvasRenderingContext2DSetShadow)
DECLARE_CALLBACK(CanvasRenderingContext2DDrawImage)
DECLARE_CALLBACK(CanvasRenderingContext2DDrawImageFromRect)
DECLARE_CALLBACK(CanvasRenderingContext2DCreatePattern)
DECLARE_CALLBACK(CanvasRenderingContext2DFillText)
DECLARE_CALLBACK(CanvasRenderingContext2DStrokeText)
DECLARE_CALLBACK(CanvasRenderingContext2DPutImageData)

// Implementation of Clipboard attributes and methods.
DECLARE_PROPERTY_ACCESSOR_GETTER(ClipboardTypes)
DECLARE_CALLBACK(ClipboardClearData)
DECLARE_CALLBACK(ClipboardGetData)
DECLARE_CALLBACK(ClipboardSetData)
DECLARE_CALLBACK(ClipboardSetDragImage);

// Implementation of Element methods.
DECLARE_CALLBACK(ElementQuerySelector)
DECLARE_CALLBACK(ElementQuerySelectorAll)
DECLARE_CALLBACK(ElementSetAttribute)
DECLARE_CALLBACK(ElementSetAttributeNode)
DECLARE_CALLBACK(ElementSetAttributeNS)
DECLARE_CALLBACK(ElementSetAttributeNodeNS)

// Implementation of custom Location methods.
DECLARE_PROPERTY_ACCESSOR_SETTER(LocationProtocol)
DECLARE_PROPERTY_ACCESSOR_SETTER(LocationHost)
DECLARE_PROPERTY_ACCESSOR_SETTER(LocationHostname)
DECLARE_PROPERTY_ACCESSOR_SETTER(LocationPort)
DECLARE_PROPERTY_ACCESSOR_SETTER(LocationPathname)
DECLARE_PROPERTY_ACCESSOR_SETTER(LocationSearch)
DECLARE_PROPERTY_ACCESSOR_SETTER(LocationHash)
DECLARE_PROPERTY_ACCESSOR_SETTER(LocationHref)
DECLARE_PROPERTY_ACCESSOR_GETTER(LocationAssign)
DECLARE_PROPERTY_ACCESSOR_GETTER(LocationReplace)
DECLARE_PROPERTY_ACCESSOR_GETTER(LocationReload)
DECLARE_CALLBACK(LocationAssign)
DECLARE_CALLBACK(LocationReplace)
DECLARE_CALLBACK(LocationReload)
DECLARE_CALLBACK(LocationToString)
DECLARE_CALLBACK(LocationValueOf)

// Implementation of EventTarget::addEventListener
// and EventTarget::removeEventListener
DECLARE_CALLBACK(NodeAddEventListener)
DECLARE_CALLBACK(NodeRemoveEventListener)

// Custom implementation is Navigator properties.
// We actually only need this because WebKit has
// navigator.appVersion as custom. Our version just
// passes through.
DECLARE_PROPERTY_ACCESSOR(NavigatorAppVersion)

// Custom implementation of XMLHttpRequest properties
DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestOnabort)
DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestOnerror)
DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestOnload)
DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestOnloadstart)
DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestOnprogress)
DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestOnreadystatechange)
DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestResponseText)
DECLARE_CALLBACK(XMLHttpRequestAddEventListener)
DECLARE_CALLBACK(XMLHttpRequestRemoveEventListener)
DECLARE_CALLBACK(XMLHttpRequestOpen)
DECLARE_CALLBACK(XMLHttpRequestSend)
DECLARE_CALLBACK(XMLHttpRequestSetRequestHeader)
DECLARE_CALLBACK(XMLHttpRequestGetResponseHeader)
DECLARE_CALLBACK(XMLHttpRequestOverrideMimeType)
DECLARE_CALLBACK(XMLHttpRequestDispatchEvent)

// Custom implementation of XMLHttpRequestUpload properties
DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestUploadOnabort)
DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestUploadOnerror)
DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestUploadOnload)
DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestUploadOnloadstart)
DECLARE_PROPERTY_ACCESSOR(XMLHttpRequestUploadOnprogress)
DECLARE_CALLBACK(XMLHttpRequestUploadAddEventListener)
DECLARE_CALLBACK(XMLHttpRequestUploadRemoveEventListener)
DECLARE_CALLBACK(XMLHttpRequestUploadDispatchEvent)

// Custom implementation of TreeWalker functions
DECLARE_CALLBACK(TreeWalkerParentNode)
DECLARE_CALLBACK(TreeWalkerFirstChild)
DECLARE_CALLBACK(TreeWalkerLastChild)
DECLARE_CALLBACK(TreeWalkerNextNode)
DECLARE_CALLBACK(TreeWalkerPreviousNode)
DECLARE_CALLBACK(TreeWalkerNextSibling)
DECLARE_CALLBACK(TreeWalkerPreviousSibling)

// Custom implementation of NodeIterator functions
DECLARE_CALLBACK(NodeIteratorNextNode)
DECLARE_CALLBACK(NodeIteratorPreviousNode)

// Custom implementation of NodeFilter function
DECLARE_CALLBACK(NodeFilterAcceptNode)

// Custom implementation of HTMLFormElement
DECLARE_CALLBACK(HTMLFormElementSubmit)

DECLARE_INDEXED_PROPERTY_GETTER(DOMStringList)
DECLARE_CALLBACK(DOMStringListItem)

DECLARE_NAMED_PROPERTY_GETTER(DOMWindow)
DECLARE_INDEXED_PROPERTY_GETTER(DOMWindow)
DECLARE_NAMED_ACCESS_CHECK(DOMWindow)
DECLARE_INDEXED_ACCESS_CHECK(DOMWindow)

DECLARE_NAMED_PROPERTY_GETTER(HTMLFrameSetElement)
DECLARE_NAMED_PROPERTY_GETTER(HTMLFormElement)
DECLARE_NAMED_PROPERTY_GETTER(HTMLDocument)
DECLARE_NAMED_PROPERTY_SETTER(HTMLDocument)
DECLARE_NAMED_PROPERTY_DELETER(HTMLDocument)
DECLARE_NAMED_PROPERTY_GETTER(NodeList)
DECLARE_NAMED_PROPERTY_GETTER(NamedNodeMap)
DECLARE_NAMED_PROPERTY_GETTER(CSSStyleDeclaration)
DECLARE_NAMED_PROPERTY_SETTER(CSSStyleDeclaration)
DECLARE_NAMED_PROPERTY_GETTER(HTMLPlugInElement)
DECLARE_NAMED_PROPERTY_SETTER(HTMLPlugInElement)
DECLARE_INDEXED_PROPERTY_GETTER(HTMLPlugInElement)
DECLARE_INDEXED_PROPERTY_SETTER(HTMLPlugInElement)

// Plugin object can be called as function.
DECLARE_CALLBACK(HTMLPlugInElement)

DECLARE_NAMED_PROPERTY_GETTER(StyleSheetList)
DECLARE_INDEXED_PROPERTY_GETTER(NamedNodeMap)
DECLARE_INDEXED_PROPERTY_GETTER(HTMLFormElement)
DECLARE_INDEXED_PROPERTY_GETTER(HTMLOptionsCollection)
DECLARE_INDEXED_PROPERTY_SETTER(HTMLOptionsCollection)
DECLARE_INDEXED_PROPERTY_SETTER(HTMLSelectElementCollection)
DECLARE_NAMED_PROPERTY_GETTER(HTMLCollection)

// Canvas and supporting classes
DECLARE_INDEXED_PROPERTY_GETTER(CanvasPixelArray)
DECLARE_INDEXED_PROPERTY_SETTER(CanvasPixelArray)

// MessagePort
DECLARE_PROPERTY_ACCESSOR(MessagePortOnmessage)
DECLARE_PROPERTY_ACCESSOR(MessagePortOnclose)
DECLARE_CALLBACK(MessagePortStartConversation)
DECLARE_CALLBACK(MessagePortAddEventListener)
DECLARE_CALLBACK(MessagePortRemoveEventListener)

// SVG custom properties and callbacks
#if ENABLE(SVG)
DECLARE_PROPERTY_ACCESSOR_GETTER(SVGLengthValue)
DECLARE_CALLBACK(SVGLengthConvertToSpecifiedUnits)
DECLARE_CALLBACK(SVGMatrixInverse)
DECLARE_CALLBACK(SVGMatrixRotateFromVector)
DECLARE_CALLBACK(SVGElementInstanceAddEventListener)
DECLARE_CALLBACK(SVGElementInstanceRemoveEventListener)
#endif

// Worker
#if ENABLE(WORKERS)
DECLARE_PROPERTY_ACCESSOR(WorkerOnmessage)
DECLARE_PROPERTY_ACCESSOR(WorkerOnerror)
DECLARE_CALLBACK(WorkerConstructor)
DECLARE_CALLBACK(WorkerAddEventListener)
DECLARE_CALLBACK(WorkerRemoveEventListener)

DECLARE_PROPERTY_ACCESSOR_GETTER(WorkerContextSelf)
DECLARE_PROPERTY_ACCESSOR(WorkerContextOnmessage)
DECLARE_CALLBACK(WorkerContextSetTimeout)
DECLARE_CALLBACK(WorkerContextClearTimeout)
DECLARE_CALLBACK(WorkerContextSetInterval)
DECLARE_CALLBACK(WorkerContextClearInterval)
DECLARE_CALLBACK(WorkerContextAddEventListener)
DECLARE_CALLBACK(WorkerContextRemoveEventListener)
#endif

#undef DECLARE_INDEXED_ACCESS_CHECK
#undef DECLARE_NAMED_ACCESS_CHECK

#undef DECLARE_PROPERTY_ACCESSOR_SETTER
#undef DECLARE_PROPERTY_ACCESSOR_GETTER
#undef DECLARE_PROPERTY_ACCESSOR

#undef DECLARE_NAMED_PROPERTY_GETTER
#undef DECLARE_NAMED_PROPERTY_SETTER
#undef DECLARE_NAMED_PROPERTY_DELETER

#undef DECLARE_INDEXED_PROPERTY_GETTER
#undef DECLARE_INDEXED_PROPERTY_SETTER
#undef DECLARE_INDEXED_PROPERTY_DELETER

#undef DECLARE_CALLBACK

  // Returns the NPObject corresponding to an HTMLElement object.
  static NPObject* GetHTMLPlugInElementNPObject(v8::Handle<v8::Object> object);

  // Returns the owner frame pointer of a DOM wrapper object. It only works for
  // these DOM objects requiring cross-domain access check.
  static Frame* GetTargetFrame(v8::Local<v8::Object> host,
                               v8::Local<v8::Value> data);

  // Special case for downcasting SVG path segments
#if ENABLE(SVG)
  static V8ClassIndex::V8WrapperType DowncastSVGPathSeg(void* path_seg);
#endif

 private:
  static v8::Handle<v8::Value> WindowSetTimeoutImpl(const v8::Arguments& args,
                                                    bool single_shot);
  static void ClearTimeoutImpl(const v8::Arguments& args);
  static void WindowSetLocation(DOMWindow*, const String&);
};

}  // namespace WebCore

#endif  // V8_CUSTOM_H__

