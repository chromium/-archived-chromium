// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/extension_port_container.h"

#include "base/logging.h"
#include "base/json_reader.h"
#include "base/json_writer.h"
#include "chrome/common/render_messages.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"

// TODO(siggi): Find a more structured way to read and write JSON messages.
// TODO(siggi): move these constants to
//    chrome/test/automation/extension_automation_constants.cc
const char ExtensionPortContainer::kAutomationOrigin[] = "__priv_xtapi";
const char ExtensionPortContainer::kAutomationRequestTarget[] =
  "__priv_prtreq";
const char ExtensionPortContainer::kAutomationResponseTarget[] =
  "__priv_prtres";

const wchar_t ExtensionPortContainer::kAutomationRequestIdKey[] = L"rqid";
const wchar_t ExtensionPortContainer::kAutomationConnectionIdKey[] = L"connid";
const wchar_t ExtensionPortContainer::kAutomationExtensionIdKey[] = L"extid";
const wchar_t ExtensionPortContainer::kAutomationPortIdKey[] = L"portid";
const wchar_t ExtensionPortContainer::kAutomationMessageDataKey[] = L"data";


ExtensionPortContainer::ExtensionPortContainer(AutomationProvider* automation,
                                               int tab_handle) :
    automation_(automation), service_(NULL), port_id_(-1),
    tab_handle_(tab_handle) {
  URLRequestContext* context = automation_->profile()->GetRequestContext();
  service_ = ExtensionMessageService::GetInstance(context);
  DCHECK(service_);
}

ExtensionPortContainer::~ExtensionPortContainer() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  if (port_id_ != -1)
    service_->CloseAutomationChannel(port_id_);
}

bool ExtensionPortContainer::PostResponseToExternalPort(
    const std::string& message) {
  return automation_->Send(
      new AutomationMsg_ForwardMessageToExternalHost(
          0, tab_handle_, message, kAutomationOrigin,
          kAutomationResponseTarget));
}

bool ExtensionPortContainer::PostMessageToExternalPort(
    const std::string& message) {
  return automation_->Send(
      new AutomationMsg_ForwardMessageToExternalHost(0,
                                                     tab_handle_,
                                                     message,
                                                     kAutomationOrigin,
                                                     kAutomationRequestTarget));
}

void ExtensionPortContainer::PostMessageFromExternalPort(
    const std::string &message) {
  service_->PostMessageFromRenderer(port_id_, message);
}

bool ExtensionPortContainer::Connect(const std::string &extension_id,
                                     int process_id,
                                     int routing_id,
                                     int connection_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  port_id_ = service_->OpenAutomationChannelToExtension(process_id,
                                                        routing_id,
                                                        extension_id,
                                                        this);

  SendConnectionResponse(connection_id, port_id_);
  return port_id_ != -1;
}

void ExtensionPortContainer::SendConnectionResponse(int connection_id,
                                                    int port_id) {
  // Compose the reply message.
  scoped_ptr<DictionaryValue> msg_dict(new DictionaryValue());
  msg_dict->SetInteger(kAutomationRequestIdKey, CHANNEL_OPENED);
  msg_dict->SetInteger(kAutomationConnectionIdKey, connection_id);
  msg_dict->SetInteger(kAutomationPortIdKey, port_id);

  std::string msg_json;
  JSONWriter::Write(msg_dict.get(), false, &msg_json);

  PostResponseToExternalPort(msg_json);
}

bool ExtensionPortContainer::Send(IPC::Message *message) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  IPC_BEGIN_MESSAGE_MAP(ExtensionPortContainer, *message)
    IPC_MESSAGE_HANDLER(ViewMsg_ExtensionHandleMessage,
                        OnExtensionHandleMessage)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

  delete message;
  return true;
}

void ExtensionPortContainer::OnExtensionHandleMessage(
    const std::string& message, int source_port_id) {
  // Compose the reply message and fire it away.
  scoped_ptr<DictionaryValue> msg_dict(new DictionaryValue());
  msg_dict->SetInteger(kAutomationRequestIdKey, POST_MESSAGE);
  msg_dict->SetInteger(kAutomationPortIdKey, port_id_);
  msg_dict->SetString(kAutomationMessageDataKey, message);

  std::string msg_json;
  JSONWriter::Write(msg_dict.get(), false, &msg_json);

  PostMessageToExternalPort(msg_json);
}

bool ExtensionPortContainer::InterceptMessageFromExternalHost(
    const std::string& message, const std::string& origin,
    const std::string& target, AutomationProvider* automation,
    RenderViewHost *view_host, int tab_handle) {
  if (target != kAutomationRequestTarget)
    return false;

  if (origin != kAutomationOrigin) {
    // TODO(siggi): Should we block the message on wrong origin?
    LOG(WARNING) << "Wrong origin on automation port message " << origin;
  }

  scoped_ptr<Value> message_value(JSONReader::Read(message, false));
  DCHECK(message_value->IsType(Value::TYPE_DICTIONARY));
  if (!message_value->IsType(Value::TYPE_DICTIONARY))
    return true;

  DictionaryValue* message_dict =
      reinterpret_cast<DictionaryValue*>(message_value.get());

  int command = -1;
  bool got_value = message_dict->GetInteger(kAutomationRequestIdKey, &command);
  DCHECK(got_value);
  if (!got_value)
    return true;

  if (command == OPEN_CHANNEL) {
    // Extract the "extension_id" and "connection_id" parameters.
    std::string extension_id;
    got_value = message_dict->GetString(kAutomationExtensionIdKey,
                                        &extension_id);
    DCHECK(got_value);
    if (!got_value)
      return true;

    int connection_id;
    got_value = message_dict->GetInteger(kAutomationConnectionIdKey,
                                         &connection_id);
    DCHECK(got_value);
    if (!got_value)
      return true;

    int routing_id = view_host->routing_id();
    // Create the extension port and connect it.
    scoped_ptr<ExtensionPortContainer> port(
        new ExtensionPortContainer(automation, tab_handle));

    int process_id = view_host->process()->pid();
    if (port->Connect(extension_id, process_id, routing_id, connection_id)) {
      // We have a successful connection.
      automation->AddPortContainer(port.release());
    }
  } else if (command == POST_MESSAGE) {
    int port_id = -1;
    got_value = message_dict->GetInteger(kAutomationPortIdKey, &port_id);
    DCHECK(got_value);
    if (!got_value)
      return true;

    std::string data;
    got_value = message_dict->GetString(kAutomationMessageDataKey, &data);
    DCHECK(got_value);
    if (!got_value)
      return true;

    ExtensionPortContainer* port = automation->GetPortContainer(port_id);
    DCHECK(port);
    if (port)
      port->PostMessageFromExternalPort(data);
  } else {
    // We don't expect other messages here.
    NOTREACHED();
  }

  return true;
}
