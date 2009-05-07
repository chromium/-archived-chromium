// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/debugger_remote_service.h"

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_protocol_handler.h"
#include "chrome/browser/debugger/devtools_remote_message.h"
#include "chrome/browser/debugger/inspectable_tab_proxy.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/render_messages.h"

const std::string DebuggerRemoteServiceCommand::kAttach = "attach";
const std::string DebuggerRemoteServiceCommand::kDetach = "detach";
const std::string DebuggerRemoteServiceCommand::kDebuggerCommand =
    "debugger_command";
const std::string DebuggerRemoteServiceCommand::kEvaluateJavascript =
    "evaluate_javascript";

const std::string DebuggerRemoteService::kToolName = "V8Debugger";
const std::wstring DebuggerRemoteService::kDataWide = L"data";
const std::wstring DebuggerRemoteService::kResultWide = L"result";

DebuggerRemoteService::DebuggerRemoteService(DevToolsProtocolHandler* delegate)
    : delegate_(delegate) {}

DebuggerRemoteService::~DebuggerRemoteService() {}

// message from remote debugger
void DebuggerRemoteService::HandleMessage(
    const DevToolsRemoteMessage& message) {
  static const std::wstring kCommandWide = L"command";
  const std::string destination = message.destination();
  scoped_ptr<Value> request(JSONReader::Read(message.content(), true));
  if (request.get() == NULL) {
    // Bad JSON
    NOTREACHED();
    return;
  }
  DictionaryValue* content;
  if (!request->IsType(Value::TYPE_DICTIONARY)) {
    NOTREACHED();  // Broken protocol :(
    return;
  }
  content = static_cast<DictionaryValue*>(request.get());
  if (!content->HasKey(kCommandWide)) {
    NOTREACHED();  // Broken protocol :(
    return;
  }
  std::string command;
  DictionaryValue response;

  content->GetString(kCommandWide, &command);
  response.SetString(kCommandWide, command);
  bool send_response = true;
  if (destination.size() == 0) {
    // Unknown command (bad format?)
    NOTREACHED();
    response.SetInteger(kResultWide, Result::kUnknownCommand);
    SendResponse(response, message.tool(), message.destination());
    return;
  }
  int32 tab_uid = -1;
  StringToInt(destination, &tab_uid);

  if (command == DebuggerRemoteServiceCommand::kAttach) {
    // TODO(apavlov): handle 0 for a new tab
    response.SetString(kCommandWide, DebuggerRemoteServiceCommand::kAttach);
    AttachTab(destination, &response);
  } else if (command == DebuggerRemoteServiceCommand::kDetach) {
    response.SetString(kCommandWide, DebuggerRemoteServiceCommand::kDetach);
    DetachTab(destination, &response);
  } else if (command == DebuggerRemoteServiceCommand::kDebuggerCommand) {
    if (tab_uid != -1) {
      DevToolsManager* manager = g_browser_process->devtools_manager();
      if (manager == NULL) {
        response.SetInteger(kResultWide, Result::kDebuggerError);
      }
      TabContents* tab_contents = ToTabContents(tab_uid);
      if (tab_contents != NULL) {
        DevToolsClientHost* client_host =
            manager->GetDevToolsClientHostFor(tab_contents->render_view_host());
        if (client_host != NULL) {
          std::string v8_command;
          DictionaryValue* v8_command_value;
          content->GetDictionary(kDataWide, &v8_command_value);
          JSONWriter::Write(v8_command_value, false, &v8_command);
          g_browser_process->devtools_manager()->ForwardToDevToolsAgent(
              client_host, DevToolsAgentMsg_DebuggerCommand(v8_command));
          send_response = false;
          // Do not send response right now as the JSON will be received from
          // the V8 debugger asynchronously
        } else {
          // tab_uid is not being debugged (Attach has not been invoked)
          response.SetInteger(kResultWide, Result::kIllegalTabState);
        }
      } else {
        // Unknown tab_uid from remote debugger
        response.SetInteger(kResultWide, Result::kUnknownTab);
      }
    } else {
      // Invalid tab_uid from remote debugger (perhaps NaN)
      response.SetInteger(kResultWide, Result::kUnknownTab);
    }
  } else if (command == DebuggerRemoteServiceCommand::kEvaluateJavascript) {
    if (tab_uid != -1) {
      TabContents* tab_contents = ToTabContents(tab_uid);
      if (tab_contents != NULL) {
        RenderViewHost* rvh = tab_contents->render_view_host();
        if (rvh != NULL) {
          std::wstring javascript;
          content->GetString(kDataWide, &javascript);
          rvh->Send(new ViewMsg_ScriptEvalRequest(
              rvh->routing_id(), L"", javascript));
          send_response = false;
        } else {
          // No RenderViewHost
          response.SetInteger(kResultWide, Result::kDebuggerError);
        }
      } else {
        // Unknown tab_uid from remote debugger
        response.SetInteger(kResultWide, Result::kUnknownTab);
      }
    }
  } else {
    // Unknown command
    NOTREACHED();
    response.SetInteger(kResultWide, Result::kUnknownCommand);
  }

  if (send_response) {
    SendResponse(response, message.tool(), message.destination());
  }
}

