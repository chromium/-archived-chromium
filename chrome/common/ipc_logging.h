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

#ifndef CHROME_COMMON_IPC_LOGGING_H__
#define CHROME_COMMON_IPC_LOGGING_H__

#include <vector>
#include <windows.h>
#include "base/basictypes.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "chrome/common/ipc_message_utils.h"

#ifdef IPC_MESSAGE_LOG_ENABLED

namespace IPC {

class Message;

// One instance per process.  Needs to be created on the main thread (the UI
// thread in the browser) but OnPreDispatchMessage/OnPostDispatchMessage
// can be called on other threads.
class Logging : public base::RefCounted<Logging>,
                public MessageLoop::Watcher {
 public:
  // Implemented by consumers of log messages.
  class Consumer {
   public:
    virtual void Log(const IPC::LogData& data) = 0;
  };

  void SetConsumer(Consumer* consumer);

  ~Logging();
  static Logging* current();

  void Enable();
  void Disable();
  bool inline Enabled() const;

  // Called by child processes to give the logger object the channel to send
  // logging data to the browser process.
  void SetIPCSender(IPC::Message::Sender* sender);

  // Called in the browser process when logging data from a child process is
  // received.
  void OnReceivedLoggingMessage(const Message& message);

  void OnSendMessage(Message* message, const std::wstring& channel_id);
  void OnPreDispatchMessage(const Message& message);
  void OnPostDispatchMessage(const Message& message,
                             const std::wstring& channel_id);

  // Returns the name of the logging enabled/disabled events so that the
  // sandbox can add them to to the policy.  If true, gets the name of the
  // enabled event, if false, gets the name of the disabled event.
  static std::wstring GetEventName(bool enabled);

  // Like the *MsgLog functions declared for each message class, except this
  // calls the correct one based on the message type automatically.  Defined in
  // ipc_logging.cc.
  static void GetMessageText(uint16 type, std::wstring* name,
                             const Message* message, std::wstring* params);

  // MessageLoop::Watcher
  void OnObjectSignaled(HANDLE object);

 private:
  Logging();

  std::wstring GetEventName(int browser_pid, bool enabled);
  void OnSendLogs();
  void Log(const LogData& data);

  void RegisterWaitForEvent(bool enabled);

  HANDLE logging_event_on_;
  HANDLE logging_event_off_;
  bool enabled_;

  std::vector<LogData> queued_logs_;
  bool queue_invoke_later_pending_;

  IPC::Message::Sender* sender_;
  MessageLoop* main_thread_;

  Consumer* consumer_;

  static scoped_refptr<Logging> current_;

  static Lock logger_lock_;
};

}

#endif // IPC_MESSAGE_LOG_ENABLED

#endif  // CHROME_COMMON_IPC_LOGGING_H__
