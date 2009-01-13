// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  V8AbstractEventListener(Frame* frame, bool isInline);


  // Call listener function.
  virtual v8::Local<v8::Value> CallListenerFunction(
      v8::Handle<v8::Value> jsevent,
      Event* event,
      bool isWindowEvent) = 0;

  // Get the receiver object to use for event listener call.
  v8::Local<v8::Object> GetReceiverObject(Event* event,
                                          bool isWindowEvent);

  // Frame to which the event listener is attached to.
  // An event listener must be destroyed before its owner
  // frame is deleted.
  // See fast/dom/replaceChild.html
  // TODO(fqian): this could hold m_frame live until
  // the event listener is deleted. Fix this!
  Frame* m_frame;

  // Listener object.
  v8::Persistent<v8::Object> m_listener;

  // Flags this is a HTML type listener.
  bool m_isInline;

  // Position in the HTML source for HTML event listeners.
  int m_lineNumber;
  int m_columnNumber;

  friend class V8EventListener;
  friend class V8ObjectEventListener;
  friend class V8LazyEventListener;
};

// V8EventListener is a wrapper of a JS object implements EventListener
// interface (has handleEvent(event) method), or a JS function
// that can handle the event.
class V8EventListener : public V8AbstractEventListener {
 public:
  static PassRefPtr<V8EventListener> create(Frame* frame, 
      v8::Local<v8::Object> listener, bool isInline) {
    return adoptRef(new V8EventListener(frame, listener, isInline));
  }

  V8EventListener(Frame* frame, v8::Local<v8::Object> listener, bool isInline);
  virtual ~V8EventListener();
  virtual bool isInline() const { return m_isInline; }

  // Detach the listener from its owner frame.
  void disconnectFrame() { m_frame = 0; }

 private:
  // Call listener function.
  virtual v8::Local<v8::Value> CallListenerFunction(
      v8::Handle<v8::Value> jsevent, Event* event, bool isWindowEvent);
  v8::Local<v8::Function> GetListenerFunction();
};


// V8ObjectEventListener is a special listener wrapper for objects not
// in the DOM.  It keeps the JS listener as a weak pointer.
class V8ObjectEventListener : public V8EventListener {
 public:
  static PassRefPtr<V8ObjectEventListener> create(Frame* frame, 
      v8::Local<v8::Object> listener, bool isInline) {
    return adoptRef(new V8ObjectEventListener(frame, listener, isInline));
  }
  V8ObjectEventListener(Frame* frame, v8::Local<v8::Object> listener,
                        bool isInline);
  virtual ~V8ObjectEventListener();
};


// V8LazyEventListener is a wrapper for a JavaScript code string that is
// compiled and evaluated when an event is fired.
// A V8LazyEventListener is always a HTML event handler.
class V8LazyEventListener : public V8AbstractEventListener {
 public:
  static PassRefPtr<V8LazyEventListener> create(Frame* frame, 
      const String& code, const String& func_name) {
    return adoptRef(new V8LazyEventListener(frame, code, func_name));
  }
  V8LazyEventListener(Frame *frame, const String& code,
                      const String& func_name);
  virtual ~V8LazyEventListener();
  virtual bool isInline() const { return true; }

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

  v8::Local<v8::Function> GetListenerFunction();
};

}  // namespace WebCore

#endif  // V8_EVENTS_H__

