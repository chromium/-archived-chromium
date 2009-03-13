// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(pfeldman): Remove these once JSON is available in
// WebCore namespace.

#include "PlatformString.h"
#undef LOG

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/values.h"
#include "webkit/glue/devtools/devtools_rpc.h"
#include "webkit/glue/glue_util.h"

DevToolsRpc::DevToolsRpc(Delegate* delegate) : delegate_(delegate) {
}

DevToolsRpc::~DevToolsRpc() {
}

void DevToolsRpc::SendValueMessage(const Value* value) {
  std::string json;
  JSONWriter::Write(value, false, &json);
  delegate_->SendRpcMessage(json);
}

// static
Value* DevToolsRpc::ParseMessage(const std::string& raw_msg) {
  return JSONReader::Read(raw_msg, false);
}

// static
void DevToolsRpc::GetListValue(
    const ListValue& message,
    int index,
    bool* value) {
  message.GetBoolean(index, value);
}

// static
void DevToolsRpc::GetListValue(
    const ListValue& message,
    int index,
    int* value) {
  message.GetInteger(index, value);
}

// static
void DevToolsRpc::GetListValue(
    const ListValue& message,
    int index,
    String* value) {
  std::string tmp;
  message.GetString(index, &tmp);
  *value = webkit_glue::StdStringToString(tmp);
}

// static
void DevToolsRpc::GetListValue(
    const ListValue& message,
    int index,
    Value** value) {
  message.Get(index, value);
}

// static
Value* DevToolsRpc::CreateValue(const String* value) {
  return Value::CreateStringValue(
      webkit_glue::StringToStdString(*value));
}

// static
Value* DevToolsRpc::CreateValue(int* value) {
  return Value::CreateIntegerValue(*value);
}

// static
Value* DevToolsRpc::CreateValue(bool* value) {
  return Value::CreateBooleanValue(*value);
}

// static
Value* DevToolsRpc::CreateValue(const Value* value) {
  return value->DeepCopy();
}