void DebuggerRemoteService::OnConnectionLost() {
  delegate_->inspectable_tab_proxy()->OnRemoteDebuggerDetached();
}

void DebuggerRemoteService::SendResponse(const Value& response,
                                         const std::string& tool,
                                         const std::string& destination) {
  std::string response_content;
  JSONWriter::Write(&response, false, &response_content);
  scoped_ptr<DevToolsRemoteMessage> response_message(
      DevToolsRemoteMessageBuilder::instance().Create(tool,
                                                      destination,
                                                      response_content));
  delegate_->Send(*response_message.get());
}

TabContents* DebuggerRemoteService::ToTabContents(int32 tab_uid) {
  const InspectableTabProxy::ControllersMap& navcon_map =
      delegate_->inspectable_tab_proxy()->controllers_map();
  InspectableTabProxy::ControllersMap::const_iterator it =
      navcon_map.find(tab_uid);
  if (it != navcon_map.end()) {
    TabContents* tab_contents = it->second->tab_contents();
    if (tab_contents == NULL) {
      return NULL;
    } else {
      return tab_contents;
    }
  } else {
    return NULL;
  }
}

void DebuggerRemoteService::DebuggerOutput(int32 tab_id,
                                           const std::string& message) {
  std::string content;
  content.append("{\"command\":\"")
      .append(DebuggerRemoteServiceCommand::kDebuggerCommand)
      .append("\",\"result\":")
      .append(IntToString(Result::kOk))
      .append(",\"data\":")
      .append(message)
      .append("}");
  scoped_ptr<DevToolsRemoteMessage> response_message(
      DevToolsRemoteMessageBuilder::instance().Create(
          kToolName,
          IntToString(tab_id),
          content));
  delegate_->Send(*(response_message.get()));
}

void DebuggerRemoteService::AttachTab(const std::string& destination,
                                      DictionaryValue* response) {
  int32 tab_uid = -1;
  StringToInt(destination, &tab_uid);
  if (tab_uid < 0) {
    // Bad tab_uid received from remote debugger (perhaps NaN)
    response->SetInteger(kResultWide, Result::kUnknownTab);
    return;
  }
  if (tab_uid == 0) {  // single tab_uid
    // We've been asked to open a new tab with URL.
    // TODO(apavlov): implement
    NOTIMPLEMENTED();
    response->SetInteger(kResultWide, Result::kUnknownTab);
    return;
  }
  TabContents* tab_contents = ToTabContents(tab_uid);
  if (tab_contents == NULL) {
    // No active web contents with tab_uid
    response->SetInteger(kResultWide, Result::kUnknownTab);
    return;
  }
  RenderViewHost* target_host = tab_contents->render_view_host();
  if (g_browser_process->devtools_manager()->GetDevToolsClientHostFor(
      target_host) == NULL) {
    DevToolsClientHost* client_host =
        delegate_->inspectable_tab_proxy()->NewClientHost(tab_uid, this);
    DevToolsManager* manager = g_browser_process->devtools_manager();
    if (manager != NULL) {
      manager->RegisterDevToolsClientHostFor(target_host, client_host);
      manager->ForwardToDevToolsAgent(client_host, DevToolsAgentMsg_Attach());
      response->SetInteger(kResultWide, Result::kOk);
    } else {
      response->SetInteger(kResultWide, Result::kDebuggerError);
    }
  } else {
    // DevToolsClientHost for this tab already registered
    response->SetInteger(kResultWide, Result::kIllegalTabState);
  }
}

void DebuggerRemoteService::DetachTab(const std::string& destination,
                                      DictionaryValue* response) {
  int32 tab_uid = -1;
  int resultCode = -1;
  StringToInt(destination, &tab_uid);
  if (tab_uid == -1) {
    // Bad tab_uid received from remote debugger (NaN)
    if (response != NULL) {
      response->SetInteger(kResultWide, Result::kUnknownTab);
    }
    return;
  }
  TabContents* tab_contents = ToTabContents(tab_uid);
  if (tab_contents == NULL) {
    // Unknown tab
    resultCode = Result::kUnknownTab;
  } else {
    DevToolsManager* manager = g_browser_process->devtools_manager();
    if (manager != NULL) {
      DevToolsClientHost* client_host =
          manager->GetDevToolsClientHostFor(tab_contents->render_view_host());
      if (client_host != NULL) {
        manager->ForwardToDevToolsAgent(
            client_host, DevToolsAgentMsg_Detach());
        client_host->InspectedTabClosing();
        resultCode = Result::kOk;
      } else {
        // No client host registered
        resultCode = Result::kUnknownTab;
      }
    } else {
      // No DevToolsManager
      resultCode = Result::kDebuggerError;
    }
  }
  if (response != NULL) {
    response->SetInteger(kResultWide, resultCode);
  }
}
