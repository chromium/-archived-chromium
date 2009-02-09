// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROXY_H__
#define V8_PROXY_H__

#include <v8.h>
#include "v8_index.h"
#include "v8_custom.h"
#include "v8_utility.h"
#include "ChromiumBridge.h"
#include "Node.h"
#include "NodeFilter.h"
#include "SecurityOrigin.h"  // for WebCore::SecurityOrigin
#include "PlatformString.h"  // for WebCore::String
#include <wtf/PassRefPtr.h> // so generated bindings don't have to
#include <wtf/Assertions.h>

#include <iterator>
#include <list>

#ifdef ENABLE_DOM_STATS_COUNTERS
#define INC_STATS(name) ChromiumBridge::incrementStatsCounter(name)
#else
#define INC_STATS(name)
#endif

namespace WebCore {

class CSSStyleDeclaration;
class DOMImplementation;
class Element;
class Event;
class EventListener;
class Frame;
class HTMLCollection;
class HTMLOptionsCollection;
class HTMLElement;
class HTMLDocument;
class MediaList;
class NamedNodeMap;
class Node;
class NodeList;
class Screen;
class String;
class StyleSheet;
class SVGElement;
class DOMWindow;
class Document;
class EventTarget;
class Event;
class EventListener;
class Navigator;
class MimeType;
class MimeTypeArray;
class Plugin;
class PluginArray;
class StyleSheetList;
class CSSValue;
class CSSRule;
class CSSRuleList;
class CSSValueList;
class NodeFilter;
class ScriptExecutionContext;

#if ENABLE(SVG)
class SVGElementInstance;
#endif

class V8EventListener;
class V8ObjectEventListener;
typedef std::list<V8EventListener*>  V8EventListenerList;
typedef std::list<v8::Extension*> V8ExtensionList;

// TODO(fqian): use standard logging facilities in WebCore.
void log_info(Frame* frame, const String& msg, const String& url);


#ifndef NDEBUG

#define GlobalHandleTypeList(V) \
  V(PROXY)                      \
  V(NPOBJECT)                   \
  V(SCHEDULED_ACTION)           \
  V(EVENT_LISTENER)             \
  V(NODE_FILTER)                \
  V(SCRIPTINSTANCE)             \
  V(SCRIPTVALUE)                


// Host information of persistent handles.
enum GlobalHandleType {
#define ENUM(name) name,
  GlobalHandleTypeList(ENUM)
#undef ENUM
};


class GlobalHandleInfo {
 public:
  GlobalHandleInfo(void* host, GlobalHandleType type)
      : host_(host), type_(type) { }
  void* host_;
  GlobalHandleType type_;
};

#endif  // NDEBUG

// The following Batch structs and methods are used for setting multiple
// properties on an ObjectTemplate, used from the generated bindings
// initialization (ConfigureXXXTemplate).  This greatly reduces the binary
// size by moving from code driven setup to data table driven setup.

// BatchedAttribute translates into calls to SetAccessor() on either the
// instance or the prototype ObjectTemplate, based on |on_proto|.
struct BatchedAttribute {
  const char* const name;
  v8::AccessorGetter getter;
  v8::AccessorSetter setter;
  V8ClassIndex::V8WrapperType data;
  v8::AccessControl settings;
  v8::PropertyAttribute attribute;
  bool on_proto;
};

void BatchConfigureAttributes(v8::Handle<v8::ObjectTemplate> inst,
                              v8::Handle<v8::ObjectTemplate> proto,
                              const BatchedAttribute* attrs,
                              size_t num_attrs);

// BhatchedConstant translates into calls to Set() for setting up an object's
// constants.  It sets the constant on both the FunctionTemplate |desc| and the
// ObjectTemplate |proto|.  PropertyAttributes is always ReadOnly.
struct BatchedConstant {
  const char* const name;
  int value;
};

void BatchConfigureConstants(v8::Handle<v8::FunctionTemplate> desc,
                             v8::Handle<v8::ObjectTemplate> proto,
                             const BatchedConstant* consts,
                             size_t num_consts);

class V8Proxy {
 public:
  // The types of javascript errors that can be thrown.
  enum ErrorType {
    RANGE_ERROR,
    REFERENCE_ERROR,
    SYNTAX_ERROR,
    TYPE_ERROR,
    GENERAL_ERROR
  };

