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
#include "ScriptController.h"

#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "GCController.h"
#include "JSEventListener.h"
#include "npruntime_impl.h"
#include "NP_jsobject.h"
#include "Page.h"
#include "PageGroup.h"
#include "runtime_root.h"
#include "Settings.h"

#include <kjs/debugger.h>
#include <kjs/JSLock.h>

#if ENABLE(NETSCAPE_PLUGIN_API)
#include "HTMLPlugInElement.h"
#endif

#if ENABLE(SVG)
#include "JSSVGLazyEventListener.h"
#endif

using namespace KJS;
using namespace WebCore::EventNames;

namespace WebCore {

bool ScriptController::m_recordPlaybackMode = false;

ScriptController::ScriptController(Frame* frame)
    : m_frame(frame)
    , m_handlerLineno(0)
    , m_sourceURL(0)
    , m_processingTimerCallback(false)
    , m_paused(false)
#if ENABLE(NETSCAPE_PLUGIN_API)
    , m_windowScriptNPObject(0)
#endif
#if PLATFORM(MAC)
    , m_windowScriptObject(0)
#endif
{
#if PLATFORM(MAC) && ENABLE(MAC_JAVA_BRIDGE)
    static bool initializedJavaJSBindings;
    if (!initializedJavaJSBindings) {
        initializedJavaJSBindings = true;
        initJavaJSBindings();
    }
#endif
}

ScriptController::~ScriptController()
{
    if (m_windowShell) {
        m_windowShell = 0;
    
        // It's likely that releasing the global object has created a lot of garbage.
        gcController().garbageCollectSoon();
    }

    disconnectPlatformScriptObjects();
}

// Evaluate a script file in the environment of this proxy.
String ScriptController::evaluate(const String& filename, int baseLine, const String& code, Node* node, bool* succ) {
    // Not implemented.
    ASSERT(false);
    return String();
}


// Evaluate a script file in the environment of this proxy.
JSResult ScriptController::evaluate(const String& filename, int baseLine, const String& code, Node* node) {
    // Not implemented.
    ASSERT(false);
    return 0;
}

void ScriptController::clearWindowShell()
{
    if (!m_windowShell)
        return;

    JSLock lock(false);
    m_windowShell->window()->clear();
    m_liveFormerWindows.add(m_windowShell->window());
    m_windowShell->setWindow(new (JSDOMWindow::commonJSGlobalData()) JSDOMWindow(m_frame->domWindow(), m_windowShell));
    if (Page* page = m_frame->page()) {
        attachDebugger(page->debugger());
        m_windowShell->window()->setProfileGroup(page->group().identifier());
    }

    // There is likely to be a lot of garbage now.
    gcController().garbageCollectSoon();
}

PassRefPtr<EventListener> ScriptController::createHTMLEventHandler(const String& functionName, const String& code, Node* node)
{
    initScriptIfNeeded();
    JSLock lock(false);
    return JSLazyEventListener::create(functionName, code, m_windowShell->window(), node, m_handlerLineno);
}

#if ENABLE(SVG)
PassRefPtr<EventListener> ScriptController::createSVGEventHandler(const String& functionName, const String& code, Node* node)
{
    initScriptIfNeeded();
    JSLock lock(false);
    return JSSVGLazyEventListener::create(functionName, code, m_windowShell->window(), node, m_handlerLineno);
}
#endif

void ScriptController::BindToWindowObject(Frame* frame, const String& key, NPObject* object) {
  // Not implemented.
  ASSERT(false);
}

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

NPRuntimeFunctions* ScriptController::functions() 
{
    return &npruntime_functions;
}

// static
Frame* ScriptController::retrieveActiveFrame() {
  // Not implemented.
  ASSERT(false);
  return 0;
}

// static
bool ScriptController::isSafeScript(Frame* target) {
  // Not implemented.
  ASSERT(false);
  return false;
}

// static
void ScriptController::setDomain(Frame*, const String&) {
    // empty
}

// static
void ScriptController::setFlags(const char*, int) {
    // empty 
}

// static
void ScriptController::gcProtectJSWrapper(void* dom_object) {
    KJS::JSLock lock(false);
    KJS::gcProtectNullTolerant(ScriptInterpreter::getDOMObject(dom_object));
}

// static
void ScriptController::gcUnprotectJSWrapper(void* dom_object) {
    KJS::JSLock lock(false);
    KJS::gcUnprotectNullTolerant(ScriptInterpreter::getDOMObject(dom_object));
}

void ScriptController::finishedWithEvent(Event* event)
{
    // This is called when the DOM implementation has finished with a particular event. This
    // is the case in sitations where an event has been created just for temporary usage,
    // e.g. an image load or mouse move. Once the event has been dispatched, it is forgotten
    // by the DOM implementation and so does not need to be cached still by the interpreter
    ScriptInterpreter::forgetDOMObject(event);
}

void ScriptController::setEventHandlerLineno(int lineno) {
    m_handlerLineno = lineno;
}

void ScriptController::initScript()
{
    if (m_windowShell)
        return;

    JSLock lock(false);

    m_windowShell = new JSDOMWindowShell(m_frame->domWindow());
    updateDocument();

    if (Page* page = m_frame->page()) {
        attachDebugger(page->debugger());
        m_windowShell->window()->setProfileGroup(page->group().identifier());
    }

    m_frame->loader()->dispatchWindowObjectAvailable();
}

void ScriptController::disconnectFrame()
{
    // Not implemented.
    ASSERT(false);
}

bool ScriptController::processingUserGesture() const
{
    if (!m_windowShell)
        return false;

    if (Event* event = m_windowShell->window()->currentEvent()) {
        const AtomicString& type = event->type();
        if ( // mouse events
            type == clickEvent || type == mousedownEvent ||
            type == mouseupEvent || type == dblclickEvent ||
            // keyboard events
            type == keydownEvent || type == keypressEvent ||
            type == keyupEvent ||
            // other accepted events
            type == selectEvent || type == changeEvent ||
            type == focusEvent || type == blurEvent ||
            type == submitEvent)
            return true;
    } else { // no event
        if (m_sourceURL && m_sourceURL->isNull() && !m_processingTimerCallback) {
            // This is the <a href="javascript:window.open('...')> case -> we let it through
            return true;
        }
        // This is the <script>window.open(...)</script> case or a timer callback -> block it
    }
    return false;
}

bool ScriptController::isEnabled() const
{
    Settings* settings = m_frame->settings();
    return (settings && settings->isJavaScriptEnabled());
}

void ScriptController::attachDebugger(void* debugger_voidptr)
{
    if (!m_windowShell)
        return;

    KJS::Debugger* debugger = reinterpret_cast<KJS::Debugger*>(debugger_voidptr);
    if (debugger)
        debugger->attach(m_windowShell->window());
    else if (KJS::Debugger* currentDebugger = m_windowShell->window()->debugger())
        currentDebugger->detach(m_windowShell->window());
}

Bindings::RootObject* ScriptController::bindingRootObject()
{
    if (!isEnabled())
        return 0;

    if (!m_bindingRootObject) {
        JSLock lock(false);
        m_bindingRootObject = Bindings::RootObject::create(0, globalObject());
    }
    return m_bindingRootObject.get();
}

#if ENABLE(NETSCAPE_PLUGIN_API)
NPObject* ScriptController::windowScriptNPObject()
{
    if (!m_windowScriptNPObject) {
        if (isEnabled()) {
            // JavaScript is enabled, so there is a JavaScript window object.
            // Return an NPObject bound to the window object.
            KJS::JSLock lock(false);
            JSObject* win = windowShell()->window();
            ASSERT(win);
            Bindings::RootObject* root = bindingRootObject();
            m_windowScriptNPObject = _NPN_CreateScriptObject(0, win, root);
        } else {
            // JavaScript is not enabled, so we cannot bind the NPObject to the JavaScript window object.
            // Instead, we create an NPObject of a different class, one which is not bound to a JavaScript object.
            m_windowScriptNPObject = _NPN_CreateNoScriptObject();
        }
    }

    return m_windowScriptNPObject;
}

NPObject* ScriptController::createScriptObjectForPluginElement(HTMLPlugInElement* plugin)
{
    // Can't create NPObjects when JavaScript is disabled
    if (!isEnabled())
        return _NPN_CreateNoScriptObject();

    // Create a JSObject bound to this element
    JSLock lock(false);
    ExecState* exec = globalObject()->globalExec();
    JSValue* jsElementValue = toJS(exec, plugin);
    if (!jsElementValue || !jsElementValue->isObject())
        return _NPN_CreateNoScriptObject();

    // Wrap the JSObject in an NPObject
    return _NPN_CreateScriptObject(0, jsElementValue->getObject(), bindingRootObject());
}
#endif

#if !PLATFORM(MAC)
void ScriptController::clearPlatformScriptObjects()
{
}

void ScriptController::disconnectPlatformScriptObjects()
{
}
#endif

void ScriptController::cleanupScriptObjectsForPlugin(void* nativeHandle)
{
    RootObjectMap::iterator it = m_rootObjects.find(nativeHandle);

    if (it == m_rootObjects.end())
        return;

    it->second->invalidate();
    m_rootObjects.remove(it);
}

void ScriptController::clearScriptObjects()
{
    JSLock lock(false);

    RootObjectMap::const_iterator end = m_rootObjects.end();
    for (RootObjectMap::const_iterator it = m_rootObjects.begin(); it != end; ++it)
        it->second->invalidate();

    m_rootObjects.clear();

    if (m_bindingRootObject) {
        m_bindingRootObject->invalidate();
        m_bindingRootObject = 0;
    }

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (m_windowScriptNPObject) {
        // Call _NPN_DeallocateObject() instead of _NPN_ReleaseObject() so that we don't leak if a plugin fails to release the window
        // script object properly.
        // This shouldn't cause any problems for plugins since they should have already been stopped and destroyed at this point.
        _NPN_DeallocateObject(m_windowScriptNPObject);
        m_windowScriptNPObject = 0;
    }
#endif

    clearPlatformScriptObjects();
}

void ScriptController::pauseTimeouts(OwnPtr<PausedTimeouts>& result)
{
    if (!haveWindowShell()) {
        result.clear();
        return;
    }

    windowShell()->window()->pauseTimeouts(result);
}

void ScriptController::resumeTimeouts(OwnPtr<PausedTimeouts>& pausedTimeouts)
{
    if (!haveWindowShell()) {
        // Callers can assume we will always clear the passed in timeouts
        pausedTimeouts.clear();
        return;
    }

    windowShell()->window()->resumeTimeouts(pausedTimeouts);
}

// JSInstanceHolder
JSInstanceHolder::JSInstanceHolder()
    : m_instance(0) {
}

JSInstanceHolder::JSInstanceHolder(JSInstanceHandle instance) {
    m_instance = instance;
}

JSInstanceHolder::~JSInstanceHolder() {
    Clear();
}

bool JSInstanceHolder::IsEmpty() {
    return m_instance == 0;
}

void JSInstanceHolder::Clear() {
    m_instance = 0;
}

JSInstance JSInstanceHolder::Get() {
    return m_instance.get();
}

JSInstance JSInstanceHolder::EmptyInstance() {
    return 0;
}

JSInstanceHolder& JSInstanceHolder::operator=(JSInstanceHandle instance) {
    m_instance = instance;
    return *this;
}

}  // namespace WebCore

