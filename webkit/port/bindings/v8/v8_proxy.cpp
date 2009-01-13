// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"

#include <algorithm>
#include <utility>

#include <v8.h>
#include <v8-debug.h>

#include "v8_proxy.h"
#include "dom_wrapper_map.h"
#include "v8_index.h"
#include "v8_events.h"
#include "v8_binding.h"
#include "v8_custom.h"
#include "v8_collection.h"
#include "v8_nodefilter.h"
#include "V8DOMWindow.h"

#include "ChromiumBridge.h"

#include "BarInfo.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2D.h"
#include "CanvasStyle.h"
#include "CharacterData.h"
#include "Clipboard.h"
#include "Console.h"
#include "Counter.h"
#include "CSSCharsetRule.h"
#include "CSSFontFaceRule.h"
#include "CSSImportRule.h"
#include "CSSMediaRule.h"
#include "CSSPageRule.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSValueList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSVariablesDeclaration.h"
#include "CSSVariablesRule.h"
#include "DocumentType.h"
#include "DocumentFragment.h"
#include "DOMCoreException.h"
#include "DOMImplementation.h"
#include "DOMParser.h"
#include "DOMSelection.h"
#include "DOMStringList.h"
#include "DOMWindow.h"
#include "Entity.h"
#include "EventListener.h"
#include "EventTargetNode.h"
#include "EventTarget.h"
#include "Event.h"
#include "EventException.h"
#include "ExceptionCode.h"
#include "File.h"
#include "FileList.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "History.h"
#include "HTMLNames.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLSelectElement.h"
#include "HTMLOptionsCollection.h"
#include "ImageData.h"
#include "InspectorController.h"
#include "KeyboardEvent.h"
#include "Location.h"
#include "MediaError.h"
#include "MediaList.h"
#include "MediaPlayer.h"
#include "MessageChannel.h"
#include "MessageEvent.h"
#include "MessagePort.h"
#include "MimeTypeArray.h"
#include "MouseEvent.h"
#include "MutationEvent.h"
#include "Navigator.h"  // for MimeTypeArray
#include "NodeFilter.h"
#include "Notation.h"
#include "NodeList.h"
#include "NodeIterator.h"
#include "OverflowEvent.h"
#include "Page.h"
#include "Plugin.h"
#include "PluginArray.h"
#include "ProcessingInstruction.h"
#include "ProgressEvent.h"
#include "Range.h"
#include "RangeException.h"
#include "Rect.h"
#include "RGBColor.h"
#include "Screen.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StyleSheet.h"
#include "StyleSheetList.h"
#include "TextEvent.h"
#include "TextMetrics.h"
#include "TimeRanges.h"
#include "TreeWalker.h"
#include "WebKitCSSTransformValue.h"
#include "XMLHttpRequest.h"
#include "XMLHttpRequestUpload.h"
#include "XMLHttpRequestException.h"
#include "XMLSerializer.h"
#include "XPathException.h"
#include "XPathExpression.h"
#include "XPathNSResolver.h"
#include "XPathResult.h"
#include "XSLTProcessor.h"
#include "WebKitAnimationEvent.h"
#include "WebKitCSSKeyframeRule.h"
#include "WebKitCSSKeyframesRule.h"
#include "WebKitTransitionEvent.h"
#include "WheelEvent.h"
#include "XMLHttpRequestProgressEvent.h"

#include "V8DOMWindow.h"
#include "V8HTMLElement.h"

#include "ScriptController.h"

#if ENABLE(SVG)
#include "SVGAngle.h"
#include "SVGAnimatedPoints.h"
#include "SVGElement.h"
#include "SVGElementInstance.h"
#include "SVGElementInstanceList.h"
#include "SVGException.h"
#include "SVGLength.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"
#include "SVGPathSeg.h"
#include "SVGPathSegArc.h"
#include "SVGPathSegClosePath.h"
#include "SVGPathSegCurvetoCubic.h"
#include "SVGPathSegCurvetoCubicSmooth.h"
#include "SVGPathSegCurvetoQuadratic.h"
#include "SVGPathSegCurvetoQuadraticSmooth.h"
#include "SVGPathSegLineto.h"
#include "SVGPathSegLinetoHorizontal.h"
#include "SVGPathSegLinetoVertical.h"
#include "SVGPathSegList.h"
#include "SVGPathSegMoveto.h"
#include "SVGPointList.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGRenderingIntent.h"
#include "SVGStringList.h"
#include "SVGTransform.h"
#include "SVGTransformList.h"
#include "SVGUnitTypes.h"
#include "SVGURIReference.h"
#include "SVGZoomEvent.h"
#include "V8SVGPODTypeWrapper.h"
#endif  // SVG

#if ENABLE(XPATH)
#include "XPathEvaluator.h"
#endif