  explicit V8Proxy(Frame* frame)
      : m_frame(frame), m_inlineCode(false),
        m_timerCallback(false), m_recursion(0) { }

  ~V8Proxy();

  Frame* frame() { return m_frame; }

  // Clear page-specific data, but keep the global object identify.
  void clearForNavigation();

  // Clear page-specific data before shutting down the proxy object.
  void clearForClose();

  // Update document object of the frame.
  void updateDocument();

  // Update the security origin of a document
  // (e.g., after setting docoument.domain).
  void updateSecurityOrigin();

  // Destroy the global object.
  void DestroyGlobal();

  // TODO(mpcomplete): Need comment.  User Gesture related.
  bool inlineCode() const { return m_inlineCode; }
  void setInlineCode(bool value) { m_inlineCode = value; }

  bool timerCallback() const { return m_timerCallback; }
  void setTimerCallback(bool value) { m_timerCallback = value; }

  // Has the context for this proxy been initialized?
  bool ContextInitialized();

  // Disconnects the proxy from its owner frame,
  // and clears all timeouts on the DOM window.
  void disconnectFrame();

  bool isEnabled();

  // Find/Create/Remove event listener wrappers.
  PassRefPtr<V8EventListener> FindV8EventListener(v8::Local<v8::Value> listener,
                                       bool html);
  PassRefPtr<V8EventListener> FindOrCreateV8EventListener(v8::Local<v8::Value> listener,
                                               bool html);

  PassRefPtr<V8EventListener> FindObjectEventListener(v8::Local<v8::Value> listener,
                                        bool html);
  PassRefPtr<V8EventListener> FindOrCreateObjectEventListener(v8::Local<v8::Value> listener,
                                                bool html);

  void RemoveV8EventListener(V8EventListener* listener);
  void RemoveObjectEventListener(V8ObjectEventListener* listener);

  // Protect/Unprotect JS wrappers of a DOM object.
  static void GCProtect(void* dom_object);
  static void GCUnprotect(void* dom_object);

  // Create a lazy event listener.
  PassRefPtr<EventListener> createInlineEventListener(
      const String& functionName, const String& code, Node* node);
#if ENABLE(SVG)
  PassRefPtr<EventListener> createSVGEventHandler(
      const String& functionName, const String& code, Node* node);

  static void SetSVGContext(void* object, SVGElement* context);
  static SVGElement* GetSVGContext(void* object);
#endif

  void setEventHandlerLineno(int lineno) { m_handlerLineno = lineno; }
  void finishedWithEvent(Event* event) { }

  // Evaluate a script file in the current execution environment.
  // The caller must hold an execution context.
  // If cannot evalute the script, it returns an error.
  v8::Local<v8::Value> Evaluate(const String& filename, int baseLine,
                                const String& code, Node* node);

  // Run an already compiled script.
  v8::Local<v8::Value> RunScript(v8::Handle<v8::Script> script,
                                 bool inline_code);

  // Call the function with the given receiver and arguments.
  v8::Local<v8::Value> CallFunction(v8::Handle<v8::Function> function,
                                    v8::Handle<v8::Object> receiver,
                                    int argc,
                                    v8::Handle<v8::Value> argv[]);

  // Returns the dom constructor function for the given node type.
  v8::Local<v8::Function> GetConstructor(V8ClassIndex::V8WrapperType type);

  // Returns the window object of the currently executing context.
  static DOMWindow* retrieveWindow();
  // Returns the window object associated with a context.
  static DOMWindow* retrieveWindow(v8::Handle<v8::Context> context);
  // Returns V8Proxy object of the currently executing context.
  static V8Proxy* retrieve();
  // Returns V8Proxy object associated with a frame.
  static V8Proxy* retrieve(Frame* frame);
  // Returns V8Proxy object associated with a script execution context.
  static V8Proxy* retrieve(ScriptExecutionContext* context);

  // Returns the frame object of the window object associated
  // with the currently executing context.
  static Frame* retrieveFrame();
  // Returns the frame object of the window object associated with
  // a context.
  static Frame* retrieveFrame(v8::Handle<v8::Context> context);
  // Returns the frame that started JS execution.
  // NOTE: cannot declare retrieveActiveFrame as inline function,
  // VS complains at linking time.
  static Frame* retrieveActiveFrame();

  // Returns V8 Context of a frame. If none exists, creates
  // a new context.  It is potentially slow and consumes memory.
  static v8::Local<v8::Context> GetContext(Frame* frame);
  static v8::Local<v8::Context> GetCurrentContext();

