// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements AutomationExtensionFunction.

#include "chrome/browser/automation/automation_extension_function.h"

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "chrome/browser/automation/extension_automation_constants.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/renderer_host/render_view_host.h"

bool AutomationExtensionFunction::enabled_ = false;

void AutomationExtensionFunction::SetName(const std::string& name) {
  name_ = name;
}

void AutomationExtensionFunction::SetArgs(const std::string& args) {
  args_ = args;
}

const std::string AutomationExtensionFunction::GetResult() {
  // Our API result passing is done through InterceptMessageFromExternalHost
  return "";
}

const std::string AutomationExtensionFunction::GetError() {
  // Our API result passing is done through InterceptMessageFromExternalHost
  return "";
}

void AutomationExtensionFunction::Run() {
  namespace keys = extension_automation_constants;

  // We are being driven through automation, so we send the extension API
  // request over to the automation host. We do this before decoding the
  // 'args' JSON, otherwise we'd be decoding it only to encode it again.
  DictionaryValue message_to_host;
  message_to_host.SetString(keys::kAutomationNameKey, name_);
  message_to_host.SetString(keys::kAutomationArgsKey, args_);
  message_to_host.SetInteger(keys::kAutomationRequestIdKey, request_id_);
  message_to_host.SetBoolean(keys::kAutomationHasCallbackKey, has_callback_);

  std::string message;
  JSONWriter::Write(&message_to_host, false, &message);
  dispatcher()->render_view_host_->delegate()->ProcessExternalHostMessage(
      message, keys::kAutomationOrigin, keys::kAutomationRequestTarget);
}

ExtensionFunction* AutomationExtensionFunction::Factory() {
  return new AutomationExtensionFunction();
}

void AutomationExtensionFunction::SetEnabled(bool enabled) {
  if (enabled) {
    std::vector<std::string> function_names;
    ExtensionFunctionDispatcher::GetAllFunctionNames(&function_names);

    for (std::vector<std::string>::iterator it = function_names.begin();
         it != function_names.end(); it++) {
      // TODO(joi) Could make this a per-profile change rather than a global
      // change. Could e.g. have the AutomationExtensionFunction store the
      // profile pointer and dispatch to the original ExtensionFunction when the
      // current profile is not that.
      bool result = ExtensionFunctionDispatcher::OverrideFunction(
          *it, AutomationExtensionFunction::Factory);
      DCHECK(result);
    }
  } else {
    ExtensionFunctionDispatcher::ResetFunctions();
  }
}

bool AutomationExtensionFunction::InterceptMessageFromExternalHost(
    RenderViewHost* view_host,
    const std::string& message,
    const std::string& origin,
    const std::string& target) {
  namespace keys = extension_automation_constants;

  if (origin == keys::kAutomationOrigin &&
      target == keys::kAutomationResponseTarget) {
    // This is an extension API response being sent back via postMessage,
    // so redirect it.
    scoped_ptr<Value> message_value(JSONReader::Read(message, false));
    DCHECK(message_value->IsType(Value::TYPE_DICTIONARY));
    if (message_value->IsType(Value::TYPE_DICTIONARY)) {
      DictionaryValue* message_dict =
          reinterpret_cast<DictionaryValue*>(message_value.get());

      int request_id = -1;
      bool got_value = message_dict->GetInteger(keys::kAutomationRequestIdKey,
                                                &request_id);
      DCHECK(got_value);
      if (got_value) {
        std::string error;
        bool success = !message_dict->GetString(keys::kAutomationErrorKey,
                                                &error);

        std::string response;
        got_value = message_dict->GetString(keys::kAutomationResponseKey,
                                            &response);
        DCHECK(!success || got_value);

        // TODO(joi) Once ExtensionFunctionDispatcher supports asynchronous
        // functions, we should use that instead.
        view_host->SendExtensionResponse(request_id, success,
                                         response, error);
        return true;
      }
    }
  }

  return false;
}