namespace WebCore {


// DOM binding algorithm:
//
// There are two kinds of DOM objects:
// 1. DOM tree nodes, such as Document, HTMLElement, ...
//    there classes implements TreeShared<T> interface;
// 2. Non-node DOM objects, such as CSSRule, Location, etc.
//    these classes implement a ref-counted scheme.
//
// A DOM object may have a JS wrapper object. If a tree node
// is alive, its JS wrapper must be kept alive even it is not
// reachable from JS roots.
// However, JS wrappers of non-node objects can go away if
// not reachable from other JS objects. It works like a cache.
//
// DOM objects are ref-counted, and JS objects are traced from
// a set of root objects. They can create a cycle. To break
// cycles, we do following:
//   Handles from DOM objects to JS wrappers are always weak,
// so JS wrappers of non-node object cannot create a cycle.
//   Before starting a global GC, we create a virtual connection
// between nodes in the same tree in the JS heap. If the wrapper
// of one node in a tree is alive, wrappers of all nodes in
// the same tree are considered alive. This is done by creating
// object groups in GC prologue callbacks. The mark-compact
// collector will remove these groups after each GC.


// Static utility context.
v8::Persistent<v8::Context> V8Proxy::m_utilityContext;


// A helper class for undetectable document.all
class UndetectableHTMLCollection : public HTMLCollection {
};

#ifndef NDEBUG
// Keeps track of global handles created (not JS wrappers
// of DOM objects). Often these global handles are source
// of leaks.
//
// If you want to let a C++ object hold a persistent handle
// to a JS object, you should register the handle here to
// keep track of leaks.
//
// When creating a persistent handle, call:
//
// #ifndef NDEBUG
//    V8Proxy::RegisterGlobalHandle(type, host, handle);
// #endif
//
// When releasing the handle, call:
//
// #ifndef NDEBUG
//    V8Proxy::UnregisterGlobalHandle(type, host, handle);
// #endif
//
typedef HashMap<v8::Value*, GlobalHandleInfo*> GlobalHandleMap;

static GlobalHandleMap& global_handle_map()
{
    static GlobalHandleMap static_global_handle_map;
    return static_global_handle_map;
}


// The USE_VAR(x) template is used to silence C++ compiler warnings
// issued for unused variables (typically parameters or values that
// we want to watch in the debugger).
template <typename T>
static inline void USE_VAR(T) { }

// The function is the place to set the break point to inspect
// live global handles. Leaks are often come from leaked global handles.
static void EnumerateGlobalHandles()
{
  for (GlobalHandleMap::iterator it = global_handle_map().begin(),
    end = global_handle_map().end(); it != end; ++it) {
    GlobalHandleInfo* info = it->second;
    USE_VAR(info);
    v8::Value* handle = it->first;
    USE_VAR(handle);
  }
}

void V8Proxy::RegisterGlobalHandle(GlobalHandleType type, void* host,
                                   v8::Persistent<v8::Value> handle)
{
  ASSERT(!global_handle_map().contains(*handle));
  global_handle_map().set(*handle, new GlobalHandleInfo(host, type));
}


void V8Proxy::UnregisterGlobalHandle(void* host, v8::Persistent<v8::Value> handle)
{
    ASSERT(global_handle_map().contains(*handle));
    GlobalHandleInfo* info = global_handle_map().take(*handle);
    ASSERT(info->host_ == host);
    delete info;
}
#endif  // ifndef NDEBUG

void BatchConfigureAttributes(v8::Handle<v8::ObjectTemplate> inst,
                              v8::Handle<v8::ObjectTemplate> proto,
                              const BatchedAttribute* attrs,
                              size_t num_attrs)
{
  for (size_t i = 0; i < num_attrs; ++i) {
    const BatchedAttribute* a = &attrs[i];
    (a->on_proto ? proto : inst)->SetAccessor(
        v8::String::New(a->name),
        a->getter,
        a->setter,
        a->data == V8ClassIndex::INVALID_CLASS_INDEX
            ? v8::Handle<v8::Value>()
            : v8::Integer::New(V8ClassIndex::ToInt(a->data)),
        a->settings,
        a->attribute);
  }
}

void BatchConfigureConstants(v8::Handle<v8::FunctionTemplate> desc,
                             v8::Handle<v8::ObjectTemplate> proto,
                             const BatchedConstant* consts,
                             size_t num_consts)
{
  for (size_t i = 0; i < num_consts; ++i) {
    const BatchedConstant* c = &consts[i];
    desc->Set(v8::String::New(c->name),
              v8::Integer::New(c->value),
              v8::ReadOnly);
    proto->Set(v8::String::New(c->name),
               v8::Integer::New(c->value),
               v8::ReadOnly);
  }
}

typedef HashMap<Node*, v8::Object*> DOMNodeMap;
typedef HashMap<void*, v8::Object*> DOMObjectMap;

#ifndef NDEBUG
static void EnumerateDOMObjectMap(DOMObjectMap& wrapper_map)
{
  for (DOMObjectMap::iterator it = wrapper_map.begin(), end = wrapper_map.end();
       it != end; ++it) {
    v8::Persistent<v8::Object> wrapper(it->second);
    V8ClassIndex::V8WrapperType type = V8Proxy::GetDOMWrapperType(wrapper);
    void* obj = it->first;
    USE_VAR(type);
    USE_VAR(obj);
  }
}


static void EnumerateDOMNodeMap(DOMNodeMap& node_map)
{
  for (DOMNodeMap::iterator it = node_map.begin(), end = node_map.end();
    it != end; ++it) {
    Node* node = it->first;
    USE_VAR(node);
    ASSERT(v8::Persistent<v8::Object>(it->second).IsWeak());
  }
}
#endif  // NDEBUG

static void WeakDOMObjectCallback(v8::Persistent<v8::Value> obj, void* para);
static void WeakActiveDOMObjectCallback(v8::Persistent<v8::Value> obj,
                                        void* para);
static void WeakNodeCallback(v8::Persistent<v8::Value> obj, void* para);
// A map from DOM node to its JS wrapper.
static DOMWrapperMap<Node>& dom_node_map()
{
  static DOMWrapperMap<Node> static_dom_node_map(&WeakNodeCallback);
  return static_dom_node_map;
}


// A map from a DOM object (non-node) to its JS wrapper. This map does not
// contain the DOM objects which can have pending activity (active dom objects).
static DOMWrapperMap<void>& dom_object_map()
{
  static DOMWrapperMap<void>
    static_dom_object_map(&WeakDOMObjectCallback);
  return static_dom_object_map;
}


// A map from a DOM object to its JS wrapper for DOM objects which
// can have pending activity.
static DOMWrapperMap<void>& active_dom_object_map()
{
  static DOMWrapperMap<void>
    static_active_dom_object_map(&WeakActiveDOMObjectCallback);
  return static_active_dom_object_map;
}

#if ENABLE(SVG)
static void WeakSVGElementInstanceCallback(v8::Persistent<v8::Value> obj,
                                           void* param);

// A map for SVGElementInstances.
static DOMWrapperMap<SVGElementInstance>& dom_svg_element_instance_map()
{
  static DOMWrapperMap<SVGElementInstance>
      static_dom_svg_element_instance_map(&WeakSVGElementInstanceCallback);
  return static_dom_svg_element_instance_map;
}

static void WeakSVGElementInstanceCallback(v8::Persistent<v8::Value> obj,
                                           void* param)
{
  SVGElementInstance* instance = static_cast<SVGElementInstance*>(param);
  ASSERT(dom_svg_element_instance_map().contains(instance));

  instance->deref();
  dom_svg_element_instance_map().forget(instance);
}

v8::Handle<v8::Value> V8Proxy::SVGElementInstanceToV8Object(
    SVGElementInstance* instance)
{
  if (!instance)
    return v8::Null();

  v8::Handle<v8::Object> existing_instance = dom_svg_element_instance_map().get(instance);
  if (!existing_instance.IsEmpty())
    return existing_instance;

  instance->ref();

  // Instantiate the V8 object and remember it
  v8::Handle<v8::Object> result =
      InstantiateV8Object(V8ClassIndex::SVGELEMENTINSTANCE,
                          V8ClassIndex::SVGELEMENTINSTANCE,
                          instance);
  if (!result.IsEmpty()) {
    // Only update the DOM SVG element map if the result is non-empty.
    dom_svg_element_instance_map().set(instance,
      v8::Persistent<v8::Object>::New(result));
  }
  return result;
}


// SVG non-node elements may have a reference to a context node which
// should be notified when the element is change
static void WeakSVGObjectWithContext(v8::Persistent<v8::Value> obj,
                                     void* dom_obj);

// Map of SVG objects with contexts to V8 objects
static DOMWrapperMap<void>& dom_svg_object_with_context_map() {
    static DOMWrapperMap<void>
        static_dom_svg_object_with_context_map(&WeakSVGObjectWithContext);
    return static_dom_svg_object_with_context_map;
}

// Map of SVG objects with contexts to their contexts
static HashMap<void*, SVGElement*>& svg_object_to_context_map()
{
    static HashMap<void*, SVGElement*> static_svg_object_to_context_map;
    return static_svg_object_to_context_map;
}

v8::Handle<v8::Value> V8Proxy::SVGObjectWithContextToV8Object(
    V8ClassIndex::V8WrapperType type, void* object) 
{
  if (!object)
    return v8::Null();

  v8::Persistent<v8::Object> result =
    dom_svg_object_with_context_map().get(object);
  if (!result.IsEmpty()) return result;

  // Special case: SVGPathSegs need to be downcast to their real type
  if (type == V8ClassIndex::SVGPATHSEG)
    type = V8Custom::DowncastSVGPathSeg(object);

  v8::Local<v8::Object> v8obj = InstantiateV8Object(type, type, object);
  if (!v8obj.IsEmpty()) {
    result = v8::Persistent<v8::Object>::New(v8obj);
    switch (type) {
#define MAKE_CASE(TYPE, NAME)     \
      case V8ClassIndex::TYPE: static_cast<NAME*>(object)->ref(); break;
SVG_OBJECT_TYPES(MAKE_CASE)
#undef MAKE_CASE
#define MAKE_CASE(TYPE, NAME)     \
      case V8ClassIndex::TYPE:    \
        static_cast<V8SVGPODTypeWrapper<NAME>*>(object)->ref(); break;
SVG_POD_NATIVE_TYPES(MAKE_CASE)
#undef MAKE_CASE
      default:
        ASSERT(false);
    }
    dom_svg_object_with_context_map().set(object, result);
  }

  return result;
}

static void WeakSVGObjectWithContext(v8::Persistent<v8::Value> obj,
                                     void* dom_obj)
{
  v8::HandleScope handle_scope;
  ASSERT(dom_svg_object_with_context_map().contains(dom_obj));
  ASSERT(obj->IsObject());

  // Forget function removes object from the map and dispose the wrapper.
  dom_svg_object_with_context_map().forget(dom_obj);

  V8ClassIndex::V8WrapperType type =
    V8Proxy::GetDOMWrapperType(v8::Handle<v8::Object>::Cast(obj));

  switch (type) {
#define MAKE_CASE(TYPE, NAME)     \
    case V8ClassIndex::TYPE: static_cast<NAME*>(dom_obj)->deref(); break;
SVG_OBJECT_TYPES(MAKE_CASE)
#undef MAKE_CASE
#define MAKE_CASE(TYPE, NAME)     \
    case V8ClassIndex::TYPE:      \
      static_cast<V8SVGPODTypeWrapper<NAME>*>(dom_obj)->deref(); break;
SVG_POD_NATIVE_TYPES(MAKE_CASE)
#undef MAKE_CASE
    default:
      ASSERT(false);
  }
}

void V8Proxy::SetSVGContext(void* obj, SVGElement* context)
{
  SVGElement* old_context = svg_object_to_context_map().get(obj);

  if (old_context == context)
    return;

  if (old_context)
    old_context->deref();

  if (context)
    context->ref();

  svg_object_to_context_map().set(obj, context);
}

SVGElement* V8Proxy::GetSVGContext(void* obj)
{
    return svg_object_to_context_map().get(obj);
}

#endif


// Called when obj is near death (not reachable from JS roots)
// It is time to remove the entry from the table and dispose
// the handle.
static void WeakDOMObjectCallback(v8::Persistent<v8::Value> obj,
                                  void* dom_obj) {
  v8::HandleScope scope;
  ASSERT(dom_object_map().contains(dom_obj));
  ASSERT(obj->IsObject());

  // Forget function removes object from the map and dispose the wrapper.
  dom_object_map().forget(dom_obj);

  V8ClassIndex::V8WrapperType type =
    V8Proxy::GetDOMWrapperType(v8::Handle<v8::Object>::Cast(obj));
  switch (type) {
#define MAKE_CASE(TYPE, NAME)   \
    case V8ClassIndex::TYPE: static_cast<NAME*>(dom_obj)->deref(); break;
    DOM_OBJECT_TYPES(MAKE_CASE)
#undef MAKE_CASE
    default:
      ASSERT(false);
  }
}


static void WeakActiveDOMObjectCallback(v8::Persistent<v8::Value> obj,
                                        void* dom_obj)
{
  v8::HandleScope scope;
  ASSERT(active_dom_object_map().contains(dom_obj));
  ASSERT(obj->IsObject());

  // Forget function removes object from the map and dispose the wrapper.
  active_dom_object_map().forget(dom_obj);

  V8ClassIndex::V8WrapperType type =
    V8Proxy::GetDOMWrapperType(v8::Handle<v8::Object>::Cast(obj));
  switch (type) {
#define MAKE_CASE(TYPE, NAME)   \
    case V8ClassIndex::TYPE: static_cast<NAME*>(dom_obj)->deref(); break;
    ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
#undef MAKE_CASE
    default:
      ASSERT(false);
  }
}

static void WeakNodeCallback(v8::Persistent<v8::Value> obj, void* param)
{
  Node* node = static_cast<Node*>(param);
  ASSERT(dom_node_map().contains(node));

  dom_node_map().forget(node);
  node->deref();
}


// A map from a DOM node to its JS wrapper, the wrapper
// is kept as a strong reference to survive GCs.
static DOMObjectMap& gc_protected_map() {
    static DOMObjectMap static_gc_protected_map;
    return static_gc_protected_map;
}

// static
void V8Proxy::GCProtect(void* dom_object)
{
  if (!dom_object)
      return;
  if (gc_protected_map().contains(dom_object))
      return;
  if (!dom_object_map().contains(dom_object))
      return;

  // Create a new (strong) persistent handle for the object.
  v8::Persistent<v8::Object> wrapper = dom_object_map().get(dom_object);
  if (wrapper.IsEmpty()) return;

  gc_protected_map().set(dom_object, *v8::Persistent<v8::Object>::New(wrapper));
}


// static
void V8Proxy::GCUnprotect(void* dom_object)
{
  if (!dom_object)
      return;
  if (!gc_protected_map().contains(dom_object))
      return;

  // Dispose the strong reference.
  v8::Persistent<v8::Object> wrapper(gc_protected_map().take(dom_object));
  wrapper.Dispose();
}


// Create object groups for DOM tree nodes.
static void GCPrologue()
{
  v8::HandleScope scope;

#ifndef NDEBUG
  EnumerateDOMObjectMap(dom_object_map().impl());
#endif

  // Run through all objects with possible pending activity making their
  // wrappers non weak if there is pending activity.
  DOMObjectMap active_map = active_dom_object_map().impl();
  for (DOMObjectMap::iterator it = active_map.begin(), end = active_map.end();
    it != end; ++it) {
    void* obj = it->first;
    v8::Persistent<v8::Object> wrapper = v8::Persistent<v8::Object>(it->second);
    ASSERT(wrapper.IsWeak());
    V8ClassIndex::V8WrapperType type = V8Proxy::GetDOMWrapperType(wrapper);
    switch (type) {
#define MAKE_CASE(TYPE, NAME)                   \
      case V8ClassIndex::TYPE: {                \
        NAME* impl = static_cast<NAME*>(obj);   \
        if (impl->hasPendingActivity())         \
          wrapper.ClearWeak();                  \
        break;                                  \
      }
ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
      default:
        ASSERT(false);
#undef MAKE_CASE
    }

    // Additional handling of message port ensuring that entangled ports also
    // have their wrappers entangled. This should ideally be handled when the
    // ports are actually entangled in MessagePort::entangle, but to avoid
    // forking MessagePort.* this is postponed to GC time. Having this postponed
    // has the drawback that the wrappers are "entangled/unentangled" for each
    // GC even though their entnaglement most likely is still the same.
    if (type == V8ClassIndex::MESSAGEPORT) {
      // Get the port and its entangled port.
      MessagePort* port1 = static_cast<MessagePort*>(obj);
      MessagePort* port2 = port1->entangledPort();
      if (port2 != NULL) {
        // As ports are always entangled in pairs only perform the entanglement
        // once for each pair (see ASSERT in MessagePort::unentangle()).
        if (port1 < port2) {
          v8::Handle<v8::Value> port1_wrapper =
              V8Proxy::ToV8Object(V8ClassIndex::MESSAGEPORT, port1);
          v8::Handle<v8::Value> port2_wrapper =
              V8Proxy::ToV8Object(V8ClassIndex::MESSAGEPORT, port2);
          ASSERT(port1_wrapper->IsObject());
          v8::Handle<v8::Object>::Cast(port1_wrapper)->SetInternalField(
              V8Custom::kMessagePortEntangledPortIndex, port2_wrapper);
          ASSERT(port2_wrapper->IsObject());
          v8::Handle<v8::Object>::Cast(port2_wrapper)->SetInternalField(
              V8Custom::kMessagePortEntangledPortIndex, port1_wrapper);
        }
      } else {
        // Remove the wrapper entanglement when a port is not entangled.
        if (V8Proxy::DOMObjectHasJSWrapper(port1)) {
          v8::Handle<v8::Value> wrapper =
            V8Proxy::ToV8Object(V8ClassIndex::MESSAGEPORT, port1);
          ASSERT(wrapper->IsObject());
          v8::Handle<v8::Object>::Cast(wrapper)->SetInternalField(
            V8Custom::kMessagePortEntangledPortIndex, v8::Undefined());
        }
      }
    }
  }

  // Create object groups.
  typedef std::pair<uintptr_t, Node*> GrouperPair;
  typedef Vector<GrouperPair> GrouperList;

  DOMNodeMap node_map = dom_node_map().impl();
  GrouperList grouper;
  grouper.reserveCapacity(node_map.size());

  for (DOMNodeMap::iterator it = node_map.begin(), end = node_map.end();
    it != end; ++it) {
    Node* node = it->first;

    // If the node is in document, put it in the ownerDocument's object group.
    //
    // If an image element was created by JavaScript "new Image",
    // it is not in a document. However, if the load event has not
    // been fired (still onloading), it is treated as in the document.
    //
    // Otherwise, the node is put in an object group identified by the root
    // elment of the tree to which it belongs.
    uintptr_t group_id;
    if (node->inDocument() ||
        (node->hasTagName(HTMLNames::imgTag) &&
         !static_cast<HTMLImageElement*>(node)->haveFiredLoadEvent())) {
      group_id = reinterpret_cast<uintptr_t>(node->document());
    } else {
      Node* root = node;
      while (root->parent())
        root = root->parent();

      // If the node is alone in its DOM tree (doesn't have a parent or any
      // children) then the group will be filtered out later anyway.
      if (root == node && !node->hasChildNodes())
        continue;

      group_id = reinterpret_cast<uintptr_t>(root);
    }
    grouper.append(GrouperPair(group_id, node));
  }

  // Group by sorting by the group id.  This will use the std::pair operator<,
  // which will really sort by both the group id and the Node*.  However the
  // Node* is only involved to sort within a group id, so it will be fine.
  std::sort(grouper.begin(), grouper.end());

  // TODO(deanm): Should probably work in iterators here, but indexes were
  // easier for my simple mind.
  for (size_t i = 0; i < grouper.size(); ) {
    // Seek to the next key (or the end of the list).
    size_t next_key_index = grouper.size();
    for (size_t j = i; j < grouper.size(); ++j) {
      if (grouper[i].first != grouper[j].first) {
        next_key_index = j;
        break;
      }
    }

    ASSERT(next_key_index > i);

    // We only care about a group if it has more than one object.  If it only
    // has one object, it has nothing else that needs to be kept alive.
    if (next_key_index - i <= 1) {
      i = next_key_index;
      continue;
    }

    Vector<v8::Persistent<v8::Value> > group;
    group.reserveCapacity(next_key_index - i);
    for (; i < next_key_index; ++i) {
      v8::Persistent<v8::Value> wrapper =
          dom_node_map().get(grouper[i].second);
      if (!wrapper.IsEmpty())
        group.append(wrapper);
    }

    if (group.size() > 1)
      v8::V8::AddObjectGroup(&group[0], group.size());

    ASSERT(i == next_key_index);
  }
}


static void GCEpilogue()
{
  v8::HandleScope scope;

  // Run through all objects with pending activity making their wrappers weak
  // again.
  DOMObjectMap active_map = active_dom_object_map().impl();
  for (DOMObjectMap::iterator it = active_map.begin(), end = active_map.end();
    it != end; ++it) {
    void* obj = it->first;
    v8::Persistent<v8::Object> wrapper = v8::Persistent<v8::Object>(it->second);
    V8ClassIndex::V8WrapperType type = V8Proxy::GetDOMWrapperType(wrapper);
    switch (type) {
#define MAKE_CASE(TYPE, NAME)                                     \
      case V8ClassIndex::TYPE: {                                  \
        NAME* impl = static_cast<NAME*>(obj);                     \
        if (impl->hasPendingActivity()) {                         \
          ASSERT(!wrapper.IsWeak());                              \
          wrapper.MakeWeak(impl, &WeakActiveDOMObjectCallback);   \
        }                                                         \
        break;                                                    \
      }
ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
      default:
        ASSERT(false);
#undef MAKE_CASE
    }
  }

#ifndef NDEBUG
  // Check all survivals are weak.
  EnumerateDOMObjectMap(dom_object_map().impl());
  EnumerateDOMNodeMap(dom_node_map().impl());
  EnumerateDOMObjectMap(gc_protected_map());
  EnumerateGlobalHandles();
#undef USE_VAR
#endif
}


typedef HashMap<int, v8::FunctionTemplate*> FunctionTemplateMap;

bool AllowAllocation::m_current = false;


// JavaScriptConsoleMessages encapsulate everything needed to
// log messages originating from JavaScript to the Chrome console.
class JavaScriptConsoleMessage {
 public:
  JavaScriptConsoleMessage(const String& str,
                           const String& sourceID,
                           unsigned lineNumber)
    : m_string(str)
    , m_sourceID(sourceID)
    , m_lineNumber(lineNumber) { }

  void AddToPage(Page* page) const;

 private:
  const String m_string;
  const String m_sourceID;
  const unsigned m_lineNumber;
};

void JavaScriptConsoleMessage::AddToPage(Page* page) const
{
    ASSERT(page);
    Console* console = page->mainFrame()->domWindow()->console();
    console->addMessage(JSMessageSource, ErrorMessageLevel, m_string, m_lineNumber, m_sourceID);
}

// The ConsoleMessageManager handles all console messages that stem
// from JavaScript. It keeps a list of messages that have been delayed but
// it makes sure to add all messages to the console in the right order.
class ConsoleMessageManager {
 public:
  // Add a message to the console. May end up calling JavaScript code
  // indirectly through the inspector so only call this function when
  // it is safe to do allocations.
  static void AddMessage(Page* page, const JavaScriptConsoleMessage& message);

  // Add a message to the console but delay the reporting until it
  // is safe to do so: Either when we leave JavaScript execution or
  // when adding other console messages. The primary purpose of this
  // method is to avoid calling into V8 to handle console messages
  // when the VM is in a state that does not support GCs or allocations.
  // Delayed messages are always reported in the page corresponding
  // to the active context.
  static void AddDelayedMessage(const JavaScriptConsoleMessage& message);

  // Process any delayed messages. May end up calling JavaScript code
  // indirectly through the inspector so only call this function when
  // it is safe to do allocations.
  static void ProcessDelayedMessages();