  // If the current context causes out of memory, JavaScript setting
  // is disabled and it returns true.
  static bool HandleOutOfMemory();

  // Check if the active execution context can access the target frame.
  static bool CanAccessFrame(Frame* target, bool report_error);

  // Check if it is safe to access the given node from the
  // current security context.
  static bool CheckNodeSecurity(Node* node);

  static v8::Handle<v8::Value> CheckNewLegal(const v8::Arguments& args);

  // Create a V8 wrapper for a C pointer
  static v8::Handle<v8::Value> WrapCPointer(void* cptr) {
    // Represent void* as int
    int addr = reinterpret_cast<int>(cptr);
    ASSERT((addr & 0x01) == 0);  // the address must be aligned.
    return v8::Integer::New(addr >> 1);
  }

  // Take C pointer out of a v8 wrapper
  template <class C>
  static C* ExtractCPointer(v8::Handle<v8::Value> obj) {
    return static_cast<C*>(ExtractCPointerImpl(obj));
  }

  static v8::Handle<v8::Script> CompileScript(v8::Handle<v8::String> code,
                                              const String& fileName,
                                              int baseLine);

#ifndef NDEBUG
  // Checks if a v8 value can be a DOM wrapper
  static bool MaybeDOMWrapper(v8::Handle<v8::Value> value);
#endif

  // Sets contents of a DOM wrapper.
  static void SetDOMWrapper(v8::Handle<v8::Object> obj, int type, void* ptr);

  static v8::Handle<v8::Object> LookupDOMWrapper(
    V8ClassIndex::V8WrapperType type, v8::Handle<v8::Value> value);

  // A helper function extract native object pointer from a DOM wrapper
  // and cast to the specified type.
  template <class C>
  static C* DOMWrapperToNative(v8::Handle<v8::Value> object) {
    ASSERT(MaybeDOMWrapper(object));
    v8::Handle<v8::Value> ptr =
      v8::Handle<v8::Object>::Cast(object)->GetInternalField(
          V8Custom::kDOMWrapperObjectIndex);
    return ExtractCPointer<C>(ptr);
  }

  // A help function extract a node type pointer from a DOM wrapper.
  // Wrapped pointer must be cast to Node* first.
  template <class C>
  static C* DOMWrapperToNode(v8::Handle<v8::Value> value) {
    ASSERT(MaybeDOMWrapper(value));

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);

    ASSERT(GetDOMWrapperType(object) == V8ClassIndex::NODE);

    v8::Handle<v8::Value> wrapper =
       object->GetInternalField(V8Custom::kDOMWrapperObjectIndex);
    return static_cast<C*>(ExtractCPointer<Node>(wrapper));
  }

  template<typename T>
  static v8::Handle<v8::Value> ToV8Object(V8ClassIndex::V8WrapperType type, PassRefPtr<T> imp)
  {
    return ToV8Object(type, imp.get());
  }
  static v8::Handle<v8::Value> ToV8Object(V8ClassIndex::V8WrapperType type,
                                          void* imp);
  // Fast-path for Node objects.
  static v8::Handle<v8::Value> NodeToV8Object(Node* node);

  template <class C>
  static C* ToNativeObject(V8ClassIndex::V8WrapperType type,
                           v8::Handle<v8::Value> object) {
    return static_cast<C*>(ToNativeObjectImpl(type, object));
  }

  static V8ClassIndex::V8WrapperType GetDOMWrapperType(
      v8::Handle<v8::Object> object);

  // If the exception code is different from zero, a DOM exception is
  // schedule to be thrown.
  static void SetDOMException(int exception_code);

  // Schedule an error object to be thrown.
  static v8::Handle<v8::Value> ThrowError(ErrorType type, const char* message);

  // Create an instance of a function descriptor and set to the global object
  // as a named property. Used by v8_test_shell.
  static void BindJSObjectToWindow(Frame* frame,
                                   const char* name,
                                   int type,
                                   v8::Handle<v8::FunctionTemplate> desc,
                                   void* imp);

  static v8::Handle<v8::Value> EventToV8Object(Event* event);
  static Event* ToNativeEvent(v8::Handle<v8::Value> jsevent) {
    if (!IsDOMEventWrapper(jsevent)) return 0;
    return DOMWrapperToNative<Event>(jsevent);
  }

