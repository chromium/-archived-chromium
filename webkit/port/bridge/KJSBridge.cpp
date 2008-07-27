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
#include "Frame.h"
#include "EventListener.h"
#include "HTMLPlugInElement.h"
#include "Node.h"
#include "kjs_dom.h"
#include "kjs/JSLock.h"
#include "kjs/object.h"
#include "KJSBridge.h"
#include "kjs_window.h"

#include "bindings/npruntime.h"
#include "npruntime_impl.h"
#include "npruntime_priv.h"

#include "bindings/runtime.h"
#include "bindings/runtime_root.h"
#include "NP_jsobject.h"

namespace WebCore {

bool JSBridge::m_recordPlaybackMode = false;

// static
void JSBridge::setFlags(const char*, int) {
  // empty 
}

// static
void JSBridge::setDomain(Frame*, const String&) {
  // empty
}

// static
Frame* JSBridge::retrieveActiveFrame() {
  // Not implemented.
  ASSERT(false);
  return 0;
}

// static
bool JSBridge::isSafeScript(Frame* target) {
  // Not implemented.
  ASSERT(false);
  return false;
}

// static
void JSBridge::gcProtectJSWrapper(void* dom_object) {
  KJS::JSLock lock;
  KJS::gcProtectNullTolerant(KJS::ScriptInterpreter::getDOMObject(dom_object));
}

// static
void JSBridge::gcUnprotectJSWrapper(void* dom_object) {
  KJS::JSLock lock;
  KJS::gcUnprotectNullTolerant(KJS::ScriptInterpreter::getDOMObject(dom_object));
}

// static
JSException JSBridge::NoException() {
  return 0;
}

// static
bool JSBridge::IsException(JSException exception) {
  return exception != 0;
}

// static
PausedTimeouts* JSBridge::pauseTimeouts(Frame* frame) {
  if (!frame) 
    return NULL;
  
  KJS::Window* window = KJS::Window::retrieveWindow(frame);
  if (!window)
    return NULL;

  return window->pauseTimeouts();
}

// static
void JSBridge::resumeTimeouts(Frame* frame, PausedTimeouts* timeouts) {
  if (!frame)
    return;

  KJS::Window* window = KJS::Window::retrieveWindow(frame);
  if (window)
      window->resumeTimeouts(timeouts);
}

void KJSBridge::disconnectFrame()
{
  if (m_proxy->haveGlobalObject()) {
    static_cast<KJS::Window*>(m_proxy->globalObject())->disconnectFrame();
  }
}

bool KJSBridge::wasRunByUserGesture() {
    return m_proxy->processingUserGesture();
}


// Evaluate a script file in the environment of this proxy.
String KJSBridge::evaluate(const String& filename, int baseLine,
                           const String& code, Node* node, bool* succ) {
  *succ = false;

  KJS::JSValue* value = m_proxy->evaluate(filename, baseLine, code);

  if (!value || value->isUndefined())
    return String();

  KJS::JSLock lock;
  KJS::ExecState* exec = m_proxy->globalObject()->globalExec();
  KJS::UString ustring = value->toString(exec);
  exec->clearException();
  *succ = true;
  return String(ustring);
}


// Evaluate a script file in the environment of this proxy.
JSResult KJSBridge::evaluate(const String& filename, int baseLine,
                             const String& code, Node* node) {
  return m_proxy->evaluate(filename, baseLine, code);
}


EventListener* KJSBridge::createHTMLEventHandler(const String& functionName,
                                                 const String& code, Node* node) {
  return m_proxy->createHTMLEventHandler(functionName, code, node);
}

#if ENABLE(SVG)
EventListener* KJSBridge::createSVGEventHandler(const String& functionName,
                                                 const String& code, Node* node) {
  return m_proxy->createSVGEventHandler(functionName, code, node);
}
#endif

void KJSBridge::finishedWithEvent(Event* evt) {
  m_proxy->finishedWithEvent(evt);
}


void KJSBridge::setEventHandlerLineno(int lineno) {
  m_proxy->setEventHandlerLineno(lineno);
}

void KJSBridge::clear() {
  m_proxy->clear();
}

// JSInstanceHolder
JSInstanceHolder::JSInstanceHolder() :
  m_instance(0) {
}

JSInstanceHolder::JSInstanceHolder(JSInstance instance) {
  *this = instance;
}

JSInstanceHolder::~JSInstanceHolder() {
  Clear();
}

bool JSInstanceHolder::IsEmpty() {
  return m_instance == 0;
}

void JSInstanceHolder::Clear() {
  if (m_instance) {
    m_instance->deref();
    m_instance = 0;
  }
}

JSInstance JSInstanceHolder::Get() {
  return m_instance;
}

JSInstance JSInstanceHolder::EmptyInstance() {
  return 0;
}

JSInstanceHolder& JSInstanceHolder::operator=(JSInstance instance) {
  Clear();
  m_instance = instance;
  if (m_instance)
    m_instance->ref();
  return *this;
}

/*
JSRootObject* KJSBridge::getRootObject() {
  KJS::JSLock lock;
  PassRefPtr<KJS::Bindings::RootObject> object = 
      KJS::Bindings::RootObject::create(0, m_proxy->interpreter());
  return ToWebCoreJSRootObject(object.releaseRef());
}
*/

NPRuntimeFunctions npruntime_functions = {
  _NPN_GetStringIdentifier,
  _NPN_GetStringIdentifiers,
  _NPN_GetIntIdentifier,
  _NPN_IdentifierIsString,
  _NPN_UTF8FromIdentifier,
  _NPN_IntFromIdentifier,
  _NPN_CreateObject,
  _NPN_RetainObject,
  _NPN_ReleaseObject,
  _NPN_Invoke,
  _NPN_InvokeDefault,
  _NPN_Evaluate,
  _NPN_GetProperty,
  _NPN_SetProperty,
  _NPN_RemoveProperty,
  _NPN_HasProperty,
  _NPN_HasMethod,
  _NPN_ReleaseVariantValue,
  _NPN_SetException
};

NPRuntimeFunctions *KJSBridge::functions() 
{
  return &npruntime_functions;
}

void KJSBridge::BindToWindowObject(Frame* frame, const String& key, NPObject* object) {
  KJS::Bindings::RootObject *root = frame->bindingRootObject();
  KJS::ExecState *exec = root->globalObject()->globalExec();
  ASSERT(exec);
  KJS::JSObject *rootObject = root->globalObject();
  KJS::JSObject *window = rootObject->get(exec, KJS::Identifier("window"))->getObject();
  ASSERT(window);

  KJS::JSObject *runtimeObject = 
          KJS::Bindings::Instance::createRuntimeObject(
              KJS::Bindings::Instance::CLanguage, object, root);

  window->put(exec, key, runtimeObject);
}

NPObject *KJSBridge::CreateScriptObject(Frame* frame)
{
  KJS::JSObject *jsObject = KJS::Window::retrieveWindow(frame);
  ASSERT(jsObject);

  // Wrap the JSObject in an NPObject
  KJS::Bindings::RootObject* rootObject = frame->bindingRootObject();
  return _NPN_CreateScriptObject(0, jsObject, rootObject);
}

NPObject *KJSBridge::CreateScriptObject(Frame* frame, 
                                        HTMLPlugInElement *element)
{
  KJS::JSLock lock;
  KJS::ExecState *exec = frame->scriptProxy()->globalObject()->globalExec();
  //KJS::JSObject* wrappedObject = static_cast<KJS::JSObject*>(object);
  //PassRefPtr<KJS::JSObject> wrappedObject = static_cast<KJS::JSObject*>(object);
  KJS::JSValue* jsElementValue = toJS(exec, element);
  return CreateScriptObject(frame, jsElementValue);
}

NPObject *KJSBridge::CreateScriptObject(Frame* frame, KJS::JSValue* value)
{
  if (!value || !value->isObject())
    return _NPN_CreateNoScriptObject();

  KJS::JSObject *jsObject = value->getObject();

  // Wrap the JSObject in an NPObject
  KJS::Bindings::RootObject* rootObject = frame->bindingRootObject();
  return _NPN_CreateScriptObject(0, jsObject, rootObject);
}

NPObject *KJSBridge::CreateNoScriptObject()
{
  return _NPN_CreateNoScriptObject();
}
}  // namespace WebCore