 private:
  // All delayed messages are stored in this vector. If the vector
  // is NULL, there are no delayed messages.
  static Vector<JavaScriptConsoleMessage>* m_delayed;
};


Vector<JavaScriptConsoleMessage>* ConsoleMessageManager::m_delayed = NULL;


void ConsoleMessageManager::AddMessage(
    Page* page,
    const JavaScriptConsoleMessage& message)
{
  // Process any delayed messages to make sure that messages
  // appear in the right order in the console.
  ProcessDelayedMessages();
  message.AddToPage(page);
}


void ConsoleMessageManager::AddDelayedMessage(const JavaScriptConsoleMessage& message)
{
    if (!m_delayed)
        // Allocate a vector for the delayed messages. Will be
        // deallocated when the delayed messages are processed
        // in ProcessDelayedMessages().
        m_delayed = new Vector<JavaScriptConsoleMessage>();
    m_delayed->append(message);
}


void ConsoleMessageManager::ProcessDelayedMessages()
{
    // If we have a delayed vector it cannot be empty.
    if (!m_delayed)
        return;
    ASSERT(!m_delayed->isEmpty());

    // Add the delayed messages to the page of the active
    // context. If that for some bizarre reason does not
    // exist, we clear the list of delayed messages to avoid
    // posting messages. We still deallocate the vector.
    Frame* frame = V8Proxy::retrieveActiveFrame();
    Page* page = NULL;
    if (frame)
        page = frame->page();
    if (!page)
        m_delayed->clear();

    // Iterate through all the delayed messages and add them
    // to the console.
    const int size = m_delayed->size();
    for (int i = 0; i < size; i++) {
        m_delayed->at(i).AddToPage(page);
    }

    // Deallocate the delayed vector.
    delete m_delayed;
    m_delayed = NULL;
}


// Convenience class for ensuring that delayed messages in the
// ConsoleMessageManager are processed quickly.
class ConsoleMessageScope {
 public:
  ConsoleMessageScope() { ConsoleMessageManager::ProcessDelayedMessages(); }
  ~ConsoleMessageScope() { ConsoleMessageManager::ProcessDelayedMessages(); }
};

void log_info(Frame* frame, const String& msg, const String& url)
{
    Page* page = frame->page();
    if (!page)
        return;
    JavaScriptConsoleMessage message(msg, url, 0);
    ConsoleMessageManager::AddMessage(page, message);
}

static void HandleConsoleMessage(v8::Handle<v8::Message> message,
                                 v8::Handle<v8::Value> data)
{
  // Use the frame where JavaScript is called from.
  Frame* frame = V8Proxy::retrieveActiveFrame();
  if (!frame)
      return;

  Page* page = frame->page();
  if (!page)
      return;

  v8::Handle<v8::String> errorMessageString = message->Get();
  ASSERT(!errorMessageString.IsEmpty());
  String errorMessage = ToWebCoreString(errorMessageString);

  v8::Handle<v8::Value> resourceName = message->GetScriptResourceName();
  bool useURL = (resourceName.IsEmpty() || !resourceName->IsString());
  String resourceNameString = (useURL)
      ? frame->document()->url()
      : ToWebCoreString(resourceName);
  JavaScriptConsoleMessage consoleMessage(errorMessage,
                                          resourceNameString,
                                          message->GetLineNumber());
  ConsoleMessageManager::AddMessage(page, consoleMessage);
}


enum DelayReporting {
  REPORT_LATER,
  REPORT_NOW
};


static void ReportUnsafeAccessTo(Frame* target, DelayReporting delay)
{
  ASSERT(target);
  Document* targetDocument = target->document();
  if (!targetDocument)
      return;

  Frame* source = V8Proxy::retrieveActiveFrame();
  if (!source || !source->document())
      return;  // Ignore error if the source document is gone.

  Document* sourceDocument = source->document();

  // FIXME: This error message should contain more specifics of why the same
  // origin check has failed.
  String str = String::format("Unsafe JavaScript attempt to access frame "
      "with URL %s from frame with URL %s. "
      "Domains, protocols and ports must match.\n",
      targetDocument->url().string().utf8().data(),
      sourceDocument->url().string().utf8().data());

  // Build a console message with fake source ID and line number.
  const String kSourceID = "";
  const int kLineNumber = 1;
  JavaScriptConsoleMessage message(str, kSourceID, kLineNumber);

  if (delay == REPORT_NOW) {
    // NOTE(tc): Apple prints the message in the target page, but it seems like
    // it should be in the source page. Even for delayed messages, we put it in
    // the source page; see ConsoleMessageManager::ProcessDelayedMessages().
    ConsoleMessageManager::AddMessage(source->page(), message);

  } else {
    ASSERT(delay == REPORT_LATER);
    // We cannot safely report the message eagerly, because this may cause
    // allocations and GCs internally in V8 and we cannot handle that at this
    // point. Therefore we delay the reporting.
    ConsoleMessageManager::AddDelayedMessage(message);
  }
}

static void ReportUnsafeJavaScriptAccess(v8::Local<v8::Object> host,
                                         v8::AccessType type,
                                         v8::Local<v8::Value> data)
{
    Frame* target = V8Custom::GetTargetFrame(host, data);
    if (target)
        ReportUnsafeAccessTo(target, REPORT_LATER);
}

static void HandleFatalErrorInV8()
{
    // TODO: We temporarily deal with V8 internal error situations
    // such as out-of-memory by crashing the renderer.
    CRASH();
}

static void ReportFatalErrorInV8(const char* location, const char* message)
{
    // V8 is shutdown, we cannot use V8 api.
    // The only thing we can do is to disable JavaScript.
    // TODO: clean up V8Proxy and disable JavaScript.
    printf("V8 error: %s (%s)\n", message, location);
    HandleFatalErrorInV8();
}

V8Proxy::~V8Proxy()
{
    clearForClose();
    DestroyGlobal();
}

void V8Proxy::DestroyGlobal()
{
    if (!m_global.IsEmpty()) {
#ifndef NDEBUG
        UnregisterGlobalHandle(this, m_global);
#endif
        m_global.Dispose();
        m_global.Clear();
    }
}


bool V8Proxy::DOMObjectHasJSWrapper(void* obj) {
    return dom_object_map().contains(obj) ||
           active_dom_object_map().contains(obj);
}


// The caller must have increased obj's ref count.
void V8Proxy::SetJSWrapperForDOMObject(void* obj, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(MaybeDOMWrapper(wrapper));
#ifndef NDEBUG
    V8ClassIndex::V8WrapperType type = V8Proxy::GetDOMWrapperType(wrapper);
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE:
ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
      ASSERT(false);
#undef MAKE_CASE
      default: break;
    }
#endif
    dom_object_map().set(obj, wrapper);
}

// The caller must have increased obj's ref count.
void V8Proxy::SetJSWrapperForActiveDOMObject(void* obj, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(MaybeDOMWrapper(wrapper));
#ifndef NDEBUG
    V8ClassIndex::V8WrapperType type = V8Proxy::GetDOMWrapperType(wrapper);
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE: break;
ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
      default: ASSERT(false);
#undef MAKE_CASE
    }
#endif
    active_dom_object_map().set(obj, wrapper);
}

// The caller must have increased node's ref count.
void V8Proxy::SetJSWrapperForDOMNode(Node* node, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(MaybeDOMWrapper(wrapper));
    dom_node_map().set(node, wrapper);
}

PassRefPtr<EventListener> V8Proxy::createInlineEventListener(
                                               const String& functionName,
                                               const String& code, Node* node)
{
    return V8LazyEventListener::create(m_frame, code, functionName);
}

#if ENABLE(SVG)
PassRefPtr<EventListener> V8Proxy::createSVGEventHandler(const String& functionName,
                                              const String& code, Node* node)
{
    return V8LazyEventListener::create(m_frame, code, functionName);
}
#endif


// Event listeners

static V8EventListener* FindEventListenerInList(V8EventListenerList& list,
                                                v8::Local<v8::Value> listener,
                                                bool isInline)
{
  ASSERT(v8::Context::InContext());

  if (!listener->IsObject())
      return 0;

  V8EventListenerList::iterator p = list.begin();
  while (p != list.end()) {
    V8EventListener* el = *p;
    v8::Local<v8::Object> wrapper = el->GetListenerObject();
    ASSERT(!wrapper.IsEmpty());
    // Since the listener is an object, it is safe to compare for 
    // strict equality (in the JS sense) by doing a simple equality
    // check using the == operator on the handles. This is much,
    // much faster than calling StrictEquals through the API in 
    // the negative case.
    if (el->isInline() == isInline && listener == wrapper)
        return el;
    ++p;
  }
  return 0;
}

// Find an existing wrapper for a JS event listener in the map.
PassRefPtr<V8EventListener> V8Proxy::FindV8EventListener(v8::Local<v8::Value> listener,
                                              bool isInline)
{
    return FindEventListenerInList(m_event_listeners, listener, isInline);
}

PassRefPtr<V8EventListener> V8Proxy::FindOrCreateV8EventListener(v8::Local<v8::Value> obj, bool isInline)
{
  ASSERT(v8::Context::InContext());

  if (!obj->IsObject())
      return 0;

  V8EventListener* wrapper =
      FindEventListenerInList(m_event_listeners, obj, isInline);
  if (wrapper)
      return wrapper;

  // Create a new one, and add to cache.
  RefPtr<V8EventListener> new_listener =
    V8EventListener::create(m_frame, v8::Local<v8::Object>::Cast(obj), isInline);
  m_event_listeners.push_back(new_listener.get());

  return new_listener;
}


// Object event listeners (such as XmlHttpRequest and MessagePort) are
// different from listeners on DOM nodes. An object event listener wrapper
// only holds a weak reference to the JS function. A strong reference can
// create a cycle.
//
// The lifetime of these objects is bounded by the life time of its JS
// wrapper. So we can create a hidden reference from the JS wrapper to
// to its JS function.
//
//                          (map) 
//              XHR      <----------  JS_wrapper
//               |             (hidden) :  ^
//               V                      V  : (may reachable by closure)
//           V8_listener  --------> JS_function
//                         (weak)  <-- may create a cycle if it is strong
//
// The persistent reference is made weak in the constructor
// of V8ObjectEventListener.

PassRefPtr<V8EventListener> V8Proxy::FindObjectEventListener(
    v8::Local<v8::Value> listener, bool isInline)
{
  return FindEventListenerInList(m_xhr_listeners, listener, isInline);
}


PassRefPtr<V8EventListener> V8Proxy::FindOrCreateObjectEventListener(
    v8::Local<v8::Value> obj, bool isInline)
{
  ASSERT(v8::Context::InContext());

  if (!obj->IsObject())
    return 0;

  V8EventListener* wrapper =
      FindEventListenerInList(m_xhr_listeners, obj, isInline);
  if (wrapper)
    return wrapper;

  // Create a new one, and add to cache.
  RefPtr<V8EventListener> new_listener =
    V8ObjectEventListener::create(m_frame, v8::Local<v8::Object>::Cast(obj), isInline);
  m_xhr_listeners.push_back(new_listener.get());

  return new_listener.release();
}


static void RemoveEventListenerFromList(V8EventListenerList& list,
                                        V8EventListener* listener)
{
  V8EventListenerList::iterator p = list.begin();
  while (p != list.end()) {
    if (*p == listener) {
      list.erase(p);
      return;
    }
    ++p;
  }
}


void V8Proxy::RemoveV8EventListener(V8EventListener* listener)
{
  RemoveEventListenerFromList(m_event_listeners, listener);
}


void V8Proxy::RemoveObjectEventListener(V8ObjectEventListener* listener)
{
  RemoveEventListenerFromList(m_xhr_listeners, listener);
}


static void DisconnectEventListenersInList(V8EventListenerList& list)
{
  V8EventListenerList::iterator p = list.begin();
  while (p != list.end()) {
    (*p)->disconnectFrame();
    ++p;
  }
  list.clear();
}


void V8Proxy::DisconnectEventListeners()
{
  DisconnectEventListenersInList(m_event_listeners);
  DisconnectEventListenersInList(m_xhr_listeners);
}


v8::Handle<v8::Script> V8Proxy::CompileScript(v8::Handle<v8::String> code,
                                              const String& fileName,
                                              int baseLine)
{
    const uint16_t* fileNameString = FromWebCoreString(fileName);
    v8::Handle<v8::String> name =
      v8::String::New(fileNameString, fileName.length());
    v8::Handle<v8::Integer> line = v8::Integer::New(baseLine);
    v8::ScriptOrigin origin(name, line);
    v8::Handle<v8::Script> script = v8::Script::Compile(code, &origin);
    return script;
}

bool V8Proxy::HandleOutOfMemory()
{
    v8::Local<v8::Context> context = v8::Context::GetCurrent();

    if (!context->HasOutOfMemoryException())
        return false;

    // Warning, error, disable JS for this frame?
    Frame* frame = V8Proxy::retrieveFrame(context);

    V8Proxy* proxy = V8Proxy::retrieve(frame);
    // Clean m_context, and event handlers.
    proxy->clearForClose();
    // Destroy the global object.
    proxy->DestroyGlobal();

    ChromiumBridge::notifyJSOutOfMemory(frame);

    // Disable JS.
    Settings* settings = frame->settings();
    ASSERT(settings);
    settings->setJavaScriptEnabled(false);

    return true;
}

v8::Local<v8::Value> V8Proxy::Evaluate(const String& fileName, int baseLine,
                                       const String& str, Node* n)
{
    ASSERT(v8::Context::InContext());

    // Compile the script.
    v8::Local<v8::String> code = v8ExternalString(str);
    ChromiumBridge::traceEventBegin("v8.compile", n, "");
    v8::Handle<v8::Script> script = CompileScript(code, fileName, baseLine);
    ChromiumBridge::traceEventEnd("v8.compile", n, "");

    // Set inlineCode to true for <a href="javascript:doSomething()">
    // and false for <script>doSomething</script>. For some reason, fileName
    // gives us this information.
    ChromiumBridge::traceEventBegin("v8.run", n, "");
    v8::Local<v8::Value> result = RunScript(script, fileName.isNull());
    ChromiumBridge::traceEventEnd("v8.run", n, "");
    return result;
}

v8::Local<v8::Value> V8Proxy::RunScript(v8::Handle<v8::Script> script,
                                        bool inline_code)
{
  if (script.IsEmpty())
    return v8::Local<v8::Value>();

  // Compute the source string and prevent against infinite recursion.
  if (m_recursion >= 20) {
    v8::Local<v8::String> code =
        v8ExternalString("throw RangeError('Recursion too deep')");
    // TODO(kasperl): Ideally, we should be able to re-use the origin of the
    // script passed to us as the argument instead of using an empty string
    // and 0 baseLine.
    script = CompileScript(code, "", 0);
  }

  if (HandleOutOfMemory())
    ASSERT(script.IsEmpty());

  if (script.IsEmpty())
    return v8::Local<v8::Value>();

  // Save the previous value of the inlineCode flag and update the flag for
  // the duration of the script invocation.
  bool previous_inline_code = inlineCode();
  setInlineCode(inline_code);

  // Run the script and keep track of the current recursion depth.
  v8::Local<v8::Value> result;
  { ConsoleMessageScope scope;
    m_recursion++;

    // Evaluating the JavaScript could cause the frame to be deallocated,
    // so we start the keep alive timer here.
    // Frame::keepAlive method adds the ref count of the frame and sets a
    // timer to decrease the ref count. It assumes that the current JavaScript
    // execution finishs before firing the timer.
    // See issue 1218756 and 914430.
    m_frame->keepAlive();

    result = script->Run();
    m_recursion--;
  }

  if (HandleOutOfMemory())
    ASSERT(result.IsEmpty());

  // Handle V8 internal error situation (Out-of-memory).
  if (result.IsEmpty())
    return v8::Local<v8::Value>();

  // Restore inlineCode flag.
  setInlineCode(previous_inline_code);

  if (v8::V8::IsDead())
    HandleFatalErrorInV8();

  return result;
}


