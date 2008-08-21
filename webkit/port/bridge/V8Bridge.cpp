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

#define max max  // windef.h overrides this, and it breaks us.
#include "config.h"
#include "V8Bridge.h"
#include "v8_proxy.h"
#include "v8_binding.h"
#include "CString.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "Node.h"
#include "DOMWindow.h"
#include "Document.h"
#include "np_v8object.h"
#include "v8_npobject.h"

NPRuntimeFunctions npruntime_functions = {
  NPN_GetStringIdentifier,
  NPN_GetStringIdentifiers,
  NPN_GetIntIdentifier,
  NPN_IdentifierIsString,
  NPN_UTF8FromIdentifier,
  NPN_IntFromIdentifier,
  NPN_CreateObject,
  NPN_RetainObject,
  NPN_ReleaseObject,
  NPN_Invoke,
  NPN_InvokeDefault,
  NPN_Evaluate,
  NPN_GetProperty,
  NPN_SetProperty,
  NPN_RemoveProperty,
  NPN_HasProperty,
  NPN_HasMethod,
  NPN_ReleaseVariantValue,
  NPN_SetException
};



namespace WebCore {

bool JSBridge::m_recordPlaybackMode = false;

// Implements static function declared in JSBridge.
void JSBridge::setFlags(const char* str, int length) {
  v8::V8::SetFlagsFromString(str, length);
}

// static
void JSBridge::setDomain(Frame* frame, const String&) {
  V8Proxy::DomainChanged(frame);
}

// static
Frame* JSBridge::retrieveActiveFrame() {
  return V8Proxy::retrieveActiveFrame();
}

// static
bool JSBridge::isSafeScript(Frame* target) {
  return V8Proxy::IsFromSameOrigin(target, true);
}

// static
void JSBridge::gcProtectJSWrapper(void* dom_object) {
  V8Proxy::GCProtect(static_cast<Peerable*>(dom_object));
}

// static
void JSBridge::gcUnprotectJSWrapper(void* dom_object) {
  V8Proxy::GCUnprotect(static_cast<Peerable*>(dom_object));
}

// static
JSException JSBridge::NoException() {
  return v8::Local<v8::Value>();
}

// static
bool JSBridge::IsException(JSException exception) {
  return !exception.IsEmpty();
}

// static
PausedTimeouts* JSBridge::pauseTimeouts(Frame* frame) {
  if (!frame) return NULL;
  DOMWindow* window = frame->domWindow();
  if (!window) return NULL;
  return window->pauseTimeouts();
}

// static
void JSBridge::resumeTimeouts(Frame* frame, PausedTimeouts* timeouts) {
  if (!frame) return;
  DOMWindow* window = frame->domWindow();
  if (!window) return;
  window->resumeTimeouts(timeouts);
}


// ---------------------------------------------------------------------------
// V8 implementation of JSBridge.

V8Bridge::V8Bridge(Frame* frame) {
    m_proxy = new V8Proxy(frame);
}

V8Bridge::~V8Bridge() {
    delete m_proxy;
}

// Disconnect the proxy from its owner frame;
void V8Bridge::disconnectFrame() {
  m_proxy->disconnectFrame();
}

bool V8Bridge::wasRunByUserGesture() {
  Frame* active_frame = V8Proxy::retrieveActiveFrame();
  // No script is running, must be run by users.
  if (!active_frame)
    return true;

  V8Proxy* active_proxy =
      static_cast<V8Bridge*>(active_frame->scriptBridge())->proxy();

  v8::HandleScope handle_scope;
  v8::Handle<v8::Context> context = V8Proxy::GetContext(active_frame);
  // TODO(fqian): find all cases context can be empty:
  //  1) JS is disabled;
  //  2) page is NULL;
  if (context.IsEmpty())
    return true;

  v8::Context::Scope scope(context);

  v8::Handle<v8::Object> global = context->Global();
  v8::Handle<v8::Value> jsevent = global->Get(v8::String::NewSymbol("event"));
  Event* event = V8Proxy::ToNativeEvent(jsevent);

  // Based on code from kjs_bindings.cpp.
  // Note: This is more liberal than Firefox's implementation.
  if (event) {
    const AtomicString& type = event->type();
    bool event_ok =
      // mouse events
      type == EventNames::clickEvent ||
      type == EventNames::mousedownEvent ||
      type == EventNames::mouseupEvent ||
      type == EventNames::dblclickEvent ||
      // keyboard events
      type == EventNames::keydownEvent ||
      type == EventNames::keypressEvent ||
      type == EventNames::keyupEvent ||
      // other accepted events
      type == EventNames::selectEvent ||
      type == EventNames::changeEvent ||
      type == EventNames::focusEvent ||
      type == EventNames::blurEvent ||
      type == EventNames::submitEvent;

    if (event_ok)
      return true;
  } else {  // no event
    if (active_proxy->inlineCode() &&
        !active_proxy->timerCallback()) {
      // This is the <a href="javascript:window.open('...')> case -> we let it
      // through
      return true;
    }
    // This is the <script>window.open(...)</script> case or a timer callback ->
    // block it
  }

  return false;
}


// Evaluate a script file in the environment of this proxy.  Used for 
// evaluating the code in script tags and for evaluating the code from
// javascript URLs.
String V8Bridge::evaluate(const String& filename, int baseLine,
                          const String& code, Node* node, bool* succ) {
  *succ = false;
  String result;

  v8::HandleScope hs;
  v8::Handle<v8::Context> context = V8Proxy::GetContext(m_proxy->frame());
  if (context.IsEmpty())
    return result;

  v8::Context::Scope scope(context);

  v8::Local<v8::Value> obj;
  {
    // Isolate exceptions that occur when executing the code.  These
    // exceptions should not interfere with javascript code we might
    // evaluate from C++ when returning from here.
    v8::TryCatch exception_block;
    exception_block.SetVerbose(true);
    obj = m_proxy->Evaluate(filename, baseLine, code, node);
  }

  if (obj.IsEmpty() || obj->IsUndefined())
    return result;

  // If the return value is not a string, return 0 (what KJS does).
  if (!obj->IsString()) {
    v8::TryCatch exception_block;
    obj = obj->ToString();
    if (exception_block.HasCaught())
      obj = v8::String::New("");
  }

  result = ToWebCoreString(obj);
  *succ = true;

  return result;
}

v8::Persistent<v8::Value> V8Bridge::evaluate(const String& filename,
                                             int baseLine,
                                             const String& code,
                                             Node* node) {
  v8::HandleScope hs;
  v8::Handle<v8::Context> context = V8Proxy::GetContext(m_proxy->frame());
  if (context.IsEmpty())
    return v8::Persistent<v8::Value>();

  v8::Context::Scope scope(context);

  v8::Local<v8::Value> obj = m_proxy->Evaluate(filename, baseLine, code, node);

  if (obj.IsEmpty()) return v8::Persistent<v8::Value>();

  // TODO(fqian): keep track the global handle created.
  return v8::Persistent<v8::Value>::New(obj);
}

void V8Bridge::disposeJSResult(v8::Persistent<v8::Value> result) {
  result.Dispose();
  result.Clear();
}

EventListener* V8Bridge::createHTMLEventHandler(
    const String& functionName, const String& code, Node* node) {
  return m_proxy->createHTMLEventHandler(functionName, code, node);
}

#if ENABLE(SVG)
EventListener* V8Bridge::createSVGEventHandler(
    const String& functionName, const String& code, Node* node) {
  return m_proxy->createSVGEventHandler(functionName, code, node);
}
#endif

void V8Bridge::setEventHandlerLineno(int lineno) {
  m_proxy->setEventHandlerLineno(lineno);
}

void V8Bridge::finishedWithEvent(Event* evt) {
  m_proxy->finishedWithEvent(evt);
}

void V8Bridge::clear() {
  m_proxy->clear();
}

void V8Bridge::clearDocumentWrapper() {
  m_proxy->clearDocumentWrapper();
}

// Create a V8 object with an interceptor of NPObjectPropertyGetter
void V8Bridge::BindToWindowObject(Frame* frame, const String& key,
                                  NPObject* object) {
  v8::HandleScope handle_scope;

  v8::Handle<v8::Context> context = V8Proxy::GetContext(frame);
  if (context.IsEmpty())
    return;

  v8::Context::Scope scope(context);

  v8::Handle<v8::Object> value = CreateV8ObjectForNPObject(object, NULL);

  // Attach to the global object
  v8::Handle<v8::Object> global = context->Global();
  global->Set(v8String(key), value);
}


void V8Bridge::CollectGarbage() {
  v8::HandleScope hs;
  v8::Handle<v8::Context> context = V8Proxy::GetContext(m_proxy->frame());
  if (context.IsEmpty()) return;

  v8::Context::Scope scope(context);

  m_proxy->Evaluate("", 0, "if (window.gc) void(gc());", NULL);
}


NPRuntimeFunctions *V8Bridge::functions() {
  return &npruntime_functions;
}


NPObject *V8Bridge::CreateScriptObject(Frame* frame) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Context> context = V8Proxy::GetContext(frame);
  if (context.IsEmpty())
    return 0;