// KJS should only expose functions declared in npruntime.h (NPN_*)
// and npruntime_priv.h (which is an extension of npruntime.h), and
// not exposing _NPN_* functions declared in npruntime_impl.h.
// KJSBridge implements NPN_* functions by wrapping around _NPN_* functions.
// 
// Following styles in JavaScriptCore/bindings/npruntime.cpp
void NPN_ReleaseVariantValue(NPVariant *variant) {
  _NPN_ReleaseVariantValue(variant);
}

NPIdentifier NPN_GetStringIdentifier(const NPUTF8 *name) {
  return _NPN_GetStringIdentifier(name);
}

void NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount,
                              NPIdentifier *identifiers) {
  _NPN_GetStringIdentifiers(names, nameCount, identifiers);
}

NPIdentifier NPN_GetIntIdentifier(int32_t intid) {
  return _NPN_GetIntIdentifier(intid);
}

bool NPN_IdentifierIsString(NPIdentifier identifier) {
  return _NPN_IdentifierIsString(identifier);
}

NPUTF8 *NPN_UTF8FromIdentifier(NPIdentifier identifier) {
  return _NPN_UTF8FromIdentifier(identifier);
}

int32_t NPN_IntFromIdentifier(NPIdentifier identifier) {
  return _NPN_IntFromIdentifier(identifier);
}

