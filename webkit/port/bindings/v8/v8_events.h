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

#ifndef V8_EVENTS_H__
#define V8_EVENTS_H__

#include "config.h"
#include <v8.h>
#include "Frame.h"
#include "Event.h"
#include "EventListener.h"
#include "PlatformString.h"

// #define IF_DEVEL(stmt) stmt
#define IF_DEVEL(stmt) ((void) 0)

namespace WebCore {

// There are two kinds of event listeners: HTML or non-HMTL.
// onload, onfocus, etc (attributes) are always HTML event handler type;
// Event listeners added by Window.addEventListener
// or EventTargetNode::addEventListener are non-HTML type.
//
// Why does this matter?
// WebKit does not allow duplicated HTML event handlers of the same type,
// but ALLOWs duplicated non-HTML event handlers.

class V8AbstractEventListener : public EventListener {
 public:
  virtual ~V8AbstractEventListener() { }

  // Returns the owner frame of the listener.
  Frame* frame() { return m_frame; }

  // Handle event.
  void handleEvent(Event*, bool isWindowEvent);

  // Returns the listener object, either a function or an object.
  virtual v8::Local<v8::Object> GetListenerObject() {
    return v8::Local<v8::Object>::New(m_listener);
  }

  // Dispose listener object and clear the handle
  void DisposeListenerObject();

 private:
  V8AbstractEventListener(Frame* frame, bool html);


  // Call listener function.
  virtual v8::Local<v8::Value>
    CallListenerFunction(v8::Handle<v8::Value> jsevent,
                         Event* event, bool isWindowEvent) = 0;

  // Frame to which the event listener is attached to.
  // An event listener must be destroyed before its owner
  // frame is deleted.
  // See fast/dom/replaceChild.html
  // TODO(fqian): this could hold m_frame live until
  // the event listener is deleted. Fix this!
  Frame* m_frame;

  // Listener object, avoid using peer because it can keep this object alive.
  v8::Persistent<v8::Object> m_listener;

  // Flags this is a HTML type listener.
  bool m_html;

  // Position in the HTML source for HTML event listeners.
  int m_lineNumber;
  int m_columnNumber;

  friend class V8EventListener;
  friend class V8XHREventListener;
  friend class V8LazyEventListener;
};

// V8EventListener is a wrapper of a JS object implements EventListener
// interface (has handleEvent(event) method), or a JS function
// that can handle the event.
class V8EventListener : public V8AbstractEventListener {
 public:
  V8EventListener(Frame* frame, v8::Local<v8::Object> listener, bool html);
  virtual ~V8EventListener();
  virtual bool isHTMLEventListener() const { return m_html; }

  // Detach the listener from its owner frame.
  void disconnectFrame() { m_frame = 0; }

 private:
  // Call listener function.
  virtual v8::Local<v8::Value> CallListenerFunction(
      v8::Handle<v8::Value> jsevent, Event* event, bool isWindowEvent);
  v8::Local<v8::Object> GetThisObject(Event* event, bool isWindowEvent);
  v8::Local<v8::Function> GetListenerFunction();
};


// V8XHREventListener is a special listener wrapper for XMLHttpRequest object.
// It keeps JS listener week.
class V8XHREventListener : public V8EventListener {
 public:
  V8XHREventListener(Frame* frame, v8::Local<v8::Object> listener, bool html);
  virtual ~V8XHREventListener();
};


// V8LazyEventListener is a wrapper for a JavaScript code string that is
// compiled and evaluated when an event is fired.
// A V8LazyEventListener is always a HTML event handler.
class V8LazyEventListener : public V8AbstractEventListener {
 public:
  V8LazyEventListener(Frame *frame, const String& code,
                      const String& func_name);
  virtual ~V8LazyEventListener();
  virtual bool isHTMLEventListener() const { return true; }

  // For lazy event listener, the listener object is the same as its listener
  // function without additional scope chains.
  virtual v8::Local<v8::Object> GetListenerObject() {
    return GetWrappedListenerFunction();
  }

 private:
  String m_code;
  String m_func_name;  // function name
  bool m_compiled;

  // If the event listener is on a non-document dom node,
  // we compile the function with some implicit scope chains before it.
  bool m_wrapped_function_compiled;
  v8::Persistent<v8::Function> m_wrapped_function;

  v8::Local<v8::Function> GetWrappedListenerFunction();

  virtual v8::Local<v8::Value> CallListenerFunction(
      v8::Handle<v8::Value> jsevent, Event* event, bool isWindowEvent);

  v8::Local<v8::Object> GetThisObject(Event* event, bool isWindowEvent);
  v8::Local<v8::Function> GetListenerFunction();
};

}  // namespace WebCore

#endif  // V8_EVENTS_H__