  v8::Context::Scope scope(context);
  DOMWindow *window = frame->domWindow();
  v8::Handle<v8::Value> global =
      V8Proxy::ToV8Object(V8ClassIndex::DOMWINDOW, window);
  ASSERT(global->IsObject());
  return NPN_CreateScriptObject(0, v8::Handle<v8::Object>::Cast(global),
                                window);
}

NPObject *V8Bridge::CreateScriptObject(Frame* frame,
                                       HTMLPlugInElement* element) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Context> context = V8Proxy::GetContext(frame);
  if (context.IsEmpty())
    return 0;
  v8::Context::Scope scope(context);

  DOMWindow *window = frame->domWindow();
  v8::Handle<v8::Value> dom_win =
    V8Proxy::ToV8Object(V8ClassIndex::HTMLEMBEDELEMENT, element);
  if (dom_win->IsObject()) {
    return NPN_CreateScriptObject(0, v8::Handle<v8::Object>::Cast(dom_win),
                                  window);
  } else {
    return 0;
  }
}

NPObject *V8Bridge::CreateNoScriptObject() {
  return 0;  // implement me
}

bool V8Bridge::haveInterpreter() const {
  return m_proxy->ContextInitialized();
}

bool V8Bridge::isEnabled() const {
  return m_proxy->isEnabled();
}

