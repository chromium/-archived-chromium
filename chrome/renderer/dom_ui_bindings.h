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

#ifndef CHROME_RENDERER_DOM_UI_BINDINGS_H__
#define CHROME_RENDERER_DOM_UI_BINDINGS_H__

#include "chrome/common/ipc_message.h"
#include "webkit/glue/cpp_bound_class.h"

// DOMUIBindings is the class backing the "chrome" object accessible
// from Javascript from privileged pages.
//
// We expose one function, for sending a message to the browser:
//   send(String name, Object argument);
// It's plumbed through to the OnDOMUIMessage callback on RenderViewHost
// delegate.
class DOMUIBindings : public CppBoundClass {
 public:
  DOMUIBindings();
  ~DOMUIBindings();

  // The send() function provided to Javascript.
  void send(const CppArgumentList& args, CppVariant* result);

  // Set the message channel back to the browser.
  void set_message_sender(IPC::Message::Sender* sender) {
    sender_ = sender;
  }

  // Set the routing id for messages back to the browser.
  void set_routing_id(int routing_id) {
    routing_id_ = routing_id;
  }

  // Sets a property with the given name and value.
  void SetProperty(const std::string& name, const std::string& value);

 private:
  // Our channel back to the browser is a message sender
  // and routing id.
  IPC::Message::Sender* sender_;
  int routing_id_;

  // The list of properties that have been set.  We keep track of this so we
  // can free them on destruction.
  typedef std::vector<CppVariant*> PropertyList;
  PropertyList properties_;
};

#endif  // CHROME_RENDERER_DOM_UI_BINDINGS_H__