v8::Local<v8::Value> V8Proxy::CallFunction(v8::Handle<v8::Function> function,
                                           v8::Handle<v8::Object> receiver,
                                           int argc,
                                           v8::Handle<v8::Value> args[])
{
  // For now, we don't put any artificial limitations on the depth
  // of recursion that stems from calling functions. This is in
  // contrast to the script evaluations.
  v8::Local<v8::Value> result;
  { 
    ConsoleMessageScope scope;

    // Evaluating the JavaScript could cause the frame to be deallocated,
    // so we start the keep alive timer here.
    // Frame::keepAlive method adds the ref count of the frame and sets a
    // timer to decrease the ref count. It assumes that the current JavaScript
    // execution finishs before firing the timer.
    // See issue 1218756 and 914430.
    m_frame->keepAlive();

    result = function->Call(receiver, argc, args);
  }

  if (v8::V8::IsDead())
    HandleFatalErrorInV8();

  return result;
}


v8::Local<v8::Function> V8Proxy::GetConstructor(V8ClassIndex::V8WrapperType t)
{
    ASSERT(ContextInitialized());
    v8::Local<v8::Value> cached =
        m_dom_constructor_cache->Get(v8::Integer::New(V8ClassIndex::ToInt(t)));
    if (cached->IsFunction()) {
        return v8::Local<v8::Function>::Cast(cached);
    }

    // Not in cache.
    {
        // Enter the context of the proxy to make sure that the
        // function is constructed in the context corresponding to
        // this proxy.
        v8::Context::Scope scope(m_context);
        v8::Handle<v8::FunctionTemplate> templ = GetTemplate(t);
        // Getting the function might fail if we're running out of
        // stack or memory.
        v8::TryCatch try_catch;
        v8::Local<v8::Function> value = templ->GetFunction();
        if (value.IsEmpty())
            return v8::Local<v8::Function>();
        m_dom_constructor_cache->Set(v8::Integer::New(t), value);
        // Hotmail fix, see comments in v8_proxy.h above
        // m_dom_constructor_cache.
        value->Set(v8::String::New("__proto__"), m_object_prototype);
        return value;
    }
}


// Get the string 'toString'.
static v8::Persistent<v8::String> GetToStringName() {
  static v8::Persistent<v8::String> value;
  if (value.IsEmpty())
    value = v8::Persistent<v8::String>::New(v8::String::New("toString"));
  return value;
}


static v8::Handle<v8::Value> ConstructorToString(const v8::Arguments& args) {
  // The DOM constructors' toString functions grab the current toString
  // for Functions by taking the toString function of itself and then
  // calling it with the constructor as its receiver.  This means that
  // changes to the Function prototype chain or toString function are
  // reflected when printing DOM constructors.  The only wart is that
  // changes to a DOM constructor's toString's toString will cause the
  // toString of the DOM constructor itself to change.  This is extremely
  // obscure and unlikely to be a problem.
  v8::Handle<v8::Value> val = args.Callee()->Get(GetToStringName());
  if (!val->IsFunction()) return v8::String::New("");
  return v8::Handle<v8::Function>::Cast(val)->Call(args.This(), 0, NULL);
}


v8::Persistent<v8::FunctionTemplate> V8Proxy::GetTemplate(
    V8ClassIndex::V8WrapperType type)
{
  v8::Persistent<v8::FunctionTemplate>* cache_cell =
      V8ClassIndex::GetCache(type);
  if (!(*cache_cell).IsEmpty())
    return *cache_cell;

  // not found
  FunctionTemplateFactory factory = V8ClassIndex::GetFactory(type);
  v8::Persistent<v8::FunctionTemplate> desc = factory();
  // DOM constructors are functions and should print themselves as such.
  // However, we will later replace their prototypes with Object
  // prototypes so we need to explicitly override toString on the
  // instance itself.  If we later make DOM constructors full objects
  // we can give them class names instead and Object.prototype.toString
  // will work so we can remove this code.
  static v8::Persistent<v8::FunctionTemplate> to_string_template;
  if (to_string_template.IsEmpty()) {
    to_string_template = v8::Persistent<v8::FunctionTemplate>::New(
        v8::FunctionTemplate::New(ConstructorToString));
  }
  desc->Set(GetToStringName(), to_string_template);
  switch (type) {
    case V8ClassIndex::CSSSTYLEDECLARATION:
      // The named property handler for style declarations has a
      // setter.  Therefore, the interceptor has to be on the object
      // itself and not on the prototype object.
      desc->InstanceTemplate()->SetNamedPropertyHandler(
          USE_NAMED_PROPERTY_GETTER(CSSStyleDeclaration),
          USE_NAMED_PROPERTY_SETTER(CSSStyleDeclaration));
      SetCollectionStringOrNullIndexedGetter<CSSStyleDeclaration>(desc);
      break;
    case V8ClassIndex::CSSRULELIST:
      SetCollectionIndexedGetter<CSSRuleList, CSSRule>(desc, 
                                                       V8ClassIndex::CSSRULE);
      break;
    case V8ClassIndex::CSSVALUELIST:
      SetCollectionIndexedGetter<CSSValueList, CSSValue>(
          desc,
          V8ClassIndex::CSSVALUE);
      break;
    case V8ClassIndex::CSSVARIABLESDECLARATION:
      SetCollectionStringOrNullIndexedGetter<CSSVariablesDeclaration>(desc);
      break;
    case V8ClassIndex::WEBKITCSSTRANSFORMVALUE:
      SetCollectionIndexedGetter<WebKitCSSTransformValue, CSSValue>(
          desc,
          V8ClassIndex::CSSVALUE);
      break;
    case V8ClassIndex::UNDETECTABLEHTMLCOLLECTION:
      desc->InstanceTemplate()->MarkAsUndetectable();  // fall through
    case V8ClassIndex::HTMLCOLLECTION:
      desc->InstanceTemplate()->SetNamedPropertyHandler(
          USE_NAMED_PROPERTY_GETTER(HTMLCollection));
      desc->InstanceTemplate()->SetCallAsFunctionHandler(
          USE_CALLBACK(HTMLCollectionCallAsFunction));
      SetCollectionIndexedGetter<HTMLCollection, Node>(desc, 
                                                       V8ClassIndex::NODE);
      break;
    case V8ClassIndex::HTMLOPTIONSCOLLECTION:
      SetCollectionNamedGetter<HTMLOptionsCollection, Node>(
          desc, 
          V8ClassIndex::NODE);
      desc->InstanceTemplate()->SetIndexedPropertyHandler(
          USE_INDEXED_PROPERTY_GETTER(HTMLOptionsCollection),
          USE_INDEXED_PROPERTY_SETTER(HTMLOptionsCollection));
      desc->InstanceTemplate()->SetCallAsFunctionHandler(
          USE_CALLBACK(HTMLCollectionCallAsFunction));
      break;
    case V8ClassIndex::HTMLSELECTELEMENT:
      desc->InstanceTemplate()->SetNamedPropertyHandler(
          NodeCollectionNamedPropertyGetter<HTMLSelectElement>,
          0,
          0,
          0,
          0,
          v8::Integer::New(V8ClassIndex::NODE));
      desc->InstanceTemplate()->SetIndexedPropertyHandler(
          NodeCollectionIndexedPropertyGetter<HTMLSelectElement>,
          USE_INDEXED_PROPERTY_SETTER(HTMLSelectElementCollection),
          0,
          0,
          NodeCollectionIndexedPropertyEnumerator<HTMLSelectElement>,
          v8::Integer::New(V8ClassIndex::NODE));
      break;
    case V8ClassIndex::HTMLDOCUMENT: {
      desc->InstanceTemplate()->SetNamedPropertyHandler(
          USE_NAMED_PROPERTY_GETTER(HTMLDocument),
          USE_NAMED_PROPERTY_SETTER(HTMLDocument),
          0,
          USE_NAMED_PROPERTY_DELETER(HTMLDocument));

      // We add an extra internal field to all Document wrappers for
      // storing a per document DOMImplementation wrapper.
      //
      // Additionally, we add two extra internal fields for
      // HTMLDocuments to implement temporary shadowing of
      // document.all.  One field holds an object that is used as a
      // marker.  The other field holds the marker object if
      // document.all is not shadowed and some other value if
      // document.all is shadowed.
      v8::Local<v8::ObjectTemplate> instance_template =
        desc->InstanceTemplate();
      ASSERT(instance_template->InternalFieldCount() ==
             V8Custom::kDefaultWrapperInternalFieldCount);
      instance_template->SetInternalFieldCount(
          V8Custom::kHTMLDocumentInternalFieldCount);
      break;
    }
#if ENABLE(SVG)
    case V8ClassIndex::SVGDOCUMENT:  // fall through
#endif
    case V8ClassIndex::DOCUMENT: {
      // We add an extra internal field to all Document wrappers for
      // storing a per document DOMImplementation wrapper.
      v8::Local<v8::ObjectTemplate> instance_template =
        desc->InstanceTemplate();
      ASSERT(instance_template->InternalFieldCount() ==
             V8Custom::kDefaultWrapperInternalFieldCount);
      instance_template->SetInternalFieldCount(
          V8Custom::kDocumentMinimumInternalFieldCount);
      break;
    }
    case V8ClassIndex::HTMLAPPLETELEMENT:  // fall through
    case V8ClassIndex::HTMLEMBEDELEMENT:  // fall through
    case V8ClassIndex::HTMLOBJECTELEMENT:
      // HTMLAppletElement, HTMLEmbedElement and HTMLObjectElement are
      // inherited from HTMLPlugInElement, and they share the same property
      // handling code.
      desc->InstanceTemplate()->SetNamedPropertyHandler(
          USE_NAMED_PROPERTY_GETTER(HTMLPlugInElement),
          USE_NAMED_PROPERTY_SETTER(HTMLPlugInElement));
      desc->InstanceTemplate()->SetIndexedPropertyHandler(
          USE_INDEXED_PROPERTY_GETTER(HTMLPlugInElement),
          USE_INDEXED_PROPERTY_SETTER(HTMLPlugInElement));
      desc->InstanceTemplate()->SetCallAsFunctionHandler(
          USE_CALLBACK(HTMLPlugInElement));
      break;
    case V8ClassIndex::HTMLFRAMESETELEMENT:
      desc->InstanceTemplate()->SetNamedPropertyHandler(
          USE_NAMED_PROPERTY_GETTER(HTMLFrameSetElement));
      break;
    case V8ClassIndex::HTMLFORMELEMENT:
      desc->InstanceTemplate()->SetNamedPropertyHandler(
          USE_NAMED_PROPERTY_GETTER(HTMLFormElement));
      desc->InstanceTemplate()->SetIndexedPropertyHandler(
          USE_INDEXED_PROPERTY_GETTER(HTMLFormElement),
          0,
          0,
          0,
          NodeCollectionIndexedPropertyEnumerator<HTMLFormElement>,
          v8::Integer::New(V8ClassIndex::NODE));
      break;
    case V8ClassIndex::STYLESHEET:  // fall through
    case V8ClassIndex::CSSSTYLESHEET: {
      // We add an extra internal field to hold a reference to
      // the owner node. 
      v8::Local<v8::ObjectTemplate> instance_template =
        desc->InstanceTemplate();
      ASSERT(instance_template->InternalFieldCount() ==
             V8Custom::kDefaultWrapperInternalFieldCount);
      instance_template->SetInternalFieldCount(
          V8Custom::kStyleSheetInternalFieldCount);
      break;
    }
    case V8ClassIndex::MEDIALIST:
      SetCollectionStringOrNullIndexedGetter<MediaList>(desc);
      break;
    case V8ClassIndex::MIMETYPEARRAY:
      SetCollectionIndexedAndNamedGetters<MimeTypeArray, MimeType>(
          desc,
          V8ClassIndex::MIMETYPE);
      break;
    case V8ClassIndex::NAMEDNODEMAP:
      desc->InstanceTemplate()->SetNamedPropertyHandler(
          USE_NAMED_PROPERTY_GETTER(NamedNodeMap));
      desc->InstanceTemplate()->SetIndexedPropertyHandler(
          USE_INDEXED_PROPERTY_GETTER(NamedNodeMap),
          0,
          0,
          0,
          CollectionIndexedPropertyEnumerator<NamedNodeMap>,
          v8::Integer::New(V8ClassIndex::NODE));
      break;
    case V8ClassIndex::NODELIST:
      SetCollectionIndexedGetter<NodeList, Node>(desc, V8ClassIndex::NODE);
      desc->InstanceTemplate()->SetNamedPropertyHandler(
          USE_NAMED_PROPERTY_GETTER(NodeList));
      break;
    case V8ClassIndex::PLUGIN:
      SetCollectionIndexedAndNamedGetters<Plugin, MimeType>(
          desc,
          V8ClassIndex::MIMETYPE);
      break;
    case V8ClassIndex::PLUGINARRAY:
      SetCollectionIndexedAndNamedGetters<PluginArray, Plugin>(
          desc,
          V8ClassIndex::PLUGIN);
      break;
    case V8ClassIndex::STYLESHEETLIST:
      desc->InstanceTemplate()->SetNamedPropertyHandler(
          USE_NAMED_PROPERTY_GETTER(StyleSheetList));
      SetCollectionIndexedGetter<StyleSheetList, StyleSheet>(
          desc,
          V8ClassIndex::STYLESHEET);
      break;
    case V8ClassIndex::DOMWINDOW: {
      v8::Local<v8::Signature> default_signature = v8::Signature::New(desc);

      desc->PrototypeTemplate()->SetNamedPropertyHandler(
          USE_NAMED_PROPERTY_GETTER(DOMWindow));
      desc->PrototypeTemplate()->SetIndexedPropertyHandler(
          USE_INDEXED_PROPERTY_GETTER(DOMWindow));

      desc->SetHiddenPrototype(true);

      // Reserve spaces for references to location and navigator objects.
      v8::Local<v8::ObjectTemplate> instance_template =
          desc->InstanceTemplate();
      instance_template->SetInternalFieldCount(
          V8Custom::kDOMWindowInternalFieldCount);

      // Set access check callbacks, but turned off initially.
      // When a context is detached from a frame, turn on the access check.
      // Turning on checks also invalidates inline caches of the object.
      instance_template->SetAccessCheckCallbacks(
          V8Custom::v8DOMWindowNamedSecurityCheck,
          V8Custom::v8DOMWindowIndexedSecurityCheck,
          v8::Integer::New(V8ClassIndex::DOMWINDOW),
          false);
      break;
    }
    case V8ClassIndex::LOCATION: {
      break;
    }
    case V8ClassIndex::HISTORY: {
      break;
    }

    case V8ClassIndex::MESSAGECHANNEL: {
        // Reserve two more internal fields for referencing the port1
        // and port2 wrappers.  This ensures that the port wrappers are
        // kept alive when the channel wrapper is.
        desc->SetCallHandler(USE_CALLBACK(MessageChannelConstructor));
        v8::Local<v8::ObjectTemplate> instance_template =
          desc->InstanceTemplate();
        instance_template->SetInternalFieldCount(
            V8Custom::kMessageChannelInternalFieldCount);
        break;
    }

    case V8ClassIndex::MESSAGEPORT: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instance_template =
            desc->InstanceTemplate();
        instance_template->SetInternalFieldCount(
            V8Custom::kMessagePortInternalFieldCount);
        break;
    }

    // DOMParser, XMLSerializer, and XMLHttpRequest objects are created from
    // JS world, but we setup the constructor function lazily in
    // WindowNamedPropertyHandler::get.
    case V8ClassIndex::DOMPARSER:
      desc->SetCallHandler(USE_CALLBACK(DOMParserConstructor));
      break;
    case V8ClassIndex::XMLSERIALIZER:
      desc->SetCallHandler(USE_CALLBACK(XMLSerializerConstructor));
      break;
    case V8ClassIndex::XMLHTTPREQUEST: {
      // Reserve one more internal field for keeping event listeners.
      v8::Local<v8::ObjectTemplate> instance_template =
          desc->InstanceTemplate();
      instance_template->SetInternalFieldCount(
          V8Custom::kXMLHttpRequestInternalFieldCount);
      desc->SetCallHandler(USE_CALLBACK(XMLHttpRequestConstructor));
      break;
    }
    case V8ClassIndex::XMLHTTPREQUESTUPLOAD: {
      // Reserve one more internal field for keeping event listeners.
      v8::Local<v8::ObjectTemplate> instance_template =
          desc->InstanceTemplate();
      instance_template->SetInternalFieldCount(
          V8Custom::kXMLHttpRequestInternalFieldCount);
      break;
    }
    case V8ClassIndex::XPATHEVALUATOR:
      desc->SetCallHandler(USE_CALLBACK(XPathEvaluatorConstructor));
      break;
    case V8ClassIndex::XSLTPROCESSOR:
      desc->SetCallHandler(USE_CALLBACK(XSLTProcessorConstructor));
      break;
    default:
      break;
  }

  *cache_cell = desc;
  return desc;
}


