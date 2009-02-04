// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_logging.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ipc_sync_message.h"
#include "chrome/common/ipc_message_utils.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/plugin_messages.h"

#ifdef IPC_MESSAGE_LOG_ENABLED

using base::Time;

// IPC::Logging is allocated as a singleton, so we don't need any kind of
// special retention program.
template <>
struct RunnableMethodTraits<IPC::Logging> {
  static void RetainCallee(IPC::Logging*) {}
  static void ReleaseCallee(IPC::Logging*) {}
};

namespace IPC {

const wchar_t kLoggingEventName[] = L"ChromeIPCLog.%d";
const int kLogSendDelayMs = 100;

Logging::Logging()
    : logging_event_on_(NULL),
      logging_event_off_(NULL),
      enabled_(false),
      sender_(NULL),
      consumer_(NULL),
      queue_invoke_later_pending_(false),
      main_thread_(MessageLoop::current()) {
  memset(log_function_mapping_, sizeof(log_function_mapping_), 0);

  // Create an event for this browser instance that's set when logging is
  // enabled, so child processes can know when logging is enabled.
  int browser_pid;

  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  std::wstring process_type =
    parsed_command_line.GetSwitchValue(switches::kProcessType);
  if (process_type.empty()) {
    browser_pid = GetCurrentProcessId();
  } else {
    std::wstring channel_name =
        parsed_command_line.GetSwitchValue(switches::kProcessChannelID);

    browser_pid = _wtoi(channel_name.c_str());
    DCHECK(browser_pid != 0);
  }

  std::wstring event_name = GetEventName(browser_pid, true);
  logging_event_on_ = CreateEvent(NULL, TRUE, FALSE, event_name.c_str());

  event_name = GetEventName(browser_pid, false);
  logging_event_off_ = CreateEvent(NULL, TRUE, FALSE, event_name.c_str());

  RegisterWaitForEvent(true);
}

Logging::~Logging() {
  watcher_.StopWatching();
  CloseHandle(logging_event_on_);
  CloseHandle(logging_event_off_);
}

Logging* Logging::current() {
  return Singleton<Logging>::get();
}

void Logging::RegisterWaitForEvent(bool enabled) {
  watcher_.StopWatching();
  watcher_.StartWatching(
      enabled ? logging_event_on_ : logging_event_off_, this);
}

void Logging::OnObjectSignaled(HANDLE object) {
  enabled_ = object == logging_event_on_;
  RegisterWaitForEvent(!enabled_);
}

void Logging::RegisterMessageLogger(int msg_start, LogFunction* func) {
  int msg_class = msg_start >> 12;
  if (msg_class > arraysize(log_function_mapping_)) {
    NOTREACHED();
    return;
  }

  log_function_mapping_[msg_class] = func;
}

std::wstring Logging::GetEventName(bool enabled) {
  return current()->GetEventName(GetCurrentProcessId(), enabled);
}

std::wstring Logging::GetEventName(int browser_pid, bool enabled) {
  std::wstring result = StringPrintf(kLoggingEventName, browser_pid);
  result += enabled ? L"on" : L"off";
  return result;
}

void Logging::SetConsumer(Consumer* consumer) {
  consumer_ = consumer;
}

void Logging::Enable() {
  ResetEvent(logging_event_off_);
  SetEvent(logging_event_on_);
}

void Logging::Disable() {
  ResetEvent(logging_event_on_);
  SetEvent(logging_event_off_);
}

inline bool Logging::Enabled() const {
  return enabled_;
}

void Logging::OnSendLogs() {
  queue_invoke_later_pending_ = false;
  if (!sender_)
    return;

  Message* msg = new Message(
      MSG_ROUTING_CONTROL, IPC_LOGGING_ID, Message::PRIORITY_NORMAL);
  WriteParam(msg, queued_logs_);
  queued_logs_.clear();
  sender_->Send(msg);
}

void Logging::SetIPCSender(IPC::Message::Sender* sender) {
  sender_ = sender;
}

void Logging::OnReceivedLoggingMessage(const Message& message) {
  std::vector<LogData> data;
  void* iter = NULL;
  if (!ReadParam(&message, &iter, &data))
    return;

  for (size_t i = 0; i < data.size(); ++i) {
    Log(data[i]);
  }
}

void Logging::OnSendMessage(Message* message, const std::wstring& channel_id) {
  if (!Enabled())
    return;

  if (message->is_reply()) {
    LogData* data = message->sync_log_data();
    if (!data)
      return;

    // This is actually the delayed reply to a sync message.  Create a string
    // of the output parameters, add it to the LogData that was earlier stashed
    // with the reply, and log the result.
    data->channel = channel_id;
    GenerateLogData(L"", *message, data);
    Log(*data);
    delete data;
    message->set_sync_log_data(NULL);
  } else {
    // If the time has already been set (i.e. by ChannelProxy), keep that time
    // instead as it's more accurate.
    if (!message->sent_time())
      message->set_sent_time(Time::Now().ToInternalValue());
  }
}

void Logging::OnPreDispatchMessage(const Message& message) {
  message.set_received_time(Time::Now().ToInternalValue());
}

void Logging::OnPostDispatchMessage(const Message& message,
                                    const std::wstring& channel_id) {
  if (!Enabled() || !message.sent_time() || message.dont_log())
    return;

  LogData data;
  GenerateLogData(channel_id, message, &data);

  if (MessageLoop::current() == main_thread_) {
    Log(data);
  } else {
    main_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &Logging::Log, data));
  }
}

void Logging::GetMessageText(uint16 type, std::wstring* name,
                             const Message* message,
                             std::wstring* params) {
  int message_class = type >> 12;
  if (current()->log_function_mapping_[message_class] != NULL) {
    current()->log_function_mapping_[message_class](type, name, message, params);
  } else {
    DLOG(INFO) << "No logger function associated with message class " <<
        message_class;
  }
}

void Logging::Log(const LogData& data) {
  if (consumer_) {
    // We're in the browser process.
    consumer_->Log(data);
  } else {
    // We're in the renderer or plugin processes.
    if (sender_) {
      queued_logs_.push_back(data);
      if (!queue_invoke_later_pending_) {
        queue_invoke_later_pending_ = true;
        MessageLoop::current()->PostDelayedTask(FROM_HERE, NewRunnableMethod(
            this, &Logging::OnSendLogs), kLogSendDelayMs);
      }
    }
  }
}

void GenerateLogData(const std::wstring& channel, const Message& message,
                     LogData* data) {
  if (message.is_reply()) {
    // "data" should already be filled in.
    std::wstring params;
    Logging::GetMessageText(data->type, NULL, &message, &params);

    if (!data->params.empty() && !params.empty())
      data->params += L", ";

    data->flags += L" DR";

    data->params += params;
  } else {
    std::wstring flags;
    if (message.is_sync())
      flags = L"S";

    if (message.is_reply())
      flags += L"R";

    if (message.is_reply_error())
      flags += L"E";

    std::wstring params;
    Logging::GetMessageText(message.type(), NULL, &message, &params);

    data->channel = channel;
    data->type = message.type();
    data->flags = flags;
    data->sent = message.sent_time();
    data->receive = message.received_time();
    data->dispatch = Time::Now().ToInternalValue();
    data->params = params;
  }
}

}

#endif  // IPC_MESSAGE_LOG_ENABLED
