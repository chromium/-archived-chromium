// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_protocol_handler.h"
#include "chrome/browser/debugger/devtools_remote_message.h"
#include "chrome/browser/debugger/devtools_remote_service.h"
#include "chrome/browser/debugger/inspectable_tab_proxy.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/devtools_messages.h"

const std::string DevToolsRemoteServiceCommand::kPing = "ping";
const std::string DevToolsRemoteServiceCommand::kVersion = "version";
const std::string DevToolsRemoteServiceCommand::kListTabs = "list_tabs";

const std::wstring DevToolsRemoteService::kCommandWide = L"command";
const std::wstring DevToolsRemoteService::kDataWide = L"data";
const std::wstring DevToolsRemoteService::kResultWide = L"result";
const std::string DevToolsRemoteService::kToolName = "DevToolsService";

DevToolsRemoteService::DevToolsRemoteService(DevToolsProtocolHandler* delegate)
    : delegate_(delegate) {}

DevToolsRemoteService::~DevToolsRemoteService() {}

void DevToolsRemoteService::HandleMessage(
    const DevToolsRemoteMessage& message) {
  scoped_ptr<Value> request(JSONReader::Read(message.content(), false));
  if (request.get() == NULL) {
    // Bad JSON
    NOTREACHED();
    return;
  }
  DictionaryValue* json;
  if (request->IsType(Value::TYPE_DICTIONARY)) {
    json = static_cast<DictionaryValue*>(request.get());
    if (!json->HasKey(kCommandWide)) {
      NOTREACHED();  // Broken protocol - no "command" specified
      return;
    }
  } else {
    NOTREACHED();  // Broken protocol - not a JS object
    return;
  }
  ProcessJson(json, message);
}

void DevToolsRemoteService::ProcessJson(DictionaryValue* json,
                                        const DevToolsRemoteMessage& message) {
  static const std::string kOkResponse = "ok";  // "Ping" response
  static const std::string kVersion = "0.1";  // Current protocol version
  std::string command;
  DictionaryValue response;

  json->GetString(kCommandWide, &command);
  response.SetString(kCommandWide, command);

  if (command == DevToolsRemoteServiceCommand::kPing) {
    response.SetInteger(kResultWide, Result::kOk);
    response.SetString(kDataWide, kOkResponse);
  } else if (command == DevToolsRemoteServiceCommand::kVersion) {
    response.SetInteger(kResultWide, Result::kOk);
    response.SetString(kDataWide, kVersion);
  } else if (command == DevToolsRemoteServiceCommand::kListTabs) {
    ListValue* data = new ListValue();
    const InspectableTabProxy::ControllersMap& navcon_map =
        delegate_->inspectable_tab_proxy()->controllers_map();
    for (InspectableTabProxy::ControllersMap::const_iterator it =
        navcon_map.begin(), end = navcon_map.end(); it != end; ++it) {
      NavigationEntry* entry = it->second->GetActiveEntry();
      if (entry == NULL) {
        continue;
      }
      if (entry->url().is_valid()) {
        ListValue* tab = new ListValue();
        tab->Append(Value::CreateIntegerValue(
            static_cast<int32>(it->second->session_id().id())));
        tab->Append(Value::CreateStringValue(entry->url().spec()));
        data->Append(tab);
      }
    }
    response.SetInteger(kResultWide, Result::kOk);
    response.Set(kDataWide, data);
  } else {
    // Unknown protocol command.
    NOTREACHED();
    response.SetInteger(kResultWide, Result::kUnknownCommand);
  }
  std::string response_json;
  JSONWriter::Write(&response, false, &response_json);
  scoped_ptr<DevToolsRemoteMessage> response_message(
      DevToolsRemoteMessageBuilder::instance().Create(message.tool(),
                                                      message.destination(),
                                                      response_json));
  delegate_->Send(*response_message.get());
}