bool V8Proxy::ContextInitialized()
{
    // m_context, m_global, m_object_prototype, and
    // m_dom_constructor_cache should all be non-empty if m_context is
    // non-empty.
    ASSERT(m_context.IsEmpty() || !m_global.IsEmpty());
    ASSERT(m_context.IsEmpty() || !m_object_prototype.IsEmpty());
    ASSERT(m_context.IsEmpty() || !m_dom_constructor_cache.IsEmpty());
    return !m_context.IsEmpty();
}


DOMWindow* V8Proxy::retrieveWindow()
{
    // TODO: This seems very fragile. How do we know that the global object
    // from the current context is something sensible? Do we need to use the
    // last entered here? Who calls this?
    return retrieveWindow(v8::Context::GetCurrent());
}


DOMWindow* V8Proxy::retrieveWindow(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    global = LookupDOMWrapper(V8ClassIndex::DOMWINDOW, global);
    ASSERT(!global.IsEmpty());
    return ToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, global);
}


Frame* V8Proxy::retrieveFrame(v8::Handle<v8::Context> context)
{
    return retrieveWindow(context)->frame();
}


Frame* V8Proxy::retrieveActiveFrame()
{
    v8::Handle<v8::Context> context = v8::Context::GetEntered();
    if (context.IsEmpty()) 
        return 0;
    return retrieveFrame(context);
}


Frame* V8Proxy::retrieveFrame()
{
    DOMWindow* window = retrieveWindow();
    return window ? window->frame() : 0;
}


V8Proxy* V8Proxy::retrieve()
{
    DOMWindow* window = retrieveWindow();
    ASSERT(window);
    return retrieve(window->frame());
}

V8Proxy* V8Proxy::retrieve(Frame* frame)
{
    if (!frame)
        return 0;
    return frame->script()->isEnabled() ? frame->script()->proxy() : 0;
}


V8Proxy* V8Proxy::retrieve(ScriptExecutionContext* context)
{
    if (!context->isDocument())
        return 0;
    return retrieve(static_cast<Document*>(context)->frame());
}


void V8Proxy::disconnectFrame()
{
    // disconnect all event listeners
    DisconnectEventListeners();

    // remove all timeouts
    if (m_frame->domWindow())
      m_frame->domWindow()->clearAllTimeouts();
}


bool V8Proxy::isEnabled()
{
    Settings* settings = m_frame->settings();
    if (!settings)
        return false;

    // In the common case, JavaScript is enabled and we're done.
    if (settings->isJavaScriptEnabled())
        return true;

    // If JavaScript has been disabled, we need to look at the frame to tell
    // whether this script came from the web or the embedder. Scripts from the 
    // embedder are safe to run, but scripts from the other sources are 
    // disallowed.
    Document* document = m_frame->document();
    if (!document)
        return false;

    SecurityOrigin* origin = document->securityOrigin();
    if (origin->protocol().isEmpty())
        return false;  // Uninitialized document

    if (origin->protocol() == "http" || origin->protocol() == "https")
        return false;  // Web site

    if (origin->protocol() == ChromiumBridge::uiResourceProtocol())
        return true;   // Embedder's scripts are ok to run

    // If the scheme is ftp: or file:, an empty file name indicates a directory
    // listing, which requires JavaScript to function properly.
    const char* kDirProtocols[] = { "ftp", "file" };
    for (size_t i = 0; i < arraysize(kDirProtocols); ++i) {
        if (origin->protocol() == kDirProtocols[i]) {
            const KURL& url = document->url();
            return url.pathAfterLastSlash() == url.pathEnd();
        }
    }

    return false;  // Other protocols fall through to here
}


void V8Proxy::UpdateDocumentWrapper(v8::Handle<v8::Value> wrapper) {
    ClearDocumentWrapper();

    ASSERT(m_document.IsEmpty());
    m_document = v8::Persistent<v8::Value>::New(wrapper);
#ifndef NDEBUG
    RegisterGlobalHandle(PROXY, this, m_document);
#endif
}


void V8Proxy::ClearDocumentWrapper()
{
    if (!m_document.IsEmpty()) {
#ifndef NDEBUG
        UnregisterGlobalHandle(this, m_document);
#endif
        m_document.Dispose();
        m_document.Clear();
    }
}


void V8Proxy::DisposeContextHandles() {
    if (!m_context.IsEmpty()) {
        m_context.Dispose();
        m_context.Clear();
    }

    if (!m_dom_constructor_cache.IsEmpty()) {
#ifndef NDEBUG
        UnregisterGlobalHandle(this, m_dom_constructor_cache);
#endif
        m_dom_constructor_cache.Dispose();
        m_dom_constructor_cache.Clear();
    }

    if (!m_object_prototype.IsEmpty()) {
#ifndef NDEBUG
        UnregisterGlobalHandle(this, m_object_prototype);
#endif
        m_object_prototype.Dispose();
        m_object_prototype.Clear();
    }
}

void V8Proxy::clearForClose()
{
    if (!m_context.IsEmpty()) {
        v8::HandleScope handle_scope;

        ClearDocumentWrapper();
        DisposeContextHandles();
    }
}


void V8Proxy::clearForNavigation()
{
    if (!m_context.IsEmpty()) {
        v8::HandleScope handle;
        ClearDocumentWrapper();

        v8::Context::Scope context_scope(m_context);

        // Turn on access check on the old DOMWindow wrapper.
        v8::Handle<v8::Object> wrapper =
            LookupDOMWrapper(V8ClassIndex::DOMWINDOW, m_global);
        ASSERT(!wrapper.IsEmpty());
        wrapper->TurnOnAccessCheck();

        // Clear all timeouts.
        DOMWindow* domWindow =
            ToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, wrapper);
        domWindow->clearAllTimeouts();

        // disconnect all event listeners
        DisconnectEventListeners();

        // Separate the context from its global object.
        m_context->DetachGlobal();

        DisposeContextHandles();

        // Reinitialize the context so the global object points to
        // the new DOM window.
        InitContextIfNeeded();
    }
}


void V8Proxy::SetSecurityToken() {
    Document* document = m_frame->document();
    // Setup security origin and security token
    if (!document) {
        m_context->UseDefaultSecurityToken();
        return;
    }

    // Ask the document's SecurityOrigin to generate a security token.
    // If two tokens are equal, then the SecurityOrigins canAccess each other.
    // If two tokens are not equal, then we have to call canAccess.
    // Note: we can't use the HTTPOrigin if it was set from the DOM.
    SecurityOrigin* origin = document->securityOrigin();
    String token;
    if (!origin->domainWasSetInDOM())
        token = document->securityOrigin()->toString();

    // An empty token means we always have to call canAccess.  In this case, we
    // use the global object as the security token to avoid calling canAccess
    // when a script accesses its own objects.
    if (token.isEmpty()) {
        m_context->UseDefaultSecurityToken();
        return;
    }

    CString utf8_token = token.utf8();
    // NOTE: V8 does identity comparison in fast path, must use a symbol
    // as the security token.
    m_context->SetSecurityToken(
        v8::String::NewSymbol(utf8_token.data(), utf8_token.length()));
}


void V8Proxy::updateDocument()
{
    if (!m_frame->document())
        return;

    if (m_global.IsEmpty()) {
        ASSERT(m_context.IsEmpty());
        return;
    }

    {
        v8::HandleScope scope;
        SetSecurityToken();
    }
}

void V8Proxy::updateSecurityOrigin()
{
    v8::HandleScope scope;
    SetSecurityToken();
}

// Same origin policy implementation:
//
// Same origin policy prevents JS code from domain A access JS & DOM objects
// in a different domain B. There are exceptions and several objects are
// accessible by cross-domain code. For example, the window.frames object is
// accessible by code from a different domain, but window.document is not.
//
// The binding code sets security check callbacks on a function template,
// and accessing instances of the template calls the callback function.
// The callback function checks same origin policy.
//
// Callback functions are expensive. V8 uses a security token string to do
// fast access checks for the common case where source and target are in the
// same domain. A security token is a string object that represents
// the protocol/url/port of a domain.
//
// There are special cases where a security token matching is not enough.
// For example, JavaScript can set its domain to a super domain by calling
// document.setDomain(...). In these cases, the binding code can reset
// a context's security token to its global object so that the fast access
// check will always fail.

// Check if the current execution context can access a target frame.
// First it checks same domain policy using the lexical context
//
// This is equivalent to KJS::Window::allowsAccessFrom(ExecState*, String&).
bool V8Proxy::CanAccessPrivate(DOMWindow* target_window)
{
    ASSERT(target_window);

    String message;

    DOMWindow* origin_window = retrieveWindow();
    if (origin_window == target_window)
        return true;

    if (!origin_window)
        return false;

    // JS may be attempting to access the "window" object, which should be
    // valid, even if the document hasn't been constructed yet.
    // If the document doesn't exist yet allow JS to access the window object.
    if (!origin_window->document())
        return true;

    const SecurityOrigin* active_security_origin = origin_window->securityOrigin();
    const SecurityOrigin* target_security_origin = target_window->securityOrigin();

    String ui_resource_protocol = ChromiumBridge::uiResourceProtocol();
    if (active_security_origin->protocol() == ui_resource_protocol) {
        KURL inspector_url = ChromiumBridge::inspectorURL();
        ASSERT(inspector_url.protocol() == ui_resource_protocol);

        // The Inspector can access anything.
        if (active_security_origin->host() == inspector_url.host())
            return true;

        // To mitigate XSS vulnerabilities on the browser itself, UI resources
        // besides the Inspector can't access other documents.
        return false;
    }

    if (active_security_origin->canAccess(target_security_origin))
        return true;

    // Allow access to a "about:blank" page if the dynamic context is a
    // detached context of the same frame as the blank page.
    if (target_security_origin->isEmpty() &&
        origin_window->frame() == target_window->frame())
        return true;

    return false;
}


bool V8Proxy::CanAccessFrame(Frame* target, bool report_error)
{
    // The subject is detached from a frame, deny accesses.
    if (!target)
        return false;

    if (!CanAccessPrivate(target->domWindow())) {
        if (report_error)
            ReportUnsafeAccessTo(target, REPORT_NOW);
        return false;
    }
    return true;
}


bool V8Proxy::CheckNodeSecurity(Node* node)
{
    if (!node)
        return false;

    Frame* target = node->document()->frame();

    if (!target)
        return false;

    return CanAccessFrame(target, true);
}


