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

#include "v8_events.h"
#include "v8_proxy.h"
#include "v8_binding.h"
#include "Frame.h"
#include "DOMWindow.h"
#include "V8Event.h"
#include "Event.h"
#include "Document.h"
#include "Tokenizer.h"
#include "Node.h"
#include "XMLHttpRequest.h"
#include "WorkerContextExecutionProxy.h"
#include "CString.h"

namespace WebCore {


V8AbstractEventListener::V8AbstractEventListener(Frame* frame, bool isInline)
    : m_isInline(isInline), m_frame(frame) {
  if (!m_frame) return;

  // Get the position in the source if any.
  m_lineNumber = 0;
  m_columnNumber = 0;
  if (m_isInline && m_frame->document()->tokenizer()) {
    m_lineNumber = m_frame->document()->tokenizer()->lineNumber();
    m_columnNumber = m_frame->document()->tokenizer()->columnNumber();
  }
}

void V8AbstractEventListener::handleEvent(Event* event, bool isWindowEvent) {
  // EventListener could be disconnected from the frame.
  if (disconnected())
    return;

  // The callback function on XMLHttpRequest can clear the event listener
  // and destroys 'this' object. Keep a local reference of it.
  // See issue 889829
  RefPtr<V8AbstractEventListener> self(this);

  v8::HandleScope handle_scope;

  v8::Handle<v8::Context> context = V8Proxy::GetContext(m_frame);
  if (context.IsEmpty())
    return;
  v8::Context::Scope scope(context);

  // m_frame can removed by the callback function,
  // protect it until the callback function returns.
  RefPtr<Frame> protector(m_frame);

  IF_DEVEL(log_info(frame, "Handling DOM event", m_frame->document()->URL()));

  v8::Handle<v8::Value> jsevent = V8Proxy::EventToV8Object(event);

  // For compatibility, we store the event object as a property on the window
  // called "event".  Because this is the global namespace, we save away any
  // existing "event" property, and then restore it after executing the
  // javascript handler.
  v8::Local<v8::String> event_symbol = v8::String::NewSymbol("event");

  // Save the old 'event' property.
  v8::Local<v8::Value> saved_evt = context->Global()->Get(event_symbol);

  // Make the event available in the window object.
  //
  // TODO: This does not work as in safari if the window.event
  // property is already set. We need to make sure that property
  // access is intercepted correctly.
  context->Global()->Set(event_symbol, jsevent);

  v8::Local<v8::Value> ret;
  {
    // Catch exceptions thrown in the event handler so they do not
    // propagate to javascript code that caused the event to fire.
    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    // Call the event handler.
    ret = CallListenerFunction(jsevent, event, isWindowEvent);
  }

  // Restore the old event. This must be done for all exit paths through
  // this method.
  context->Global()->Set(event_symbol, saved_evt);

  if (V8Proxy::HandleOutOfMemory())
    ASSERT(ret.IsEmpty());

  if (ret.IsEmpty()) {
    return;
  }

  if (!ret.IsEmpty()) {
    if (!ret->IsNull() && !ret->IsUndefined() &&
        event->storesResultAsString()) {
      event->storeResult(ToWebCoreString(ret));
    }
    // Prevent default action if the return value is false;
    // TODO(fqian): example, and reference to buganizer entry
    if (m_isInline) {
      if (ret->IsBoolean() && !ret->BooleanValue()) {
        event->preventDefault();
      }
    }
  }

  Document::updateDocumentsRendering();
}


void V8AbstractEventListener::DisposeListenerObject() {
  if (!m_listener.IsEmpty()) {
#ifndef NDEBUG
    V8Proxy::UnregisterGlobalHandle(this, m_listener);
#endif
    m_listener.Dispose();
    m_listener.Clear();
  }
}


v8::Local<v8::Object> V8AbstractEventListener::GetReceiverObject(
    Event* event,
    bool isWindowEvent) {
  if (!m_listener.IsEmpty() && !m_listener->IsFunction()) {
    return v8::Local<v8::Object>::New(m_listener);
  }

  if (isWindowEvent) {
    return v8::Context::GetCurrent()->Global();
  }

  EventTarget* target = event->currentTarget();
  v8::Handle<v8::Value> value = V8Proxy::EventTargetToV8Object(target);
  if (value.IsEmpty()) return v8::Local<v8::Object>();
  return v8::Local<v8::Object>::New(v8::Handle<v8::Object>::Cast(value));
}


V8EventListener::V8EventListener(Frame* frame, v8::Local<v8::Object> listener,
                                 bool isInline)
  : V8AbstractEventListener(frame, isInline) {
  m_listener = v8::Persistent<v8::Object>::New(listener);
#ifndef NDEBUG
  V8Proxy::RegisterGlobalHandle(EVENT_LISTENER, this, m_listener);
#endif
}


V8EventListener::~V8EventListener() {
  if (m_frame) {
    V8Proxy* proxy = V8Proxy::retrieve(m_frame);
    if (proxy)
      proxy->RemoveV8EventListener(this);
  }

  DisposeListenerObject();
}


v8::Local<v8::Function> V8EventListener::GetListenerFunction() {
  // It could be disposed already.
  if (m_listener.IsEmpty()) return v8::Local<v8::Function>();

  if (m_listener->IsFunction()) {
    return v8::Local<v8::Function>::New(
        v8::Persistent<v8::Function>::Cast(m_listener));

  } else if (m_listener->IsObject()) {
    v8::Local<v8::Value> prop =
      m_listener->Get(v8::String::NewSymbol("handleEvent"));
    if (prop->IsFunction()) {
      return v8::Local<v8::Function>::Cast(prop);
    }
  }

  return v8::Local<v8::Function>();
}


v8::Local<v8::Value>
V8EventListener::CallListenerFunction(v8::Handle<v8::Value> jsevent,
                                      Event* event, bool isWindowEvent) {
  v8::Local<v8::Function> handler_func = GetListenerFunction();
  v8::Local<v8::Object> receiver = GetReceiverObject(event, isWindowEvent);
  if (handler_func.IsEmpty() || receiver.IsEmpty()) {
    return v8::Local<v8::Value>();
  }

  v8::Handle<v8::Value> parameters[1] = { jsevent };

  V8Proxy* proxy = V8Proxy::retrieve(m_frame);
  return proxy->CallFunction(handler_func, receiver, 1, parameters);
}


// ------- V 8 X H R E v e n t L i s t e n e r -----------------

static void WeakObjectEventListenerCallback(v8::Persistent<v8::Value> obj,
                                            void* para) {
  V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(para);

  // Remove the wrapper
  Frame* frame = listener->frame();
  if (frame) {
    V8Proxy* proxy = V8Proxy::retrieve(frame);
    if (proxy)
      proxy->RemoveObjectEventListener(listener);

    // Because the listener is no longer in the list, it must
    // be disconnected from the frame to avoid dangling frame pointer
    // in the destructor.
    listener->disconnectFrame();
  }

  // Dispose the listener object.
  listener->DisposeListenerObject();
}


V8ObjectEventListener::V8ObjectEventListener(Frame* frame,
                                             v8::Local<v8::Object> listener,
                                             bool isInline)
    : V8EventListener(frame, listener, isInline) {
  // make m_listener weak.
  m_listener.MakeWeak(this, WeakObjectEventListenerCallback);
}


V8ObjectEventListener::~V8ObjectEventListener() {
  if (m_frame) {
    ASSERT(!m_listener.IsEmpty());
    V8Proxy* proxy = V8Proxy::retrieve(m_frame);
    if (proxy)
      proxy->RemoveObjectEventListener(this);
  }

  DisposeListenerObject();
}


// ------- L a z y E v e n t L i s t e n e r ---------------

V8LazyEventListener::V8LazyEventListener(Frame *frame, const String& code,
                                         const String& func_name)
    : V8AbstractEventListener(frame, true), m_code(code),
      m_func_name(func_name), m_compiled(false),
      m_wrapped_function_compiled(false) {
}


V8LazyEventListener::~V8LazyEventListener() {
  DisposeListenerObject();

  // Dispose wrapped function
  if (!m_wrapped_function.IsEmpty()) {
#ifndef NDEBUG
    V8Proxy::UnregisterGlobalHandle(this, m_wrapped_function);
#endif
    m_wrapped_function.Dispose();
    m_wrapped_function.Clear();
  }
}


v8::Local<v8::Function> V8LazyEventListener::GetListenerFunction() {
  if (m_compiled) {
    ASSERT(m_listener.IsEmpty() || m_listener->IsFunction());
    return m_listener.IsEmpty() ?
        v8::Local<v8::Function>() :
        v8::Local<v8::Function>::New(
            v8::Persistent<v8::Function>::Cast(m_listener));
  }

  m_compiled = true;

  ASSERT(m_frame);

  {
    // Switch to the contex of m_frame
    v8::HandleScope handle_scope;

    // Use the outer scope to hold context.
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_frame);
    // Bail out if we could not get the context.
    if (context.IsEmpty()) return v8::Local<v8::Function>();

    v8::Context::Scope scope(context);

    // Wrap function around the event code.  The parenthesis around
    // the function are needed so that evaluating the code yields
    // the function value.  Without the parenthesis the function
    // value is thrown away.

    // Make it an anonymous function to avoid name conflict for cases like
    // <body onload='onload()'>
    // <script> function onload() { alert('hi'); } </script>.
    // Set function name to function object instead.
    // See issue 944690.
    //
    // The ECMAScript spec says (very obliquely) that the parameter to
    // an event handler is named "evt".
    String code = "(function (evt) {\n";
    code.append(m_code);
    code.append("})");

    IF_DEVEL(log_info(frame, code, "<getListener>"));

    v8::Handle<v8::String> codeExternalString = v8ExternalString(code);
    v8::Handle<v8::Script> script = V8Proxy::CompileScript(codeExternalString,
        m_frame->document()->url(), m_lineNumber - 1);
    if (!script.IsEmpty()) {
      V8Proxy* proxy = V8Proxy::retrieve(m_frame);
      ASSERT(proxy);  // must be valid at this point
      v8::Local<v8::Value> value = proxy->RunScript(script, false);
      if (!value.IsEmpty()) {
        ASSERT(value->IsFunction());
        v8::Local<v8::Function> listener_func =
            v8::Local<v8::Function>::Cast(value);
        // Set the function name.
        listener_func->SetName(v8::String::New(FromWebCoreString(m_func_name),
                                               m_func_name.length()));

        m_listener = v8::Persistent<v8::Function>::New(listener_func);
#ifndef NDEBUG
        V8Proxy::RegisterGlobalHandle(EVENT_LISTENER, this, m_listener);
#endif
      }
    }
  }  // end of HandleScope

