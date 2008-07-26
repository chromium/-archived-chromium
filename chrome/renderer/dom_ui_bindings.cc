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

#include "chrome/renderer/dom_ui_bindings.h"

#include "base/json_writer.h"
#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/stl_util-inl.h"

DOMUIBindings::DOMUIBindings() : routing_id_(0) {
  BindMethod("send", &DOMUIBindings::send);
}

DOMUIBindings::~DOMUIBindings() {
  STLDeleteContainerPointers(properties_.begin(), properties_.end());
}

void DOMUIBindings::send(const CppArgumentList& args, CppVariant* result) {
  // We expect at least a string message identifier, and optionally take
  // an object parameter.  If we get anything else we bail.
  if (args.size() < 1 || args.size() > 2)
    return;

  // Require the first parameter to be the message name.
  if (!args[0].isString())
    return;
  const std::string message = args[0].ToString();

  // If they've provided an optional message parameter, convert that into JSON.
  std::string content;
  if (args.size() == 2) {
    if (!args[1].isObject())
      return;
    // TODO(evanm): we ought to support more than just sending arrays of
    // strings, but it's not yet necessary for the current code.
    std::vector<std::wstring> strings = args[1].ToStringVector();
    ListValue value;
    for (size_t i = 0; i < strings.size(); ++i) {
      value.Append(Value::CreateStringValue(strings[i]));
    }
    JSONWriter::Write(&value, /* pretty_print= */ false, &content);
  }

  // Send the message up to the browser.
  sender_->Send(
      new ViewHostMsg_DOMUISend(routing_id_, message, content));
}

void DOMUIBindings::SetProperty(const std::string& name,
                                const std::string& value) {
  CppVariant* cpp_value = new CppVariant;
  cpp_value->Set(value);
  BindProperty(name, cpp_value);
  properties_.push_back(cpp_value);
}
