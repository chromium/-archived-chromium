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