  ASSERT(m_listener.IsEmpty() || m_listener->IsFunction());
  return m_listener.IsEmpty() ?
      v8::Local<v8::Function>() :
      v8::Local<v8::Function>::New(
          v8::Persistent<v8::Function>::Cast(m_listener));
}


v8::Local<v8::Value>
V8LazyEventListener::CallListenerFunction(v8::Handle<v8::Value> jsevent,
                                          Event* event, bool isWindowEvent) {
  v8::Local<v8::Function> handler_func = GetWrappedListenerFunction();
  v8::Local<v8::Object> receiver = GetReceiverObject(event, isWindowEvent);
  if (handler_func.IsEmpty() || receiver.IsEmpty()) {
    return v8::Local<v8::Value>();
  }

  v8::Handle<v8::Value> parameters[1] = { jsevent };

  V8Proxy* proxy = V8Proxy::retrieve(m_frame);
  return proxy->CallFunction(handler_func, receiver, 1, parameters);
}


v8::Local<v8::Function> V8LazyEventListener::GetWrappedListenerFunction() {
  if (m_wrapped_function_compiled) {
    ASSERT(m_wrapped_function.IsEmpty() || m_wrapped_function->IsFunction());
    return m_wrapped_function.IsEmpty() ? v8::Local<v8::Function>() :
      v8::Local<v8::Function>::New(m_wrapped_function);
  }

  m_wrapped_function_compiled = true;

  {
    // Switch to the contex of m_frame
    v8::HandleScope handle_scope;

    // Use the outer scope to hold context.
    v8::Handle<v8::Context> context = V8Proxy::GetContext(m_frame);
    // Bail out if we cannot get the context.
    if (context.IsEmpty()) return v8::Local<v8::Function>();

    v8::Context::Scope scope(context);

    // TODO(fqian): cache the wrapper function.
    String code = "(function (evt) {\n";

    // Nodes other than the document object, when executing inline event
    // handlers push document, form, and the target node on the scope chain.
    // We do this by using 'with' statement.
    // See chrome/fast/forms/form-action.html
    //     chrome/fast/forms/selected-index-value.html
    //     base/fast/overflow/onscroll-layer-self-destruct.html
    code.append("  with (this.ownerDocument ? this.ownerDocument : {}) {\n");
    code.append("    with (this.form ? this.form : {}) {\n");
    code.append("      with (this) {\n");
    code.append("        return (function(evt){");
    code.append(m_code);
    code.append("}).call(this, evt);\n");
    code.append("      }\n");
    code.append("    }\n");
    code.append("  }\n");
    code.append("})");
    v8::Handle<v8::String> codeExternalString = v8ExternalString(code);
    v8::Handle<v8::Script> script = V8Proxy::CompileScript(codeExternalString,
        m_frame->document()->url(), m_lineNumber - 4);
    if (!script.IsEmpty()) {
      V8Proxy* proxy = V8Proxy::retrieve(m_frame);
      ASSERT(proxy);
      v8::Local<v8::Value> value = proxy->RunScript(script, false);
      if (!value.IsEmpty()) {
        ASSERT(value->IsFunction());

        m_wrapped_function = v8::Persistent<v8::Function>::New(
            v8::Local<v8::Function>::Cast(value));
#ifndef NDEBUG
        V8Proxy::RegisterGlobalHandle(EVENT_LISTENER, this, m_wrapped_function);
#endif
        // Set the function name.
        m_wrapped_function->SetName(v8::String::New(
            FromWebCoreString(m_func_name), m_func_name.length()));
      }
    }
  }  // end of local scope

  return v8::Local<v8::Function>::New(m_wrapped_function);
}

