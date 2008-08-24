// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains V8 specific bits that's necessary to build WebKit.

#ifndef V8_BRIDGE_H__
#define V8_BRIDGE_H__

#include "v8.h"
#include "JSBridge.h"

namespace WebCore {
class Frame;
class Node;
class V8Proxy;

class V8Bridge : public JSBridge {
 public:
  explicit V8Bridge(Frame* frame);
  virtual ~V8Bridge();

  virtual void disconnectFrame();
  virtual bool wasRunByUserGesture();
  // Evaluate a script file in the environment of this proxy.
  // When succeed, sets the 'succ' parameter to true and
  // returns the result as a string.
  virtual String evaluate(const String& filename, int baseLine,
                          const String& code, Node* node, bool* succ);

  // Evaluate a script in the environment of this proxy.
  // Returns a v8::Persistent handle. If the evaluation fails,
  // it returns an empty handle. The caller must check
  // whether the return value is empty.
  virtual JSResult evaluate(const String& filename, int baseLine,
                            const String& code, Node* node);
  virtual void disposeJSResult(JSResult result);

  virtual EventListener* createHTMLEventHandler(const String& functionName,
                                                const String& code, Node* node);
#if ENABLE(SVG)
  virtual EventListener* createSVGEventHandler(const String& functionName,
                                        const String& code, Node* node);
#endif
  virtual void setEventHandlerLineno(int lineno);
  virtual void finishedWithEvent(Event* evt);
  virtual void clear();

  V8Proxy* proxy() { return m_proxy; }

  // virtual JSRootObject *getRootObject();

  virtual void BindToWindowObject(Frame* frame, const String& key,
                                  NPObject* object);
  virtual NPRuntimeFunctions *functions();
  virtual NPObject *CreateScriptObject(Frame* frame);
  virtual NPObject *CreateScriptObject(Frame* frame, HTMLPlugInElement* pe);
  virtual NPObject *CreateNoScriptObject();

  virtual bool haveInterpreter() const;
  virtual bool isEnabled() const;

  virtual void clearDocumentWrapper();

  virtual void CollectGarbage();

 private:
  V8Proxy* m_proxy;
};

}  // namespace WebCore
#endif  // V8_BRIDGE_H__