  static v8::Handle<v8::Value> EventTargetToV8Object(EventTarget* target);
  // Wrap and unwrap JS event listeners
  static v8::Handle<v8::Value> EventListenerToV8Object(EventListener* target);

  // DOMImplementation is a singleton and it is handled in a special
  // way.  A wrapper is generated per document and stored in an
  // internal field of the document.
  static v8::Handle<v8::Value> DOMImplementationToV8Object(
      DOMImplementation* impl);

  // Wrap JS node filter in C++
  static PassRefPtr<NodeFilter> ToNativeNodeFilter(v8::Handle<v8::Value> filter);

  static v8::Persistent<v8::FunctionTemplate> GetTemplate(
      V8ClassIndex::V8WrapperType type);

  template <int tag, typename T>
    static v8::Handle<v8::Value> ConstructDOMObject(const v8::Arguments& args);

  // Checks whether a DOM object has a JS wrapper.
  static bool DOMObjectHasJSWrapper(void* obj); 
  // Set JS wrapper of a DOM object, the caller in charge of increase ref.
  static void SetJSWrapperForDOMObject(void* obj,
                                       v8::Persistent<v8::Object> wrapper);
  static void SetJSWrapperForActiveDOMObject(void* obj,
                                             v8::Persistent<v8::Object> wrapper);
  static void SetJSWrapperForDOMNode(Node* node,
                                     v8::Persistent<v8::Object> wrapper);

  // Process any pending JavaScript console messages.
  static void ProcessConsoleMessages();

#ifndef NDEBUG
  // For debugging and leak detection purpose
  static void RegisterGlobalHandle(GlobalHandleType type,
                                   void* host,
                                   v8::Persistent<v8::Value> handle);
  static void UnregisterGlobalHandle(void* host,
                                     v8::Persistent<v8::Value> handle);
#endif

  // Check whether a V8 value is a wrapper of type |classType|.
  static bool IsWrapperOfType(v8::Handle<v8::Value> obj,
                              V8ClassIndex::V8WrapperType classType);

  // Function for retrieving the line number and source name for the top
  // JavaScript stack frame.
  static int GetSourceLineNumber();
  static String GetSourceName();

 private:
  // Register extensions before initializing the context.  Once the context
  // is initialized, extensions cannot be registered.
  static void RegisterExtension(v8::Extension* extension);
  void InitContextIfNeeded();
  void DisconnectEventListeners();
  void SetSecurityToken();
  void ClearDocumentWrapper();
  void UpdateDocumentWrapper(v8::Handle<v8::Value> wrapper);
  // Dispose global handles of m_contexts and friends.
  void DisposeContextHandles();

  static bool CanAccessPrivate(DOMWindow* target);

  // Check whether a V8 value is a DOM Event wrapper
  static bool IsDOMEventWrapper(v8::Handle<v8::Value> obj);

  static void* ToNativeObjectImpl(V8ClassIndex::V8WrapperType type,
                                  v8::Handle<v8::Value> object);

  // Take C pointer out of a v8 wrapper
  static void* ExtractCPointerImpl(v8::Handle<v8::Value> obj) {
    ASSERT(obj->IsNumber());
    int addr = obj->Int32Value();
    return reinterpret_cast<void*>(addr << 1);
  }


  static v8::Handle<v8::Value> StyleSheetToV8Object(StyleSheet* sheet);
  static v8::Handle<v8::Value> CSSValueToV8Object(CSSValue* value);
  static v8::Handle<v8::Value> CSSRuleToV8Object(CSSRule* rule);
  // Returns the JS wrapper of a window object, initializes the environment
  // of the window frame if needed.
  static v8::Handle<v8::Value> WindowToV8Object(DOMWindow* window);

#if ENABLE(SVG)
  static v8::Handle<v8::Value> SVGElementInstanceToV8Object(
      SVGElementInstance* instance);
  static v8::Handle<v8::Value> SVGObjectWithContextToV8Object(
      V8ClassIndex::V8WrapperType type, void* object);
#endif

  // Set hidden references in a DOMWindow object of a frame.
  static void SetHiddenWindowReference(Frame* frame,
                                       const int internal_index,
                                       v8::Handle<v8::Object> jsobj);

  static V8ClassIndex::V8WrapperType GetHTMLElementType(HTMLElement* elm);