// Create a new environment and setup the global object.
//
// The global object corresponds to a DOMWindow instance.  However, to
// allow properties of the JS DOMWindow instance to be shadowed, we
// use a shadow object as the global object and use the JS DOMWindow
// instance as the prototype for that shadow object.  The JS DOMWindow
// instance is undetectable from javascript code because the __proto__
// accessors skip that object.
//
// The shadow object and the DOMWindow instance are seen as one object
// from javascript.  The javascript object that corresponds to a
// DOMWindow instance is the shadow object.  When mapping a DOMWindow
// instance to a V8 object, we return the shadow object.
//
// To implement split-window, see
//   1) https://bugs.webkit.org/show_bug.cgi?id=17249
//   2) https://wiki.mozilla.org/Gecko:SplitWindow
//   3) https://bugzilla.mozilla.org/show_bug.cgi?id=296639
// we need to split the shadow object further into two objects:
// an outer window and an inner window. The inner window is the hidden
// prototype of the outer window. The inner window is the default
// global object of the context. A variable declared in the global
// scope is a property of the inner window.
//
// The outer window sticks to a Frame, it is exposed to JavaScript
// via window.window, window.self, window.parent, etc. The outer window
// has a security token which is the domain. The outer window cannot
// have its own properties. window.foo = 'x' is delegated to the
// inner window.
//
// When a frame navigates to a new page, the inner window is cut off
// the outer window, and the outer window identify is preserved for
// the frame. However, a new inner window is created for the new page.
// If there are JS code holds a closure to the old inner window,
// it won't be able to reach the outer window via its global object. 
void V8Proxy::InitContextIfNeeded()
{
  // Bail out if the context has already been initialized.
  if (!m_context.IsEmpty())
      return;

  // Install counters handler with V8.
  static bool v8_counters_initialized = false;
  if (!v8_counters_initialized) {
    ChromiumBridge::initV8CounterFunction();
    v8_counters_initialized = true;
  }

  // Setup the security handlers and message listener.  This only has
  // to be done once.
  static bool v8_initialized = false;
  if (!v8_initialized) {
    // Tells V8 not to call the default OOM handler, binding code
    // will handle it.
    v8::V8::IgnoreOutOfMemoryException();
    v8::V8::SetFatalErrorHandler(ReportFatalErrorInV8);

    v8::V8::SetGlobalGCPrologueCallback(&GCPrologue);
    v8::V8::SetGlobalGCEpilogueCallback(&GCEpilogue);

    v8::V8::AddMessageListener(HandleConsoleMessage);

    v8::V8::SetFailedAccessCheckCallbackFunction(ReportUnsafeJavaScriptAccess);

    v8_initialized = true;
  }

  // Create a new environment using an empty template for the shadow
  // object.  Reuse the global object if one has been created earlier.
  v8::Persistent<v8::ObjectTemplate> global_template =
    V8DOMWindow::GetShadowObjectTemplate();
  if (global_template.IsEmpty())
    return;

  // Install a security handler with V8.
  global_template->SetAccessCheckCallbacks(
      V8Custom::v8DOMWindowNamedSecurityCheck,
      V8Custom::v8DOMWindowIndexedSecurityCheck,
      v8::Integer::New(V8ClassIndex::DOMWINDOW));

  if (ScriptController::shouldExposeGCController()) {
      v8::RegisterExtension(new v8::Extension("v8/GCController",
          "(function v8_GCController() {"
          "   var v8_gc;"
          "   if (gc) v8_gc = gc;"
          "   GCController = new Object();"
          "   GCController.collect ="
          "     function() {if (v8_gc) v8_gc(); };"
          " })()"));
      const char* extension_names[] = { "v8/GCController" };
      v8::ExtensionConfiguration extensions(1, extension_names);
      // Create a new context.
      m_context = v8::Context::New(&extensions, global_template, m_global);
  } else {
      m_context = v8::Context::New(NULL, global_template, m_global);      
  }

  if (m_context.IsEmpty())
    return;

  // Starting from now, use local context only.
  v8::Local<v8::Context> context = GetContext();
  v8::Context::Scope scope(context);

  // Store the first global object created so we can reuse it.
  if (m_global.IsEmpty()) {
    m_global = v8::Persistent<v8::Object>::New(context->Global());
    // Bail out if allocation of the first global objects fails.
    if (m_global.IsEmpty()) {
      DisposeContextHandles();
      return;
    }
#ifndef NDEBUG
    RegisterGlobalHandle(PROXY, this, m_global);
#endif
  }

  // Allocate strings used during initialization.
  v8::Handle<v8::String> object_string = v8::String::New("Object");
  v8::Handle<v8::String> prototype_string = v8::String::New("prototype");
  v8::Handle<v8::String> implicit_proto_string = v8::String::New("__proto__");
  // Bail out if allocation failed.
  if (object_string.IsEmpty() ||
      prototype_string.IsEmpty() ||
      implicit_proto_string.IsEmpty()) {
    DisposeContextHandles();
    return;
  }

  // Allocate DOM constructor cache.
  v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(
      m_global->Get(object_string));
  m_object_prototype = v8::Persistent<v8::Value>::New(
      object->Get(prototype_string));
  m_dom_constructor_cache = v8::Persistent<v8::Array>::New(
      v8::Array::New(V8ClassIndex::WRAPPER_TYPE_COUNT));
  // Bail out if allocation failed.
  if (m_object_prototype.IsEmpty() || m_dom_constructor_cache.IsEmpty()) {
    DisposeContextHandles();
    return;
  }
#ifndef NDEBUG
  RegisterGlobalHandle(PROXY, this, m_object_prototype);
  RegisterGlobalHandle(PROXY, this, m_dom_constructor_cache);
#endif

  // Create a new JS window object and use it as the prototype for the
  // shadow global object.
  v8::Handle<v8::Function> window_constructor =
      GetConstructor(V8ClassIndex::DOMWINDOW);
  v8::Local<v8::Object> js_window =
      SafeAllocation::NewInstance(window_constructor);
  // Bail out if allocation failed.
  if (js_window.IsEmpty()) {
    DisposeContextHandles();
    return;
  }

  DOMWindow* window = m_frame->domWindow();

  // Wrap the window.
  SetDOMWrapper(js_window,
                V8ClassIndex::ToInt(V8ClassIndex::DOMWINDOW),
                window);

  window->ref();
  V8Proxy::SetJSWrapperForDOMObject(window,
      v8::Persistent<v8::Object>::New(js_window));

  // Insert the window instance as the prototype of the shadow object.
  v8::Handle<v8::Object> v8_global = context->Global();
  v8_global->Set(implicit_proto_string, js_window);

  SetSecurityToken();

  m_frame->loader()->dispatchWindowObjectAvailable();

  if (ScriptController::RecordPlaybackMode()) {
    // Inject code which overrides a few common JS functions for implementing
    // randomness.  In order to implement effective record & playback of
    // websites, it is important that the URLs not change.  Many popular web
    // based apps use randomness in URLs to unique-ify urls for proxies.
    // Unfortunately, this breaks playback.
    // To work around this, we take the two most common client-side randomness
    // generators and make them constant.  They really need to be constant
    // (rather than a constant seed followed by constant change)
    // because the playback mode wants flexibility in how it plays them back
    // and cannot always guarantee that requests for randomness are played back
    // in exactly the same order in which they were recorded.
    String script(
        "Math.random = function() { return 0.5; };"
        "__ORIGDATE__ = Date;"
        "Date.__proto__.now = function() { "
        "    return new __ORIGDATE__(1204251968254); };"
        "Date = function() { return Date.now(); };");
    this->Evaluate(String(), 0, script, 0);
  }
}


void V8Proxy::SetDOMException(int exception_code)
{
  if (exception_code <= 0)
      return;

  ExceptionCodeDescription description;
  getExceptionCodeDescription(exception_code, description);

  v8::Handle<v8::Value> exception;
  switch (description.type) {
    case DOMExceptionType:
      exception = ToV8Object(V8ClassIndex::DOMCOREEXCEPTION,
                             DOMCoreException::create(description));
      break;
    case RangeExceptionType:
      exception = ToV8Object(V8ClassIndex::RANGEEXCEPTION,
                             RangeException::create(description));
      break;
    case EventExceptionType:
      exception = ToV8Object(V8ClassIndex::EVENTEXCEPTION,
                             EventException::create(description));
      break;
    case XMLHttpRequestExceptionType:
      exception = ToV8Object(V8ClassIndex::XMLHTTPREQUESTEXCEPTION,
                             XMLHttpRequestException::create(description));
      break;
#if ENABLE(SVG)
    case SVGExceptionType:
      exception = ToV8Object(V8ClassIndex::SVGEXCEPTION,
                             SVGException::create(description));
      break;
#endif
#if ENABLE(XPATH)
    case XPathExceptionType:
      exception = ToV8Object(V8ClassIndex::XPATHEXCEPTION,
                             XPathException::create(description));
      break;
#endif
  }

  ASSERT(!exception.IsEmpty());
  v8::ThrowException(exception);
}

v8::Handle<v8::Value> V8Proxy::ThrowError(ErrorType type, const char* message)
{
  switch (type) {
    case RANGE_ERROR:
      return v8::ThrowException(v8::Exception::RangeError(v8String(message)));
    case REFERENCE_ERROR:
      return v8::ThrowException(
          v8::Exception::ReferenceError(v8String(message)));
    case SYNTAX_ERROR:
      return v8::ThrowException(v8::Exception::SyntaxError(v8String(message)));
    case TYPE_ERROR:
      return v8::ThrowException(v8::Exception::TypeError(v8String(message)));
    case GENERAL_ERROR:
      return v8::ThrowException(v8::Exception::Error(v8String(message)));
    default:
      ASSERT(false);
      return v8::Handle<v8::Value>();
  }
}

v8::Local<v8::Context> V8Proxy::GetContext(Frame* frame)
{
    V8Proxy* proxy = retrieve(frame);
    if (!proxy)
        return v8::Local<v8::Context>();

    proxy->InitContextIfNeeded();
    return proxy->GetContext();
}

v8::Local<v8::Context> V8Proxy::GetCurrentContext()
{
    return v8::Context::GetCurrent();
}

v8::Handle<v8::Value> V8Proxy::ToV8Object(V8ClassIndex::V8WrapperType type, void* imp)
{
  ASSERT(type != V8ClassIndex::EVENTLISTENER);
  ASSERT(type != V8ClassIndex::EVENTTARGET);
  ASSERT(type != V8ClassIndex::EVENT);

  bool is_active_dom_object = false;
  switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE:
    DOM_NODE_TYPES(MAKE_CASE)
#if ENABLE(SVG)
    SVG_NODE_TYPES(MAKE_CASE)
#endif
      return NodeToV8Object(static_cast<Node*>(imp));
    case V8ClassIndex::CSSVALUE:
      return CSSValueToV8Object(static_cast<CSSValue*>(imp));
    case V8ClassIndex::CSSRULE:
      return CSSRuleToV8Object(static_cast<CSSRule*>(imp));
    case V8ClassIndex::STYLESHEET:
      return StyleSheetToV8Object(static_cast<StyleSheet*>(imp));
    case V8ClassIndex::DOMWINDOW:
      return WindowToV8Object(static_cast<DOMWindow*>(imp));
#if ENABLE(SVG)
    SVG_NONNODE_TYPES(MAKE_CASE)
      if (type == V8ClassIndex::SVGELEMENTINSTANCE)
        return SVGElementInstanceToV8Object(static_cast<SVGElementInstance*>(imp));
      return SVGObjectWithContextToV8Object(type, imp);
#endif

    ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
      is_active_dom_object = true;
      break;
    default:
      break;
  }

#undef MAKE_CASE

  if (!imp) return v8::Null();

  // Non DOM node
  v8::Persistent<v8::Object> result = is_active_dom_object ?
                                      active_dom_object_map().get(imp) :
                                      dom_object_map().get(imp);
  if (result.IsEmpty()) {
    v8::Local<v8::Object> v8obj = InstantiateV8Object(type, type, imp);
    if (!v8obj.IsEmpty()) {
      // Go through big switch statement, it has some duplications
      // that were handled by code above (such as CSSVALUE, CSSRULE, etc).
      switch (type) {
#define MAKE_CASE(TYPE, NAME) \
        case V8ClassIndex::TYPE: static_cast<NAME*>(imp)->ref(); break;
        DOM_OBJECT_TYPES(MAKE_CASE)
#undef MAKE_CASE
      default:
        ASSERT(false);
      }
      result = v8::Persistent<v8::Object>::New(v8obj);
      if (is_active_dom_object)
        SetJSWrapperForActiveDOMObject(imp, result);
      else
        SetJSWrapperForDOMObject(imp, result);

      // Special case for Location and Navigator. Both Safari and FF let
      // Location and Navigator JS wrappers survive GC. To mimic their
      // behaviors, V8 creates hidden references from the DOMWindow to
      // location and navigator objects. These references get cleared
      // when the DOMWindow is reused by a new page.
      if (type == V8ClassIndex::LOCATION) {
        SetHiddenWindowReference(static_cast<Location*>(imp)->frame(),
          V8Custom::kDOMWindowLocationIndex, result);
      } else if (type == V8ClassIndex::NAVIGATOR) {
        SetHiddenWindowReference(static_cast<Navigator*>(imp)->frame(),
          V8Custom::kDOMWindowNavigatorIndex, result);
      }
    }
  }
  return result;
}


void V8Proxy::SetHiddenWindowReference(Frame* frame,
                                       const int internal_index,
                                       v8::Handle<v8::Object> jsobj)
{
    // Get DOMWindow
    if (!frame) return;  // Object might be detached from window
    v8::Handle<v8::Context> context = GetContext(frame);
    if (context.IsEmpty()) return;

    ASSERT(internal_index < V8Custom::kDOMWindowInternalFieldCount);

    v8::Handle<v8::Object> global = context->Global();
    // Look for real DOM wrapper.
    global = LookupDOMWrapper(V8ClassIndex::DOMWINDOW, global);
    ASSERT(!global.IsEmpty());
    ASSERT(global->GetInternalField(internal_index)->IsUndefined());
    global->SetInternalField(internal_index, jsobj);
}


V8ClassIndex::V8WrapperType V8Proxy::GetDOMWrapperType(v8::Handle<v8::Object> object)
{
  ASSERT(MaybeDOMWrapper(object));
  v8::Handle<v8::Value> type =
      object->GetInternalField(V8Custom::kDOMWrapperTypeIndex);
    return V8ClassIndex::FromInt(type->Int32Value());
}


void* V8Proxy::ToNativeObjectImpl(V8ClassIndex::V8WrapperType type,
                                  v8::Handle<v8::Value> object)
{
  // Native event listener is per frame, it cannot be handled
  // by this generic function.
  ASSERT(type != V8ClassIndex::EVENTLISTENER);
  ASSERT(type != V8ClassIndex::EVENTTARGET);

  ASSERT(MaybeDOMWrapper(object));

  switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE:
    DOM_NODE_TYPES(MAKE_CASE)
#if ENABLE(SVG)
    SVG_NODE_TYPES(MAKE_CASE)
#endif
      ASSERT(false);
      return NULL;
    case V8ClassIndex::XMLHTTPREQUEST:
      return DOMWrapperToNative<XMLHttpRequest>(object);
    case V8ClassIndex::EVENT:
      return DOMWrapperToNative<Event>(object);
    case V8ClassIndex::CSSRULE:
      return DOMWrapperToNative<CSSRule>(object);
    default:
      break;
  }
#undef MAKE_CASE

  return DOMWrapperToNative<void>(object);
}

v8::Handle<v8::Object> V8Proxy::LookupDOMWrapper(
    V8ClassIndex::V8WrapperType type, v8::Handle<v8::Value> value)
{
    if (value.IsEmpty())
        return v8::Handle<v8::Object>();

    v8::Handle<v8::FunctionTemplate> desc = V8Proxy::GetTemplate(type);
    while (value->IsObject()) {
        v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
        if (desc->HasInstance(object))
            return object;

        value = object->GetPrototype();
    }
    return v8::Handle<v8::Object>();
}


PassRefPtr<NodeFilter> V8Proxy::ToNativeNodeFilter(v8::Handle<v8::Value> filter)
{
    // A NodeFilter is used when walking through a DOM tree or iterating tree
    // nodes.
    // TODO: we may want to cache NodeFilterCondition and NodeFilter
    // object, but it is minor.
    // NodeFilter is passed to NodeIterator that has a ref counted pointer
    // to NodeFilter. NodeFilter has a ref counted pointer to NodeFilterCondition.
    // In NodeFilterCondition, filter object is persisted in its constructor,
    // and disposed in its destructor.
    if (!filter->IsFunction())
        return 0;

    NodeFilterCondition* cond = new V8NodeFilterCondition(filter);
    return NodeFilter::create(cond);
}


