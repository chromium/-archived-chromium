// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/dom_ui_bindings.h"

#include "base/json_writer.h"
#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/stl_util-inl.h"

DOMBoundBrowserObject::~DOMBoundBrowserObject() {
  STLDeleteContainerPointers(properties_.begin(), properties_.end());
}

DOMUIBindings::DOMUIBindings() {
  BindMethod("send", &DOMUIBindings::send);
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
  sender()->Send(
      new ViewHostMsg_DOMUISend(routing_id(), message, content));
}

void DOMBoundBrowserObject::SetProperty(const std::string& name,
                                const std::string& value) {
  CppVariant* cpp_value = new CppVariant;
  cpp_value->Set(value);
  BindProperty(name, cpp_value);
  properties_.push_back(cpp_value);
}

