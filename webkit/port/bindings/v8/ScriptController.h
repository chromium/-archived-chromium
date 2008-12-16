// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
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

// An interface to abstract implementation differences
// for various Javascript engines.

#ifndef ScriptController_h
#define ScriptController_h

#include "HashMap.h"
#include "MessagePort.h"
#include "ScriptInstance.h"
#include "ScriptValue.h"
#include "SecurityOrigin.h"

#include "bindings/npruntime.h"

#include <wtf/HashMap.h>

#include "v8.h"
#include "v8_proxy.h"

// JavaScript implementations which expose NPObject will need to implement
// these methods.
typedef void        (*NPN_ReleaseVariantValueProcPtr) (NPVariant *variant);

typedef NPIdentifier(*NPN_GetStringIdentifierProcPtr) (const NPUTF8 *name);
typedef void        (*NPN_GetStringIdentifiersProcPtr) (const NPUTF8 **names,
                         int32_t nameCount,
                         NPIdentifier *identifiers);
typedef NPIdentifier(*NPN_GetIntIdentifierProcPtr) (int32_t intid);
typedef int32_t     (*NPN_IntFromIdentifierProcPtr) (NPIdentifier identifier);
typedef bool        (*NPN_IdentifierIsStringProcPtr) (NPIdentifier identifier);
typedef NPUTF8 *    (*NPN_UTF8FromIdentifierProcPtr) (NPIdentifier identifier);

typedef NPObject*   (*NPN_CreateObjectProcPtr) (NPP,
                         NPClass *aClass);
typedef NPObject*   (*NPN_RetainObjectProcPtr) (NPObject *obj);
typedef void        (*NPN_ReleaseObjectProcPtr) (NPObject *obj);
typedef bool        (*NPN_InvokeProcPtr) (NPP npp,
                         NPObject *obj,
                         NPIdentifier methodName,
                         const NPVariant *args,
                         unsigned argCount,
                         NPVariant *result);
typedef bool        (*NPN_InvokeDefaultProcPtr) (NPP npp,
                         NPObject *obj,
                         const NPVariant *args,
                         unsigned argCount,
                         NPVariant *result);
typedef bool        (*NPN_EvaluateProcPtr) (NPP npp,
                         NPObject *obj,
                         NPString *script,
                         NPVariant *result);
typedef bool        (*NPN_GetPropertyProcPtr) (NPP npp,
                         NPObject *obj,
                         NPIdentifier propertyName,
                         NPVariant *result);
typedef bool        (*NPN_SetPropertyProcPtr) (NPP npp,
                         NPObject *obj,
                         NPIdentifier propertyName,
                         const NPVariant *value);
typedef bool        (*NPN_HasPropertyProcPtr) (NPP,
                         NPObject *npobj,
                         NPIdentifier propertyName);
typedef bool        (*NPN_HasMethodProcPtr) (NPP npp,
                         NPObject *npobj,
                         NPIdentifier methodName);
typedef bool        (*NPN_RemovePropertyProcPtr) (NPP npp,
                         NPObject *obj,
                         NPIdentifier propertyName);
typedef void        (*NPN_SetExceptionProcPtr) (NPObject *obj,
                         const NPUTF8 *message);

typedef struct _NPRuntimeFunctions {
    NPN_GetStringIdentifierProcPtr getStringIdentifier;
    NPN_GetStringIdentifiersProcPtr getStringIdentifiers;
    NPN_GetIntIdentifierProcPtr getIntIdentifier;
    NPN_IdentifierIsStringProcPtr identifierIsString;
    NPN_UTF8FromIdentifierProcPtr utf8FromIdentifier;
    NPN_IntFromIdentifierProcPtr intFromIdentifier;
    NPN_CreateObjectProcPtr createObject;
    NPN_RetainObjectProcPtr retainObject;
    NPN_ReleaseObjectProcPtr releaseObject;
    NPN_InvokeProcPtr invoke;
    NPN_InvokeDefaultProcPtr invokeDefault;
    NPN_EvaluateProcPtr evaluate;
    NPN_GetPropertyProcPtr getProperty;
    NPN_SetPropertyProcPtr setProperty;
    NPN_RemovePropertyProcPtr removeProperty;
    NPN_HasPropertyProcPtr hasProperty;
    NPN_HasMethodProcPtr hasMethod;
    NPN_ReleaseVariantValueProcPtr releaseVariantValue;
    NPN_SetExceptionProcPtr setException;
} NPRuntimeFunctions;

namespace WebCore {
class Document;
class EventListener;
class Event;
class Frame;
class HTMLPlugInElement;
class Node;
class ScriptSourceCode;
class String;
class Widget;

typedef v8::Local<v8::Object> JSInstance;
typedef v8::Local<v8::Object> JSInstanceHandle;
typedef v8::Persistent<v8::Object> JSPersistentInstance;
typedef v8::Local<v8::Value> JSException;
typedef v8::Persistent<v8::Value> JSResult;

class ScriptController {
public:
    ScriptController(Frame*);
    ~ScriptController();

