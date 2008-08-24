// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