#if ENABLE(WORKERS)
V8WorkerContextEventListener::V8WorkerContextEventListener(
      WorkerContextExecutionProxy* proxy,
      v8::Local<v8::Object> listener,
      bool isInline)
    : V8ObjectEventListener(NULL, listener, isInline)
    , m_proxy(proxy) {
}

V8WorkerContextEventListener::~V8WorkerContextEventListener() {
  if (m_proxy) {
    m_proxy->RemoveEventListener(this);
  }
  DisposeListenerObject();
}

void V8WorkerContextEventListener::handleEvent(Event* event,
                                               bool isWindowEvent) {
  // EventListener could be disconnected from the frame.
  if (disconnected())
    return;

  // The callback function on XMLHttpRequest can clear the event listener
  // and destroys 'this' object. Keep a local reference of it.
  // See issue 889829
  RefPtr<V8AbstractEventListener> self(this);

  v8::HandleScope handle_scope;

  v8::Handle<v8::Context> context = m_proxy->GetContext();
  if (context.IsEmpty())
    return;
  v8::Context::Scope scope(context);

  v8::Handle<v8::Value> jsevent =
      WorkerContextExecutionProxy::EventToV8Object(event);

  // For compatibility, we store the event object as a property on the window
  // called "event".  Because this is the global namespace, we save away any
  // existing "event" property, and then restore it after executing the
  // javascript handler.
  v8::Local<v8::String> event_symbol = v8::String::NewSymbol("event");

  // Save the old 'event' property.
  v8::Local<v8::Value> saved_evt = context->Global()->Get(event_symbol);

  // Make the event available in the window object.
  //
  // TODO: This does not work as in safari if the window.event
  // property is already set. We need to make sure that property
  // access is intercepted correctly.
  context->Global()->Set(event_symbol, jsevent);

  v8::Local<v8::Value> ret;
  {
    // Catch exceptions thrown in the event handler so they do not
    // propagate to javascript code that caused the event to fire.
    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    // Call the event handler.
    ret = CallListenerFunction(jsevent, event, isWindowEvent);
  }

  // Restore the old event. This must be done for all exit paths through
  // this method.
  context->Global()->Set(event_symbol, saved_evt);

  if (V8Proxy::HandleOutOfMemory())
    ASSERT(ret.IsEmpty());

  if (ret.IsEmpty()) {
    return;
  }

  if (!ret.IsEmpty()) {
    if (!ret->IsNull() && !ret->IsUndefined() &&
        event->storesResultAsString()) {
      event->storeResult(ToWebCoreString(ret));
    }
    // Prevent default action if the return value is false;
    // TODO(fqian): example, and reference to buganizer entry
    if (m_isInline) {
      if (ret->IsBoolean() && !ret->BooleanValue()) {
        event->preventDefault();
      }
    }
  }
}