v8::Local<v8::Object> V8Proxy::InstantiateV8Object(
    V8ClassIndex::V8WrapperType desc_type,
    V8ClassIndex::V8WrapperType cptr_type,
    void* imp)
{
  // Make a special case for document.all
  if (desc_type == V8ClassIndex::HTMLCOLLECTION &&
      static_cast<HTMLCollection*>(imp)->type() == HTMLCollection::DocAll) {
    desc_type = V8ClassIndex::UNDETECTABLEHTMLCOLLECTION;
  }

  v8::Local<v8::Function> function;
  V8Proxy* proxy = V8Proxy::retrieve();
  if (proxy) {
    // Make sure that the context of the proxy has been initialized.
    proxy->InitContextIfNeeded();
    // Constructor is configured.
    function = proxy->GetConstructor(desc_type);
  } else {
    function = GetTemplate(desc_type)->GetFunction();
  }
  v8::Local<v8::Object> instance = SafeAllocation::NewInstance(function);
  if (!instance.IsEmpty()) {
    // Avoid setting the DOM wrapper for failed allocations.
    SetDOMWrapper(instance, V8ClassIndex::ToInt(cptr_type), imp);
  }
  return instance;
}

v8::Handle<v8::Value> V8Proxy::CheckNewLegal(const v8::Arguments& args)
{
    if (!AllowAllocation::m_current)
        return ThrowError(TYPE_ERROR, "Illegal constructor");

    return args.This();
}

void V8Proxy::SetDOMWrapper(v8::Handle<v8::Object> obj, int type, void* cptr)
{
  ASSERT(obj->InternalFieldCount() >= 2);
  obj->SetInternalField(V8Custom::kDOMWrapperObjectIndex, WrapCPointer(cptr));
  obj->SetInternalField(V8Custom::kDOMWrapperTypeIndex, v8::Integer::New(type));
}


#ifndef NDEBUG
bool V8Proxy::MaybeDOMWrapper(v8::Handle<v8::Value> value)
{
  if (value.IsEmpty() || !value->IsObject()) return false;

  v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(value);
  if (obj->InternalFieldCount() == 0) return false;

  ASSERT(obj->InternalFieldCount() >=
         V8Custom::kDefaultWrapperInternalFieldCount);

  v8::Handle<v8::Value> type =
      obj->GetInternalField(V8Custom::kDOMWrapperTypeIndex);
  ASSERT(type->IsInt32());
  ASSERT(V8ClassIndex::INVALID_CLASS_INDEX < type->Int32Value() &&
    type->Int32Value() < V8ClassIndex::CLASSINDEX_END);

  v8::Handle<v8::Value> wrapper =
      obj->GetInternalField(V8Custom::kDOMWrapperObjectIndex);
  ASSERT(wrapper->IsNumber() || wrapper->IsExternal());

  return true;
}
#endif


bool V8Proxy::IsDOMEventWrapper(v8::Handle<v8::Value> value)
{
  // All kinds of events use EVENT as dom type in JS wrappers.
  // See EventToV8Object
  return IsWrapperOfType(value, V8ClassIndex::EVENT);
}

bool V8Proxy::IsWrapperOfType(v8::Handle<v8::Value> value,
                              V8ClassIndex::V8WrapperType classType)
{
  if (value.IsEmpty() || !value->IsObject()) return false;

  v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(value);
  if (obj->InternalFieldCount() == 0) return false;

  ASSERT(obj->InternalFieldCount() >=
         V8Custom::kDefaultWrapperInternalFieldCount);

  v8::Handle<v8::Value> wrapper =
      obj->GetInternalField(V8Custom::kDOMWrapperObjectIndex);
  ASSERT(wrapper->IsNumber() || wrapper->IsExternal());

  v8::Handle<v8::Value> type =
    obj->GetInternalField(V8Custom::kDOMWrapperTypeIndex);
  ASSERT(type->IsInt32());
  ASSERT(V8ClassIndex::INVALID_CLASS_INDEX < type->Int32Value() &&
    type->Int32Value() < V8ClassIndex::CLASSINDEX_END);

  return V8ClassIndex::FromInt(type->Int32Value()) == classType;
}

#if ENABLE(VIDEO)
#define FOR_EACH_VIDEO_TAG(macro)                \
  macro(audio, AUDIO)                            \
  macro(source, SOURCE)                          \
  macro(video, VIDEO)
#else
#define FOR_EACH_VIDEO_TAG(macro)
#endif

#define FOR_EACH_TAG(macro)                      \
  macro(a, ANCHOR)                               \
  macro(applet, APPLET)                          \
  macro(area, AREA)                              \
  macro(base, BASE)                              \
  macro(basefont, BASEFONT)                      \
  macro(blockquote, BLOCKQUOTE)                  \
  macro(body, BODY)                              \
  macro(br, BR)                                  \
  macro(button, BUTTON)                          \
  macro(caption, TABLECAPTION)                   \
  macro(col, TABLECOL)                           \
  macro(colgroup, TABLECOL)                      \
  macro(del, MOD)                                \
  macro(canvas, CANVAS)                          \
  macro(dir, DIRECTORY)                          \
  macro(div, DIV)                                \
  macro(dl, DLIST)                               \
  macro(embed, EMBED)                            \
  macro(fieldset, FIELDSET)                      \
  macro(font, FONT)                              \
  macro(form, FORM)                              \
  macro(frame, FRAME)                            \
  macro(frameset, FRAMESET)                      \
  macro(h1, HEADING)                             \
  macro(h2, HEADING)                             \
  macro(h3, HEADING)                             \
  macro(h4, HEADING)                             \
  macro(h5, HEADING)                             \
  macro(h6, HEADING)                             \
  macro(head, HEAD)                              \
  macro(hr, HR)                                  \
  macro(html, HTML)                              \
  macro(img, IMAGE)                              \
  macro(iframe, IFRAME)                          \
  macro(image, IMAGE)                            \
  macro(input, INPUT)                            \
  macro(ins, MOD)                                \
  macro(isindex, ISINDEX)                        \
  macro(keygen, SELECT)                          \
  macro(label, LABEL)                            \
  macro(legend, LEGEND)                          \
  macro(li, LI)                                  \
  macro(link, LINK)                              \
  macro(listing, PRE)                            \
  macro(map, MAP)                                \
  macro(marquee, MARQUEE)                        \
  macro(menu, MENU)                              \
  macro(meta, META)                              \
  macro(object, OBJECT)                          \
  macro(ol, OLIST)                               \
  macro(optgroup, OPTGROUP)                      \
  macro(option, OPTION)                          \
  macro(p, PARAGRAPH)                            \
  macro(param, PARAM)                            \
  macro(pre, PRE)                                \
  macro(q, QUOTE)                                \
  macro(script, SCRIPT)                          \
  macro(select, SELECT)                          \
  macro(style, STYLE)                            \
  macro(table, TABLE)                            \
  macro(thead, TABLESECTION)                     \
  macro(tbody, TABLESECTION)                     \
  macro(tfoot, TABLESECTION)                     \
  macro(td, TABLECELL)                           \
  macro(th, TABLECELL)                           \
  macro(tr, TABLEROW)                            \
  macro(textarea, TEXTAREA)                      \
  macro(title, TITLE)                            \
  macro(ul, ULIST)                               \
  macro(xmp, PRE)