    // TODO(eseidel): V8Proxy should either be folded into ScriptController
    // or this accessor should be made JSProxy*
    V8Proxy* proxy() { return m_proxy.get(); }

    // Evaluate a script file in the environment of this proxy.
    // If succeeded, 'succ' is set to true and result is returned
    // as a string.
    ScriptValue evaluate(const ScriptSourceCode&);

    void disposeJSResult(JSResult result);
    void collectGarbage();

    PassRefPtr<EventListener> createInlineEventListener(const String& functionName, const String& code, Node*);
#if ENABLE(SVG)
    PassRefPtr<EventListener> createSVGEventHandler(const String& functionName, const String& code, Node*);
#endif

    // Creates a property of the global object of a frame.
    void BindToWindowObject(Frame*, const String& key, NPObject*);

    NPRuntimeFunctions* functions();

    PassScriptInstance createScriptInstanceForWidget(Widget*);

    void clearPluginObjects();
    void disconnectFrame();

    // Check if the javascript engine has been initialized.
    bool haveInterpreter() const;

    bool isEnabled() const;

    // TODO(eseidel): void* is a compile hack
    void attachDebugger(void*);

    // Create a NPObject wrapper for a JSObject
    // NPObject *WrapScriptObject(NPP pluginId, JSObject* objectToWrap,
    //                           JSRootObject* originRootObject,
    //                           JSRootObject* rootObject);

    // --- Static methods assume we are running VM in single thread, ---
    // --- and there is only one VM instance.                        ---

    // Returns the frame of the calling code is in.
    // Not necessary the frame of this proxy.
    // For example, JS code in frame A calls windowB.open(...).
    // Window::open method has the frame pointer of B, but
    // the execution context is in frame A, so it needs
    // frame A's loader to complete URL.
    static Frame* retrieveActiveFrame();

    // Check whether it is safe to access a frame in another domain.
    static bool isSafeScript(Frame* target);

    // Tell the proxy that document.domain is set.
    static void setDomain(Frame* target, const String& newDomain);

    // Pass command-line flags to the JS engine
    static void setFlags(const char* str, int length);

    // Protect and unprotect the JS wrapper from garbage collected.
    static void gcProtectJSWrapper(void* object);
    static void gcUnprotectJSWrapper(void* object);

    // Get/Set RecordPlaybackMode flag.
    // This is a special mode where JS helps the browser implement
    // playback/record mode.  Generally, in this mode, some functions
    // of client-side randomness are removed.  For example, in
    // this mode Math.random() and Date.getTime() may not return
    // values which vary.
    static bool RecordPlaybackMode() { return m_recordPlaybackMode; }
    static void setRecordPlaybackMode(bool value) { m_recordPlaybackMode = value; }

    // Set/Get ShouldExposeGCController flag.
    // Some WebKit layout test need window.GCController.collect() to
    // trigger GC, this flag lets the binding code expose
    // window.GCController.collect() to the JavaScript code.
    // 
    // GCController.collect() needs V8 engine expose gc() function by passing
    // '--expose-gc' flag to the engine.
    static bool shouldExposeGCController() {
        return m_shouldExposeGCController;
    }
    static void setShouldExposeGCController(bool value) {
        m_shouldExposeGCController = value;
    }

    void finishedWithEvent(Event*);
    void setEventHandlerLineno(int lineno);

    void setProcessingTimerCallback(bool b) { m_processingTimerCallback = b; }
    bool processingUserGesture() const;

    void setPaused(bool b) { m_paused = b; }
    bool isPaused() const { return m_paused; }

    const String* sourceURL() const { return m_sourceURL; } // 0 if we are not evaluating any script

    void clearWindowShell();
    void updateDocument();

    void updateSecurityOrigin();
    void clearScriptObjects();
    void updatePlatformScriptObjects();
    void cleanupScriptObjectsForPlugin(void*);

#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* createScriptObjectForPluginElement(HTMLPlugInElement*);
    NPObject* windowScriptNPObject();
#endif

private:
    static bool m_recordPlaybackMode;
    static bool m_shouldExposeGCController;

    Frame* m_frame;
    const String* m_sourceURL;

    bool m_processingTimerCallback;
    bool m_paused;

    OwnPtr<V8Proxy> m_proxy;
    typedef HashMap<void*, NPObject*> PluginObjectMap;

    // A mapping between Widgets and their corresponding script object.
    // This list is used so that when the plugin dies, we can immediately
    // invalidate all sub-objects which are associated with that plugin.
    // The frame keeps a NPObject reference for each item on the list.
    PluginObjectMap m_pluginObjects;
#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* m_windowScriptNPObject;
#endif
};

}  // namespace WebCore

#endif  // ScriptController_h
