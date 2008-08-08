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

#ifndef JSBridge_h
#define JSBridge_h

#include "third_party/npapi/bindings/npruntime.h"
#if USE(JAVASCRIPTCORE_BINDINGS)
#include <kjs/ustring.h>
#endif

#if USE(V8_BINDING)
#include "v8.h"
//namespace v8 { class Object; }
#endif

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

#if USE(JAVASCRIPTCORE_BINDINGS)
namespace KJS { namespace Bindings { class RootObject;  class Instance; } 
}
#endif


namespace WebCore {
class Document;
class Frame;
class String;
class Node;
class EventListener;
class Event;
class HTMLPlugInElement;
class PausedTimeouts;

// JSString is the string class used for XMLHttpRequest's
// m_responseText field.
#if USE(JAVASCRIPTCORE_BINDINGS)
typedef KJS::UString JSString;
typedef KJS::Bindings::Instance* JSInstance;
typedef KJS::Bindings::Instance* JSPersistentInstance;
typedef KJS::JSValue* JSException;
typedef KJS::JSValue* JSResult;
#endif

#if USE(V8_BINDING)
typedef String JSString;
typedef v8::Local<v8::Object> JSInstance;
typedef v8::Persistent<v8::Object> JSPersistentInstance;
typedef v8::Local<v8::Value> JSException;
typedef v8::Persistent<v8::Value> JSResult;
#endif

class JSBridge {
 public:
  virtual ~JSBridge() { }

  // Disconnects the proxy from its owner frame.
  virtual void disconnectFrame() = 0;

  virtual bool wasRunByUserGesture() = 0;

  
  // Evaluate a script file in the environment of this proxy.
  // If succeeded, 'succ' is set to true and result is returned
  // as a string.
  virtual String evaluate(const String& filename, int baseLine,
                          const String& code, Node*, bool* succ) = 0;

  // Second API function for evaluating a JS code.
  // It returns a JSResult which must be disposed by calling
  // disposeJSResult. If the result is not disposed, it can cause
  // serious memory leak. The caller determines whether the evaluation
  // is successful by checking the value of JSResult.
  virtual JSResult evaluate(const String& filename, int baseLine,
                            const String& code, Node*) = 0;
  virtual void disposeJSResult(JSResult result) = 0;

  virtual EventListener* createHTMLEventHandler(const String& functionName,
                                        const String& code, Node* node) = 0;

#if ENABLE(SVG)
  virtual EventListener* createSVGEventHandler(const String& functionName,
                                        const String& code, Node* node) = 0;
#endif
  
  virtual void setEventHandlerLineno(int lineno) = 0;
  virtual void finishedWithEvent(Event*) = 0;

  virtual void clear() = 0;

  // Get the Root object
  // virtual JSRootObject* getRootObject() = 0;
  // Creates a property of the global object of a frame.
  virtual void BindToWindowObject(Frame* frame, const String& key, NPObject* object) = 0;

  // Provides access to the NPRuntime functions.
  virtual NPRuntimeFunctions *functions() = 0;

  // Create an NPObject for the window object.
  virtual NPObject *CreateScriptObject(Frame*) = 0;

  // Create an NPObject for an HTMLPluginElement
  virtual NPObject *CreateScriptObject(Frame*, HTMLPlugInElement*) = 0;
  
  // Create a "NoScript" object (used when JS is unavailable or disabled)
  virtual NPObject *CreateNoScriptObject() = 0;

  // Check if the javascript engine has been initialized.
  virtual bool haveInterpreter() const = 0;

  virtual bool isEnabled() const = 0;

  virtual void clearDocumentWrapper() = 0;

  virtual void CollectGarbage() = 0;

  // Create a NPObject wrapper for a JSObject
  //virtual NPObject *WrapScriptObject(NPP pluginId, JSObject* objectToWrap,
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
  static void setDomain(Frame* target, const String& new_domain);

  // Pass flags to the JS engine
  static void setFlags(const char* str, int length);

  // Protect and unprotect the JS wrapper from garbage collected.
  static void gcProtectJSWrapper(void* dom_object);
  static void gcUnprotectJSWrapper(void* dom_Object);

  // Returns a non-exception code object.
  static JSException NoException();
  // Returns true if the parameter is a JS exception object.
  static bool IsException(JSException exception);

  // Get/Set RecordPlaybackMode flag.
  // This is a special mode where JS helps the browser implement
  // playback/record mode.  Generally, in this mode, some functions
  // of client-side randomness are removed.  For example, in
  // this mode Math.random() and Date.getTime() may not return
  // values which vary.
  static bool RecordPlaybackMode() { return m_recordPlaybackMode; }
  static void setRecordPlaybackMode(bool value) {
      m_recordPlaybackMode = value;
  }

  // Pause timeouts for a frame.
  static PausedTimeouts* pauseTimeouts(Frame* frame);
  // Resume timeouts for a frame.
  static void resumeTimeouts(Frame* frame, PausedTimeouts* timeouts);

 private:
  static bool m_recordPlaybackMode;
};

// JSInstance is an abstraction for a wrapped C class.  KJS and V8
// have very different implementations.
class JSInstanceHolder {
 public:
   JSInstanceHolder();
   JSInstanceHolder(JSInstance instance);
   ~JSInstanceHolder();
   // Returns true if the holder is empty.
   bool IsEmpty();
   // Get the contained JSInstance.
   JSInstance Get();
   // Clear the contained JSInstance.
   void Clear();
   JSInstanceHolder& operator=(JSInstance instance);
   static JSInstance EmptyInstance();
 private:
   JSPersistentInstance m_instance;
};

}  // namespace WebCore

#endif  // JSBridge_h