V8ClassIndex::V8WrapperType V8Proxy::GetHTMLElementType(HTMLElement* element)
{
  static HashMap<String, V8ClassIndex::V8WrapperType> map;
  if (map.isEmpty()) {
#define ADD_TO_HASH_MAP(tag, name) \
    map.set(#tag, V8ClassIndex::HTML##name##ELEMENT);
FOR_EACH_TAG(ADD_TO_HASH_MAP)
#if ENABLE(VIDEO)
    if (MediaPlayer::isAvailable()) {
FOR_EACH_VIDEO_TAG(ADD_TO_HASH_MAP)
    }
#endif
#undef ADD_TO_HASH_MAP
  }

  V8ClassIndex::V8WrapperType t = map.get(element->localName().impl());
  if (t == 0)
      return V8ClassIndex::HTMLELEMENT;
  return t;
}
#undef FOR_EACH_TAG

#if ENABLE(SVG)

#if ENABLE(SVG_ANIMATION)
#define FOR_EACH_ANIMATION_TAG(macro) \
    macro(animateColor, ANIMATECOLOR) \
    macro(animate, ANIMATE) \
    macro(animateTransform, ANIMATETRANSFORM) \
    macro(set, SET)
#else
#define FOR_EACH_ANIMATION_TAG(macro)
#endif

#if ENABLE(SVG_FILTERS)
#define FOR_EACH_FILTERS_TAG(macro) \
    macro(feBlend, FEBLEND) \
    macro(feColorMatrix, FECOLORMATRIX) \
    macro(feComponentTransfer, FECOMPONENTTRANSFER) \
    macro(feComposite, FECOMPOSITE) \
    macro(feDiffuseLighting, FEDIFFUSELIGHTING) \
    macro(feDisplacementMap, FEDISPLACEMENTMAP) \
    macro(feDistantLight, FEDISTANTLIGHT) \
    macro(feFlood, FEFLOOD) \
    macro(feFuncA, FEFUNCA) \
    macro(feFuncB, FEFUNCB) \
    macro(feFuncG, FEFUNCG) \
    macro(feFuncR, FEFUNCR) \
    macro(feGaussianBlur, FEGAUSSIANBLUR) \
    macro(feImage, FEIMAGE) \
    macro(feMerge, FEMERGE) \
    macro(feMergeNode, FEMERGENODE) \
    macro(feOffset, FEOFFSET) \
    macro(fePointLight, FEPOINTLIGHT) \
    macro(feSpecularLighting, FESPECULARLIGHTING) \
    macro(feSpotLight, FESPOTLIGHT) \
    macro(feTile, FETILE) \
    macro(feTurbulence, FETURBULENCE) \
    macro(filter, FILTER)
#else
#define FOR_EACH_FILTERS_TAG(macro)
#endif

#if ENABLE(SVG_FONTS)
#define FOR_EACH_FONTS_TAG(macro) \
    macro(definition-src, DEFINITIONSRC) \
    macro(font-face, FONTFACE) \
    macro(font-face-format, FONTFACEFORMAT) \
    macro(font-face-name, FONTFACENAME) \
    macro(font-face-src, FONTFACESRC) \
    macro(font-face-uri, FONTFACEURI)
#else
#define FOR_EACH_FONTS_TAG(marco)
#endif

#if ENABLE(SVG_FOREIGN_OBJECT)
#define FOR_EACH_FOREIGN_OBJECT_TAG(macro) \
    macro(foreignObject, FOREIGNOBJECT)
#else
#define FOR_EACH_FOREIGN_OBJECT_TAG(macro)
#endif

#if ENABLE(SVG_USE)
#define FOR_EACH_USE_TAG(macro) \
    macro(use, USE)
#else
#define FOR_EACH_USE_TAG(macro)
#endif

#define FOR_EACH_TAG(macro) \
    FOR_EACH_ANIMATION_TAG(macro) \
    FOR_EACH_FILTERS_TAG(macro) \
    FOR_EACH_FONTS_TAG(macro) \
    FOR_EACH_FOREIGN_OBJECT_TAG(macro) \
    FOR_EACH_USE_TAG(macro) \
    macro(a, A) \
    macro(altGlyph, ALTGLYPH) \
    macro(circle, CIRCLE) \
    macro(clipPath, CLIPPATH) \
    macro(cursor, CURSOR) \
    macro(defs, DEFS) \
    macro(desc, DESC) \
    macro(ellipse, ELLIPSE) \
    macro(g, G) \
    macro(glyph, GLYPH) \
    macro(image, IMAGE) \
    macro(linearGradient, LINEARGRADIENT) \
    macro(line, LINE) \
    macro(marker, MARKER) \
    macro(mask, MASK) \
    macro(metadata, METADATA) \
    macro(path, PATH) \
    macro(pattern, PATTERN) \
    macro(polyline, POLYLINE) \
    macro(polygon, POLYGON) \
    macro(radialGradient, RADIALGRADIENT) \
    macro(rect, RECT) \
    macro(script, SCRIPT) \
    macro(stop, STOP) \
    macro(style, STYLE) \
    macro(svg, SVG) \
    macro(switch, SWITCH) \
    macro(symbol, SYMBOL) \
    macro(text, TEXT) \
    macro(textPath, TEXTPATH) \
    macro(title, TITLE) \
    macro(tref, TREF) \
    macro(tspan, TSPAN) \
    macro(view, VIEW) \
    // end of macro

V8ClassIndex::V8WrapperType V8Proxy::GetSVGElementType(SVGElement* element)
{
  static HashMap<String, V8ClassIndex::V8WrapperType> map;
  if (map.isEmpty()) {
#define ADD_TO_HASH_MAP(tag, name) \
    map.set(#tag, V8ClassIndex::SVG##name##ELEMENT);
FOR_EACH_TAG(ADD_TO_HASH_MAP)
#undef ADD_TO_HASH_MAP
  }

  V8ClassIndex::V8WrapperType t = map.get(element->localName().impl());
  if (t == 0) return V8ClassIndex::SVGELEMENT;
  return t;
}
#undef FOR_EACH_TAG

#endif  // ENABLE(SVG)


v8::Handle<v8::Value> V8Proxy::EventToV8Object(Event* event)
{
  if (!event)
      return v8::Null();

  v8::Handle<v8::Object> wrapper = dom_object_map().get(event);
  if (!wrapper.IsEmpty())
    return wrapper;

  V8ClassIndex::V8WrapperType type = V8ClassIndex::EVENT;

  if (event->isUIEvent()) {
    if (event->isKeyboardEvent())
      type = V8ClassIndex::KEYBOARDEVENT;
    else if (event->isTextEvent())
      type = V8ClassIndex::TEXTEVENT;
    else if (event->isMouseEvent())
      type = V8ClassIndex::MOUSEEVENT;
    else if (event->isWheelEvent())
      type = V8ClassIndex::WHEELEVENT;
#if ENABLE(SVG)
    else if (event->isSVGZoomEvent())
      type = V8ClassIndex::SVGZOOMEVENT;
#endif
    else
      type = V8ClassIndex::UIEVENT;
  } else if (event->isMutationEvent())
    type = V8ClassIndex::MUTATIONEVENT;
  else if (event->isOverflowEvent())
    type = V8ClassIndex::OVERFLOWEVENT;
  else if (event->isMessageEvent())
    type = V8ClassIndex::MESSAGEEVENT;
  else if (event->isProgressEvent()) {
    if (event->isXMLHttpRequestProgressEvent()) 
      type = V8ClassIndex::XMLHTTPREQUESTPROGRESSEVENT;
    else 
      type = V8ClassIndex::PROGRESSEVENT;
  } else if (event->isWebKitAnimationEvent()) 
    type = V8ClassIndex::WEBKITANIMATIONEVENT;
  else if (event->isWebKitTransitionEvent())
    type = V8ClassIndex::WEBKITTRANSITIONEVENT;


  v8::Handle<v8::Object> result =
      InstantiateV8Object(type, V8ClassIndex::EVENT, event);
  if (result.IsEmpty()) {
    // Instantiation failed. Avoid updating the DOM object map and
    // return null which is already handled by callers of this function
    // in case the event is NULL.
    return v8::Null();
  }

  event->ref();  // fast ref
  SetJSWrapperForDOMObject(event, v8::Persistent<v8::Object>::New(result));

  return result;
}


// Caller checks node is not null.
v8::Handle<v8::Value> V8Proxy::NodeToV8Object(Node* node)
{
  if (!node) return v8::Null();

  v8::Handle<v8::Object> wrapper = dom_node_map().get(node);
  if (!wrapper.IsEmpty())
    return wrapper;

  bool is_document = false;  // document type node has special handling
  V8ClassIndex::V8WrapperType type;

  switch (node->nodeType()) {
    case Node::ELEMENT_NODE:
      if (node->isHTMLElement())
        type = GetHTMLElementType(static_cast<HTMLElement*>(node));
#if ENABLE(SVG)
      else if (node->isSVGElement())
        type = GetSVGElementType(static_cast<SVGElement*>(node));
#endif
      else
        type = V8ClassIndex::ELEMENT;
      break;
    case Node::ATTRIBUTE_NODE:
      type = V8ClassIndex::ATTR;
      break;
    case Node::TEXT_NODE:
      type = V8ClassIndex::TEXT;
      break;
    case Node::CDATA_SECTION_NODE:
      type = V8ClassIndex::CDATASECTION;
      break;
    case Node::ENTITY_NODE:
      type = V8ClassIndex::ENTITY;
      break;
    case Node::PROCESSING_INSTRUCTION_NODE:
      type = V8ClassIndex::PROCESSINGINSTRUCTION;
      break;
    case Node::COMMENT_NODE:
      type = V8ClassIndex::COMMENT;
      break;
    case Node::DOCUMENT_NODE: {
      is_document = true;
      Document* doc = static_cast<Document*>(node);
      if (doc->isHTMLDocument())
        type = V8ClassIndex::HTMLDOCUMENT;
#if ENABLE(SVG)
      else if (doc->isSVGDocument())
        type = V8ClassIndex::SVGDOCUMENT;
#endif
      else
        type = V8ClassIndex::DOCUMENT;
      break;
    }
    case Node::DOCUMENT_TYPE_NODE:
      type = V8ClassIndex::DOCUMENTTYPE;
      break;
    case Node::NOTATION_NODE:
      type = V8ClassIndex::NOTATION;
      break;
    case Node::DOCUMENT_FRAGMENT_NODE:
      type = V8ClassIndex::DOCUMENTFRAGMENT;
      break;
    case Node::ENTITY_REFERENCE_NODE:
      type = V8ClassIndex::ENTITYREFERENCE;
      break;
    default:
      type = V8ClassIndex::NODE;
  }

  // Find the context to which the node belongs and create the wrapper
  // in that context.  If the node is not in a document, the current
  // context is used.
  v8::Local<v8::Context> context;
  Document* doc = node->document();
  if (doc) {
    context = V8Proxy::GetContext(doc->frame());
  }
  if (!context.IsEmpty()) {
    context->Enter();
  }

  v8::Local<v8::Object> result =
      InstantiateV8Object(type, V8ClassIndex::NODE, node);

  // Exit the node's context if it was entered.
  if (!context.IsEmpty()) {
    context->Exit();
  }

  if (result.IsEmpty()) {
    // If instantiation failed it's important not to add the result
    // to the DOM node map. Instead we return an empty handle, which
    // should already be handled by callers of this function in case
    // the node is NULL.
    return result;
  }

  node->ref();
  SetJSWrapperForDOMNode(node, v8::Persistent<v8::Object>::New(result));

  if (is_document) {
    Document* doc = static_cast<Document*>(node);
    V8Proxy* proxy = V8Proxy::retrieve(doc->frame());
    if (proxy)
        proxy->UpdateDocumentWrapper(result);

    if (type == V8ClassIndex::HTMLDOCUMENT) {
      // Create marker object and insert it in two internal fields.
      // This is used to implement temporary shadowing of
      // document.all.
      ASSERT(result->InternalFieldCount() ==
             V8Custom::kHTMLDocumentInternalFieldCount);
      v8::Local<v8::Object> marker = v8::Object::New();
      result->SetInternalField(V8Custom::kHTMLDocumentMarkerIndex, marker);
      result->SetInternalField(V8Custom::kHTMLDocumentShadowIndex, marker);
    }
  }

  return result;
}


// A JS object of type EventTarget can only be five possible types:
// 1) EventTargetNode; 2) XMLHttpRequest; 3) MessagePort; 4) SVGElementInstance;
// 5) XMLHttpRequestUpload
// check EventTarget.h for new type conversion methods
v8::Handle<v8::Value> V8Proxy::EventTargetToV8Object(EventTarget* target)
{
  if (!target)
      return v8::Null();

#if ENABLE(SVG)
  SVGElementInstance* instance = target->toSVGElementInstance();
  if (instance)
      return ToV8Object(V8ClassIndex::SVGELEMENTINSTANCE, instance);
#endif

  Node* node = target->toNode();
  if (node)
      return NodeToV8Object(node);

  // XMLHttpRequest is created within its JS counterpart.
  XMLHttpRequest* xhr = target->toXMLHttpRequest();
  if (xhr) {
    v8::Handle<v8::Object> wrapper = active_dom_object_map().get(xhr);
    ASSERT(!wrapper.IsEmpty());
    return wrapper;
  }

  // MessagePort is created within its JS counterpart
  MessagePort* port = target->toMessagePort();
  if (port) {
    v8::Handle<v8::Object> wrapper = active_dom_object_map().get(port);
    ASSERT(!wrapper.IsEmpty());
    return wrapper;
  }

  XMLHttpRequestUpload* upload = target->toXMLHttpRequestUpload();
  if (upload) {
    v8::Handle<v8::Object> wrapper = dom_object_map().get(upload);
    ASSERT(!wrapper.IsEmpty());
    return wrapper;
  }

  ASSERT(0);
  return v8::Handle<v8::Value>();
}


v8::Handle<v8::Value> V8Proxy::EventListenerToV8Object(
    EventListener* listener)
{
  if (listener == 0) return v8::Null();

  // TODO(fqian): can a user take a lazy event listener and set to other places?
  V8AbstractEventListener* v8listener =
      static_cast<V8AbstractEventListener*>(listener);
  return v8listener->GetListenerObject();
}


v8::Handle<v8::Value> V8Proxy::DOMImplementationToV8Object(
    DOMImplementation* impl)
{
  v8::Handle<v8::Object> result =
    InstantiateV8Object(V8ClassIndex::DOMIMPLEMENTATION,
                        V8ClassIndex::DOMIMPLEMENTATION,
                        impl);
  if (result.IsEmpty()) {
    // If the instantiation failed, we ignore it and return null instead
    // of returning an empty handle.
    return v8::Null();
  }
  return result;
}


v8::Handle<v8::Value> V8Proxy::StyleSheetToV8Object(StyleSheet* sheet)
{
  if (!sheet) return v8::Null();

  v8::Handle<v8::Object> wrapper = dom_object_map().get(sheet);
  if (!wrapper.IsEmpty())
    return wrapper;

  V8ClassIndex::V8WrapperType type = V8ClassIndex::STYLESHEET;
  if (sheet->isCSSStyleSheet())
    type = V8ClassIndex::CSSSTYLESHEET;

  v8::Handle<v8::Object> result =
      InstantiateV8Object(type, V8ClassIndex::STYLESHEET, sheet);
  if (!result.IsEmpty()) {
    // Only update the DOM object map if the result is non-empty.
    sheet->ref();
    SetJSWrapperForDOMObject(sheet, v8::Persistent<v8::Object>::New(result));
  }

  // Add a hidden reference from stylesheet object to its owner node.
  Node* owner_node = sheet->ownerNode();
  if (owner_node) {
    v8::Handle<v8::Object> owner =
        v8::Handle<v8::Object>::Cast(NodeToV8Object(owner_node));
    result->SetInternalField(V8Custom::kStyleSheetOwnerNodeIndex, owner);
  }

  return result;
}


v8::Handle<v8::Value> V8Proxy::CSSValueToV8Object(CSSValue* value)
{
  if (!value) return v8::Null();

  v8::Handle<v8::Object> wrapper = dom_object_map().get(value);
  if (!wrapper.IsEmpty())
    return wrapper;

  V8ClassIndex::V8WrapperType type;

  if (value->isWebKitCSSTransformValue()) 
    type = V8ClassIndex::WEBKITCSSTRANSFORMVALUE;
  else if (value->isValueList())
    type = V8ClassIndex::CSSVALUELIST;
  else if (value->isPrimitiveValue())
    type = V8ClassIndex::CSSPRIMITIVEVALUE;
#if ENABLE(SVG)
  else if (value->isSVGPaint())
    type = V8ClassIndex::SVGPAINT;
  else if (value->isSVGColor())
    type = V8ClassIndex::SVGCOLOR;
#endif
  else
    type = V8ClassIndex::CSSVALUE;

  v8::Handle<v8::Object> result =
      InstantiateV8Object(type, V8ClassIndex::CSSVALUE, value);
  if (!result.IsEmpty()) {
    // Only update the DOM object map if the result is non-empty.
    value->ref();
    SetJSWrapperForDOMObject(value, v8::Persistent<v8::Object>::New(result));
  }

  return result;
}


v8::Handle<v8::Value> V8Proxy::CSSRuleToV8Object(CSSRule* rule)
{
    if (!rule) return v8::Null();

    v8::Handle<v8::Object> wrapper = dom_object_map().get(rule);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type;

    switch (rule->type()) {
        case CSSRule::STYLE_RULE:
            type = V8ClassIndex::CSSSTYLERULE;
            break;
        case CSSRule::CHARSET_RULE:
            type = V8ClassIndex::CSSCHARSETRULE;
            break;
        case CSSRule::IMPORT_RULE:
            type = V8ClassIndex::CSSIMPORTRULE;
            break;
        case CSSRule::MEDIA_RULE:
            type = V8ClassIndex::CSSMEDIARULE;
            break;
        case CSSRule::FONT_FACE_RULE:
            type = V8ClassIndex::CSSFONTFACERULE;
            break;
        case CSSRule::PAGE_RULE:
            type = V8ClassIndex::CSSPAGERULE;
            break;
        case CSSRule::VARIABLES_RULE:
            type = V8ClassIndex::CSSVARIABLESRULE;
            break;
        case CSSRule::WEBKIT_KEYFRAME_RULE:
            type = V8ClassIndex::WEBKITCSSKEYFRAMERULE;
            break;
        case CSSRule::WEBKIT_KEYFRAMES_RULE:
            type = V8ClassIndex::WEBKITCSSKEYFRAMESRULE;
            break;
        default:  // CSSRule::UNKNOWN_RULE
            type = V8ClassIndex::CSSRULE;
            break;
    }

    v8::Handle<v8::Object> result =
        InstantiateV8Object(type, V8ClassIndex::CSSRULE, rule);
    if (!result.IsEmpty()) {
        // Only update the DOM object map if the result is non-empty.
        rule->ref();
        SetJSWrapperForDOMObject(rule, v8::Persistent<v8::Object>::New(result));
    }
    return result;
}

v8::Handle<v8::Value> V8Proxy::WindowToV8Object(DOMWindow* window)
{
    if (!window) return v8::Null();
    // Initializes environment of a frame, and return the global object
    // of the frame.
    Frame* frame = window->frame();
    if (!frame)
        return v8::Handle<v8::Object>();

    v8::Handle<v8::Context> context = GetContext(frame);
    if (context.IsEmpty())
        return v8::Handle<v8::Object>();

    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    return global;
}

void V8Proxy::BindJSObjectToWindow(Frame* frame,
                                   const char* name,
                                   int type,
                                   v8::Handle<v8::FunctionTemplate> desc,
                                   void* imp)
{
    // Get environment.
    v8::Handle<v8::Context> context = V8Proxy::GetContext(frame);
    if (context.IsEmpty())
        return;  // JS not enabled.

    v8::Context::Scope scope(context);
    v8::Handle<v8::Object> instance = desc->GetFunction();
    SetDOMWrapper(instance, type, imp);

    v8::Handle<v8::Object> global = context->Global();
    global->Set(v8::String::New(name), instance);
}

void V8Proxy::ProcessConsoleMessages()
{
    ConsoleMessageManager::ProcessDelayedMessages();
}


// Create the utility context for holding JavaScript functions used internally
// which are not visible to JavaScript executing on the page.
void V8Proxy::CreateUtilityContext() {
    ASSERT(m_utilityContext.IsEmpty());

    v8::HandleScope scope;
    v8::Handle<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();
    m_utilityContext = v8::Context::New(NULL, global_template);
    v8::Context::Scope context_scope(m_utilityContext);

    // Compile JavaScript function for retrieving the source line of the top
    // JavaScript stack frame.
    static const char* frame_source_line_source =
        "function frame_source_line(exec_state) {"
        "  return exec_state.frame(0).sourceLine();"
        "}";
    v8::Script::Compile(v8::String::New(frame_source_line_source))->Run();

    // Compile JavaScript function for retrieving the source name of the top
    // JavaScript stack frame.
    static const char* frame_source_name_source =
        "function frame_source_name(exec_state) {"
        "  var frame = exec_state.frame(0);"
        "  if (frame.func().resolved() && "
        "      frame.func().script() && "
        "      frame.func().script().name()) {"
        "    return frame.func().script().name();"
        "  }"
        "}";
    v8::Script::Compile(v8::String::New(frame_source_name_source))->Run();
}


int V8Proxy::GetSourceLineNumber() {
    v8::HandleScope scope;
    v8::Handle<v8::Context> utility_context = V8Proxy::GetUtilityContext();
    if (utility_context.IsEmpty()) {
        return 0;
    }
    v8::Context::Scope context_scope(utility_context);
    v8::Handle<v8::Function> frame_source_line;
    frame_source_line = v8::Local<v8::Function>::Cast(
        utility_context->Global()->Get(v8::String::New("frame_source_line")));
    if (frame_source_line.IsEmpty()) {
        return 0;
    }
    return v8::Debug::Call(frame_source_line)->Int32Value();
}


String V8Proxy::GetSourceName() {
    v8::HandleScope scope;
    v8::Handle<v8::Context> utility_context = GetUtilityContext();
    if (utility_context.IsEmpty()) {
        return String();
    }
    v8::Context::Scope context_scope(utility_context);
    v8::Handle<v8::Function> frame_source_name;
    frame_source_name = v8::Local<v8::Function>::Cast(
        utility_context->Global()->Get(v8::String::New("frame_source_name")));
    if (frame_source_name.IsEmpty()) {
        return String();
    }
    return ToWebCoreString(v8::Debug::Call(frame_source_name));
}

}  // namespace WebCore
