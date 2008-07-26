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

#include "chrome/renderer/automation/dom_automation_controller.h"

#include "chrome/common/json_value_serializer.h"
#include "chrome/common/render_messages.h"
#include "base/string_util.h"

IPC::Message::Sender* DomAutomationController::sender_(NULL);
int DomAutomationController::routing_id_(MSG_ROUTING_NONE);
int DomAutomationController::automation_id_(MSG_ROUTING_NONE);

DomAutomationController::DomAutomationController(){
  BindMethod("send", &DomAutomationController::send);
  BindMethod("setAutomationId", &DomAutomationController::setAutomationId);
}

void DomAutomationController::send(const CppArgumentList& args,
                                   CppVariant* result) {
  if (args.size() != 1) {
    result->SetNull();
    return;
  }

  if (automation_id_ == MSG_ROUTING_NONE) {
    result->SetNull();
    return;
  }

  std::string json;
  JSONStringValueSerializer serializer(&json);
  Value* value = NULL;

  // Warning: note that JSON officially requires the root-level object to be
  // an object (e.g. {foo:3}) or an array, while here we're serializing
  // strings, bools, etc. to "JSON".  This only works because (a) the JSON
  // writer is lenient, and (b) on the receiving side we wrap the JSON string
  // in square brackets, converting it to an array, then parsing it and
  // grabbing the 0th element to get the value out.
  switch(args[0].type) {
    case NPVariantType_String: {
      value = Value::CreateStringValue(UTF8ToWide(args[0].ToString()));
      break;
    }
    case NPVariantType_Bool: {
      value = Value::CreateBooleanValue(args[0].ToBoolean());
      break;
    }
    case NPVariantType_Int32: {
      value = Value::CreateIntegerValue(args[0].ToInt32());
      break;
    }
    case NPVariantType_Double: {
      // The value that is sent back is an integer while it is treated
      // as a double in this binding. The reason being that KJS treats
      // any number value as a double. Refer for more details,
      // chrome/third_party/webkit/src/JavaScriptCore/bindings/c/c_utility.cpp
      value = Value::CreateIntegerValue(args[0].ToInt32());
      break;
    }
    default: {
      result->SetNull();
      return;
    }
  }

  bool succeeded = serializer.Serialize(*value);
  if (!succeeded) {
    result->SetNull();
    return;
  }

  succeeded = sender_->Send(
    new ViewHostMsg_DomOperationResponse(routing_id_, json, automation_id_));

  automation_id_ = MSG_ROUTING_NONE;

  result->Set(succeeded);
  return;
}

void DomAutomationController::setAutomationId(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() != 1) {
    result->SetNull();
    return;
  }

  // The check here is for NumberType and not Int32 as
  // KJS::JSType only defines a NumberType (no Int32)
  if (!args[0].isNumber()) {
    result->SetNull();
    return;
  }

  automation_id_ = args[0].ToInt32();
  result->Set(true);
}