NPObject *NPN_CreateObject(NPP npp, NPClass *aClass) {
  return _NPN_CreateObject(npp, aClass);
}

NPObject *NPN_RetainObject(NPObject *obj) {
  return _NPN_RetainObject(obj);
}

void NPN_ReleaseObject(NPObject *obj) {
  _NPN_ReleaseObject(obj);
}

void NPN_DeallocateObject(NPObject *obj) {
  _NPN_DeallocateObject(obj);
}

bool NPN_Invoke(NPP npp, NPObject *npobj, NPIdentifier methodName,
                const NPVariant *args, uint32_t argCount, NPVariant *result) {
  return _NPN_Invoke(npp, npobj, methodName, args, argCount, result);
}

bool NPN_InvokeDefault(NPP npp, NPObject *npobj, const NPVariant *args,
                       uint32_t argCount, NPVariant *result) {
  return _NPN_InvokeDefault(npp, npobj, args, argCount, result);
}

bool NPN_Evaluate(NPP npp, NPObject *npobj, NPString *script, NPVariant *result) {
  return _NPN_Evaluate(npp, npobj, script, result);
}

bool NPN_GetProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName,
                     NPVariant *result) {
  return _NPN_GetProperty(npp, npobj, propertyName, result);
}

bool NPN_SetProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName,
                     const NPVariant *value) {
  return _NPN_SetProperty(npp, npobj, propertyName, value);
}

bool NPN_RemoveProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName) {
  return _NPN_RemoveProperty(npp, npobj, propertyName);
}

bool NPN_HasProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName) {
  return _NPN_HasProperty(npp, npobj, propertyName);
}

bool NPN_HasMethod(NPP npp, NPObject *npobj, NPIdentifier methodName) {
  return _NPN_HasMethod(npp, npobj, methodName);
}

void NPN_SetException(NPObject *obj, const NPUTF8 *message) {
  _NPN_SetException(obj, message);
}

bool NPN_Enumerate(NPP npp, NPObject *npobj, NPIdentifier **identifier,
                   uint32_t *count) {
  return _NPN_Enumerate(npp, npobj, identifier, count);
}
