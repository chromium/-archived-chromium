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

// This file contains KJS specific bits required to build WebKit.

#ifndef KJSBridge_h
#define KJSBridge_h

#include "JSBridge.h"
#include "kjs_binding.h"
#include "kjs_proxy.h"

namespace WebCore {
class KJSBridge : public JSBridge {
 public:
  KJSBridge(Frame* frame) : m_proxy(new KJSProxy(frame)) { }
  virtual ~KJSBridge() { delete m_proxy; }

  virtual void disconnectFrame();
  virtual bool wasRunByUserGesture();
  // Evaluate a script file in the environment of this proxy.
  virtual String evaluate(const String& filename, int baseLine,
                          const String& code, Node*, bool* succ);

  virtual JSResult evaluate(const String& filename, int baseLine,
                            const String& code, Node*);
  virtual void disposeJSResult(JSResult) { }

  virtual EventListener* createHTMLEventHandler(const String& functionName,
                                        const String& code, Node* node);
#if ENABLE(SVG)
  virtual EventListener* createSVGEventHandler(const String& functionName,
                                        const String& code, Node* node);
#endif

  virtual void setEventHandlerLineno(int lineno);

  virtual void finishedWithEvent(Event* evt);

  virtual void clear();

  // virtual JSRootObject *getRootObject();

  // Creates a property of the global object of a frame.
  virtual void BindToWindowObject(Frame* frame, const String& key, NPObject* object);
  virtual NPRuntimeFunctions *functions();
  virtual NPObject *CreateScriptObject(Frame*);
  virtual NPObject *CreateScriptObject(Frame*, HTMLPlugInElement*);
  virtual NPObject *CreateNoScriptObject();

  virtual bool haveInterpreter() const { return m_proxy->haveGlobalObject(); }
  virtual bool isEnabled() const { return m_proxy->isEnabled(); }

  virtual void clearDocumentWrapper() { m_proxy->clearDocumentWrapper(); }

  virtual void CollectGarbage() { }

  KJSProxy* proxy() { return m_proxy; }

 private: 
  // Internal call to create an NPObject for a JSValue
   NPObject *CreateScriptObject(Frame*, KJS::JSValue*);

 private:
  KJSProxy* m_proxy;
};

}  // namespace WebCore

#endif