// KJS should only expose functions declared in npruntime.h (NPN_*)
// and npruntime_priv.h (which is an extension of npruntime.h), and
// not exposing _NPN_* functions declared in npruntime_impl.h.
// KJSBridge implements NPN_* functions by wrapping around _NPN_* functions.
// 
// Following styles in JavaScriptCore/bindings/npruntime.cpp
void NPN_ReleaseVariantValue(NPVariant* variant) {
    _NPN_ReleaseVariantValue(variant);
}

NPIdentifier NPN_GetStringIdentifier(const NPUTF8* name) {
    return _NPN_GetStringIdentifier(name);
}

void NPN_GetStringIdentifiers(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers) {
    _NPN_GetStringIdentifiers(names, nameCount, identifiers);
}

NPIdentifier NPN_GetIntIdentifier(int32_t intid) {
    return _NPN_GetIntIdentifier(intid);
}

bool NPN_IdentifierIsString(NPIdentifier identifier) {
    return _NPN_IdentifierIsString(identifier);
}

NPUTF8* NPN_UTF8FromIdentifier(NPIdentifier identifier) {
    return _NPN_UTF8FromIdentifier(identifier);
}

int32_t NPN_IntFromIdentifier(NPIdentifier identifier) {
    return _NPN_IntFromIdentifier(identifier);
}

