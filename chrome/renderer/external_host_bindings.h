// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTERNAL_HOST_BINDINGS_H_
#define CHROME_RENDERER_EXTERNAL_HOST_BINDINGS_H_

#include "chrome/common/ipc_message.h"
#include "chrome/renderer/dom_ui_bindings.h"

// ExternalHostBindings is the class backing the "externalHost" object
// accessible from Javascript
//
// We expose one function, for sending a message to the external host:
//  postMessage(String message[, String target]);
class ExternalHostBindings : public DOMBoundBrowserObject {
 public:
  ExternalHostBindings();
  virtual ~ExternalHostBindings() {
  }

  // The postMessage() function provided to Javascript.
  void postMessage(const CppArgumentList& args, CppVariant* result);

  // Invokes the registered onmessage handler.
  // Returns true on successful invocation.
  bool ForwardMessageFromExternalHost(const std::string& message,
                                      const std::string& origin,
                                      const std::string& target);

  // Overridden to hold onto a pointer back to the web frame.
  void BindToJavascript(WebFrame* frame, const std::wstring& classname) {
    frame_ = frame;
    DOMBoundBrowserObject::BindToJavascript(frame, classname);
  }

 protected:
  // Creates an uninitialized instance of a MessageEvent object.
  // This is equivalent to calling window.document.createEvent("MessageEvent")
  // in javascript.
  bool CreateMessageEvent(NPObject** message_event);

 private:
  CppVariant on_message_handler_;
  WebFrame* frame_;

  DISALLOW_COPY_AND_ASSIGN(ExternalHostBindings);
};

#endif  // CHROME_RENDERER_EXTERNAL_HOST_BINDINGS_H_