JSInstanceHolder::JSInstanceHolder() : m_instance() { }

JSInstanceHolder::JSInstanceHolder(JSInstance instance) {
  *this = instance;
}

JSInstanceHolder::~JSInstanceHolder() {
  Clear();
}

bool JSInstanceHolder::IsEmpty() {
  return m_instance.IsEmpty();
}

JSInstance JSInstanceHolder::Get() {
  return v8::Local<v8::Object>::New(m_instance);
}

void JSInstanceHolder::Clear() {
  if (!m_instance.IsEmpty()) {
    v8::HandleScope scope;
    v8::Persistent<v8::Object> handle(m_instance);
#ifndef NDEBUG
    V8Proxy::UnregisterGlobalHandle(this, handle);
#endif
    handle.Dispose();
    m_instance.Clear();
  }
}

JSInstance JSInstanceHolder::EmptyInstance() {
  return v8::Local<v8::Object>();
}

JSInstanceHolder& JSInstanceHolder::operator=(JSInstance instance) {
  Clear();
  if (!instance.IsEmpty()) {
    v8::Persistent<v8::Object> handle =
        v8::Persistent<v8::Object>::New(instance);
    m_instance = handle;
#ifndef NDEBUG
    V8Proxy::RegisterGlobalHandle(JSINSTANCE, this, handle);
#endif
  }
  return *this;
}

}  // namespace WebCpre