NPObject* NPN_CreateObject(NPP npp, NPClass* aClass) {
    return _NPN_CreateObject(npp, aClass);
}

NPObject* NPN_RetainObject(NPObject* obj) {
    return _NPN_RetainObject(obj);
}

void NPN_ReleaseObject(NPObject* obj) {
    _NPN_ReleaseObject(obj);
}

void NPN_DeallocateObject(NPObject* obj) {
    _NPN_DeallocateObject(obj);
}

bool NPN_Invoke(NPP npp, NPObject* npobj, NPIdentifier methodName, const NPVariant* args, uint32_t argCount, NPVariant* result) {
    return _NPN_Invoke(npp, npobj, methodName, args, argCount, result);
}

bool NPN_InvokeDefault(NPP npp, NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result) {
    return _NPN_InvokeDefault(npp, npobj, args, argCount, result);
}

bool NPN_Evaluate(NPP npp, NPObject* npobj, NPString* script, NPVariant* result) {
    return _NPN_Evaluate(npp, npobj, script, result);
}

bool NPN_GetProperty(NPP npp, NPObject* npobj, NPIdentifier propertyName, NPVariant* result) {
    return _NPN_GetProperty(npp, npobj, propertyName, result);
}

bool NPN_SetProperty(NPP npp, NPObject* npobj, NPIdentifier propertyName, const NPVariant* value) {
    return _NPN_SetProperty(npp, npobj, propertyName, value);
}

bool NPN_RemoveProperty(NPP npp, NPObject* npobj, NPIdentifier propertyName) {
    return _NPN_RemoveProperty(npp, npobj, propertyName);
}

bool NPN_HasProperty(NPP npp, NPObject* npobj, NPIdentifier propertyName) {
    return _NPN_HasProperty(npp, npobj, propertyName);
}

bool NPN_HasMethod(NPP npp, NPObject* npobj, NPIdentifier methodName) {
    return _NPN_HasMethod(npp, npobj, methodName);
}

void NPN_SetException(NPObject* obj, const NPUTF8* message) {
    _NPN_SetException(obj, message);
}

bool NPN_Enumerate(NPP npp, NPObject* npobj, NPIdentifier** identifier, uint32_t* count) {
    return _NPN_Enumerate(npp, npobj, identifier, count);
}
