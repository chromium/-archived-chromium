// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/inspectable_tab_proxy.h"

#include "base/json_reader.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/debugger/debugger_remote_service.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/devtools_messages.h"

void DevToolsClientHostImpl::InspectedTabClosing() {
  static const std::string kEmptyUrl = "";
  TabClosed();
  NotifyCloseListener();
  delete this;
}

void DevToolsClientHostImpl::SendMessageToClient(
    const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(DevToolsClientHostImpl, msg)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_RpcMessage, OnRpcMessage);
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void DevToolsClientHostImpl::OnRpcMessage(const std::string& msg) {
  static const std::string kDebuggerAgentDelegate = "DebuggerAgentDelegate";
  static const std::string kToolsAgentDelegate = "ToolsAgentDelegate";
  static const std::string kDebuggerOutput = "DebuggerOutput";
  static const std::string kFrameNavigate = "FrameNavigate";

  scoped_ptr<Value> message(JSONReader::Read(msg, false));
  if (!message->IsType(Value::TYPE_LIST)) {
    NOTREACHED();  // The RPC protocol has changed :(
    return;
  }
  ListValue* list_msg = static_cast<ListValue*>(message.get());
  std::string class_name;
  list_msg->GetString(0, &class_name);
  std::string message_name;
  list_msg->GetString(1, &message_name);
  if (class_name == kDebuggerAgentDelegate && message_name == kDebuggerOutput) {
    std::string str;
    list_msg->GetString(2, &str);
    DebuggerOutput(str);
  } else if (class_name == kToolsAgentDelegate &&
             message_name == kFrameNavigate) {
    std::string url;
    list_msg->GetString(2, &url);
    FrameNavigate(url);
  }
}

void DevToolsClientHostImpl::DebuggerOutput(const std::string& msg) {
  service_->DebuggerOutput(id_, msg);
}

void DevToolsClientHostImpl::FrameNavigate(const std::string& url) {
  service_->FrameNavigate(id_, url);
}

void DevToolsClientHostImpl::TabClosed() {
  service_->TabClosed(id_);
}

const InspectableTabProxy::ControllersMap&
    InspectableTabProxy::controllers_map() {
  controllers_map_.clear();
  for (BrowserList::const_iterator it = BrowserList::begin(),
       end = BrowserList::end(); it != end; ++it) {
    TabStripModel* model = (*it)->tabstrip_model();
    for (int i = 0, size = model->count(); i < size; ++i) {
      NavigationController& controller =
          model->GetTabContentsAt(i)->controller();
      controllers_map_[controller.session_id().id()] = &controller;
    }
  }
  return controllers_map_;
}

DevToolsClientHost* InspectableTabProxy::NewClientHost(
    int32 id,
    DebuggerRemoteService* service) {
  DevToolsClientHostImpl* client_host =
      new DevToolsClientHostImpl(id, service, &id_to_client_host_map_);
  id_to_client_host_map_[id] = client_host;
  return client_host;
}

void InspectableTabProxy::OnRemoteDebuggerDetached() {
  while (id_to_client_host_map_.size() > 0) {
    IdToClientHostMap::iterator it = id_to_client_host_map_.begin();
    it->second->debugger_remote_service()->DetachFromTab(IntToString(it->first),
                                                         NULL);
  }
}