  // The first parameter, desc_type, specifies the function descriptor
  // used to create JS object. The second parameter, cptr_type, specifies
  // the type of third parameter, impl, for type casting.
  // For example, a HTML element has HTMLELEMENT desc_type, but always
  // use NODE as cptr_type. JS wrapper stores cptr_type and impl as
  // internal fields.
  static v8::Local<v8::Object> InstantiateV8Object(
      V8ClassIndex::V8WrapperType desc_type,
      V8ClassIndex::V8WrapperType cptr_type,
      void* impl);

  static const char* GetRangeExceptionName(int exception_code);
  static const char* GetEventExceptionName(int exception_code);
  static const char* GetXMLHttpRequestExceptionName(int exception_code);
  static const char* GetDOMExceptionName(int exception_code);

#if ENABLE(XPATH)
  static const char* GetXPathExceptionName(int exception_code);
#endif

#if ENABLE(SVG)
  static V8ClassIndex::V8WrapperType GetSVGElementType(SVGElement* elm);
  static const char* GetSVGExceptionName(int exception_code);
#endif

  // Returns a local handle of the context.
  v8::Local<v8::Context> GetContext() {
    return v8::Local<v8::Context>::New(m_context);
  }

  // Create and populate the utility context.
  static void CreateUtilityContext();

  // Returns a local handle of the utility context.
  static v8::Local<v8::Context> GetUtilityContext() {
    if (m_utilityContext.IsEmpty()) {
      CreateUtilityContext();
    }
    return v8::Local<v8::Context>::New(m_utilityContext);
  }

  Frame* m_frame;

  v8::Persistent<v8::Context> m_context;
  // DOM constructors are cached per context. A DOM constructor is a function
  // instance created from a DOM constructor template. There is one instance
  // per context. A DOM constructor is different from a normal function in
  // two ways: 1) it cannot be called as constructor (aka, used to create
  // a DOM object); 2) its __proto__ points to Object.prototype rather than
  // Function.prototype. The reason for 2) is that, in Safari, a DOM constructor
  // is a normal JS object, but not a function. Hotmail relies on the fact
  // that, in Safari, HTMLElement.__proto__ == Object.prototype.
  //
  // m_object_prototype is a cache of the original Object.prototype.
  //
  // Both handles must be disposed when the context is disposed. Otherwise,
  // it can keep all objects alive.
  v8::Persistent<v8::Array> m_dom_constructor_cache;
  v8::Persistent<v8::Value> m_object_prototype;
  
  v8::Persistent<v8::Object> m_global;
  v8::Persistent<v8::Value> m_document;

  // Utility context holding JavaScript functions used internally.
  static v8::Persistent<v8::Context> m_utilityContext;

  int m_handlerLineno;

  // A list of event listeners created for this frame,
  // the list gets cleared when removing all timeouts.
  V8EventListenerList m_event_listeners;

  // A list of event listeners create for XMLHttpRequest object for this frame,
  // the list gets cleared when removing all timeouts.
  V8EventListenerList m_xhr_listeners;

  // True for <a href="javascript:foo()"> and false for <script>foo()</script>.
  // Only valid during execution.
  bool m_inlineCode;

  // True when executing from within a timer callback.  Only valid during
  // execution.
  bool m_timerCallback;

  // Track the recursion depth to be able to avoid too deep recursion. The V8
  // engine allows much more recursion than KJS does so we need to guard against
  // excessive recursion in the binding layer.
  int m_recursion;

  // List of extensions registered with the context.
  static V8ExtensionList m_extensions;
};

template <int tag, typename T>
v8::Handle<v8::Value> V8Proxy::ConstructDOMObject(const v8::Arguments& args) {
  if (!args.IsConstructCall()) {
    V8Proxy::ThrowError(V8Proxy::TYPE_ERROR,
        "DOM object constructor cannot be called as a function.");
    return v8::Undefined();
  }


  // Note: it's OK to let this RefPtr go out of scope because we also call
  // SetDOMWrapper(), which effectively holds a reference to obj.
  RefPtr<T> obj = T::create();
  V8Proxy::SetDOMWrapper(args.Holder(), tag, obj.get());
  obj->ref();
  V8Proxy::SetJSWrapperForDOMObject(
      obj.get(), v8::Persistent<v8::Object>::New(args.Holder()));
  return args.Holder();
}

}  // namespace WebCore

#endif  // V8_PROXY_H__