v8::Local<v8::Value> V8WorkerContextEventListener::CallListenerFunction(
    v8::Handle<v8::Value> jsevent, Event* event, bool isWindowEvent) {
  v8::Local<v8::Function> handler_func = GetListenerFunction();
  if (handler_func.IsEmpty()) return v8::Local<v8::Value>();

  v8::Local<v8::Object> receiver = GetReceiverObject(event, isWindowEvent);
  v8::Handle<v8::Value> parameters[1] = {jsevent};

  v8::Local<v8::Value> result;
  {
    //ConsoleMessageScope scope;

    result = handler_func->Call(receiver, 1, parameters);
  }

  m_proxy->TrackEvent(event);

  return result;
}

v8::Local<v8::Object> V8WorkerContextEventListener::GetReceiverObject(
    Event* event, bool isWindowEvent) {
  if (!m_listener.IsEmpty() && !m_listener->IsFunction()) {
    return v8::Local<v8::Object>::New(m_listener);
  }

  if (isWindowEvent) {
    return v8::Context::GetCurrent()->Global();
  }

  EventTarget* target = event->currentTarget();
  v8::Handle<v8::Value> value =
      WorkerContextExecutionProxy::EventTargetToV8Object(target);
  if (value.IsEmpty()) return v8::Local<v8::Object>();
  return v8::Local<v8::Object>::New(v8::Handle<v8::Object>::Cast(value));
}
#endif  // WORKERS

}  // namespace WebCore
