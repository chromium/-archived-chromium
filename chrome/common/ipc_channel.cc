// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_channel.h"

#include <windows.h>
#include <sstream>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/win_util.h"
#include "chrome/common/chrome_counters.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/ipc_message_utils.h"

using namespace std;

namespace IPC {

//------------------------------------------------------------------------------

Channel::State::State()
    : is_pending(false) {
  memset(&overlapped, 0, sizeof(overlapped));
  overlapped.hEvent = CreateEvent(NULL,  // default security attributes
                                  TRUE,  // manual-reset event
                                  TRUE,  // initial state = signaled
                                  NULL); // unnamed event object
}

Channel::State::~State() {
  if (overlapped.hEvent)
    CloseHandle(overlapped.hEvent);
}

//------------------------------------------------------------------------------

Channel::Channel(const wstring& channel_id, Mode mode, Listener* listener)
    : pipe_(INVALID_HANDLE_VALUE),
      listener_(listener),
      waiting_connect_(mode == MODE_SERVER),
      processing_incoming_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
  if (!CreatePipe(channel_id, mode)) {
    // The pipe may have been closed already.
    LOG(WARNING) << "Unable to create pipe named \"" << channel_id <<
                    "\" in " << (mode == 0 ? "server" : "client") << " mode.";
  }
}

void Channel::Close() {
  // make sure we are no longer watching the pipe events
  MessageLoopForIO* loop = MessageLoopForIO::current();
  loop->WatchObject(input_state_.overlapped.hEvent, NULL);
  loop->WatchObject(output_state_.overlapped.hEvent, NULL);

  if (pipe_ != INVALID_HANDLE_VALUE) {
    CloseHandle(pipe_);
    pipe_ = INVALID_HANDLE_VALUE;
  }

  while (!output_queue_.empty()) {
    Message* m = output_queue_.front();
    output_queue_.pop();
    delete m;
  }
}

bool Channel::Send(Message* message) {
  chrome::Counters::ipc_send_counter().Increment();
#ifdef IPC_MESSAGE_DEBUG_EXTRA
  DLOG(INFO) << "sending message @" << message << " on channel @" << this
             << " with type " << message->type()
             << " (" << output_queue_.size() << " in queue)";
#endif

#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging::current()->OnSendMessage(message, L"");
#endif

  output_queue_.push(message);
  // ensure waiting to write
  if (!waiting_connect_) {
    if (!output_state_.is_pending) {
      if (!ProcessOutgoingMessages(NULL, 0))
        return false;
    }
  }

  return true;
}

const wstring Channel::PipeName(const wstring& channel_id) const {
  wostringstream ss;
  // XXX(darin): get application name from somewhere else
  ss << L"\\\\.\\pipe\\chrome." << channel_id;
  return ss.str();
}

bool Channel::CreatePipe(const wstring& channel_id, Mode mode) {
  DCHECK(pipe_ == INVALID_HANDLE_VALUE);
  const wstring pipe_name = PipeName(channel_id);
  if (mode == MODE_SERVER) {
    SECURITY_ATTRIBUTES security_attributes = {0};
    security_attributes.bInheritHandle = FALSE;
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    if (!win_util::GetLogonSessionOnlyDACL(
        reinterpret_cast<SECURITY_DESCRIPTOR**>(
            &security_attributes.lpSecurityDescriptor))) {
      NOTREACHED();
    }

    pipe_ = CreateNamedPipeW(pipe_name.c_str(),
                             PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                                FILE_FLAG_FIRST_PIPE_INSTANCE,
                             PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                             1,         // number of pipe instances
                             BUF_SIZE,  // output buffer size (XXX tune)
                             BUF_SIZE,  // input buffer size (XXX tune)
                             5000,      // timeout in milliseconds (XXX tune)
                             &security_attributes);
    LocalFree(security_attributes.lpSecurityDescriptor);
  } else {
    pipe_ = CreateFileW(pipe_name.c_str(),
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION |
                            FILE_FLAG_OVERLAPPED,
                        NULL);
  }
  if (pipe_ == INVALID_HANDLE_VALUE) {
    // If this process is being closed, the pipe may be gone already.
    LOG(WARNING) << "failed to create pipe: " << GetLastError();
    return false;
  }

  // Create the Hello message to be sent when Connect is called
  scoped_ptr<Message> m(new Message(MSG_ROUTING_NONE,
                                    HELLO_MESSAGE_TYPE,
                                    IPC::Message::PRIORITY_NORMAL));
  if (!m->WriteInt(GetCurrentProcessId())) {
    CloseHandle(pipe_);
    pipe_ = INVALID_HANDLE_VALUE;
    return false;
  }

  output_queue_.push(m.release());
  return true;
}

bool Channel::Connect() {
  DLOG(WARNING) << "Connect called twice";

  if (pipe_ == INVALID_HANDLE_VALUE)
    return false;

  MessageLoopForIO::current()->RegisterIOHandler(pipe_, this);

  // Check to see if there is a client connected to our pipe...
  if (waiting_connect_)
    ProcessConnection();

  if (!input_state_.is_pending) {
    // Complete setup asynchronously. By not setting input_state_.is_pending
    // to true, we indicate to OnIOCompleted that this is the special
    // initialization signal.
    MessageLoopForIO::current()->PostTask(FROM_HERE, factory_.NewRunnableMethod(
        &Channel::OnIOCompleted, &input_state_.overlapped, 0, 0));
  }

  if (!waiting_connect_)
    ProcessOutgoingMessages(NULL, 0);
  return true;
}

bool Channel::ProcessConnection() {
  if (input_state_.is_pending) {
    input_state_.is_pending = false;
    MessageLoopForIO::current()->RegisterIOContext(&input_state_.overlapped,
                                                   NULL);
  }

  // Do we have a client connected to our pipe?
  DCHECK(pipe_ != INVALID_HANDLE_VALUE);
  BOOL ok = ConnectNamedPipe(pipe_, &input_state_.overlapped);

  DWORD err = GetLastError();
  if (ok) {
    // Uhm, the API documentation says that this function should never
    // return success when used in overlapped mode.
    NOTREACHED();
    return false;
  }

  switch (err) {
  case ERROR_IO_PENDING:
    input_state_.is_pending = true;
    MessageLoopForIO::current()->RegisterIOContext(&input_state_.overlapped,
                                                   this);
    break;
  case ERROR_PIPE_CONNECTED:
    waiting_connect_ = false;
    break;
  default:
    NOTREACHED();
    return false;
  }

  return true;
}

bool Channel::ProcessIncomingMessages(OVERLAPPED* context,
                                      DWORD bytes_read) {
  if (input_state_.is_pending) {
    input_state_.is_pending = false;
    DCHECK(context);
    MessageLoopForIO::current()->RegisterIOContext(&input_state_.overlapped,
                                                   NULL);

    if (!context || !bytes_read)
      return false;
  } else {
    // This happens at channel initialization.
    DCHECK(!bytes_read && context == &input_state_.overlapped);
  }

  for (;;) {
    if (bytes_read == 0) {
      // Read from pipe...
      BOOL ok = ReadFile(pipe_,
                         input_buf_,
                         BUF_SIZE,
                         &bytes_read,
                         &input_state_.overlapped);
      if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING) {
          MessageLoopForIO::current()->RegisterIOContext(
              &input_state_.overlapped, this);
          input_state_.is_pending = true;
          return true;
        }
        LOG(ERROR) << "pipe error: " << err;
        return false;
      }
    }
    DCHECK(bytes_read);

    // Process messages from input buffer.

    const char* p, *end;
    if (input_overflow_buf_.empty()) {
      p = input_buf_;
      end = p + bytes_read;
    } else {
      if (input_overflow_buf_.size() > (kMaximumMessageSize - bytes_read)) {
        input_overflow_buf_.clear();
        LOG(ERROR) << "IPC message is too big";
        return false;
      }
      input_overflow_buf_.append(input_buf_, bytes_read);
      p = input_overflow_buf_.data();
      end = p + input_overflow_buf_.size();
    }

    while (p < end) {
      const char* message_tail = Message::FindNext(p, end);
      if (message_tail) {
        int len = static_cast<int>(message_tail - p);
        const Message m(p, len);
#ifdef IPC_MESSAGE_DEBUG_EXTRA
        DLOG(INFO) << "received message on channel @" << this <<
                      " with type " << m.type();
#endif
        if (m.routing_id() == MSG_ROUTING_NONE &&
            m.type() == HELLO_MESSAGE_TYPE) {
          // The Hello message contains only the process id.
          listener_->OnChannelConnected(MessageIterator(m).NextInt());
        } else {
          listener_->OnMessageReceived(m);
        }
        p = message_tail;
      } else {
        // Last message is partial.
        break;
      }
    }
    input_overflow_buf_.assign(p, end - p);

    bytes_read = 0;  // Get more data.
  }

  return true;
}

bool Channel::ProcessOutgoingMessages(OVERLAPPED* context,
                                      DWORD bytes_written) {
  DCHECK(!waiting_connect_);  // Why are we trying to send messages if there's
                              // no connection?

  if (output_state_.is_pending) {
    DCHECK(context);
    MessageLoopForIO::current()->RegisterIOContext(&output_state_.overlapped,
                                                   NULL);
    output_state_.is_pending = false;
    if (!context || bytes_written == 0) {
      DWORD err = GetLastError();
      LOG(ERROR) << "pipe error: " << err;
      return false;
    }
    // Message was sent.
    DCHECK(!output_queue_.empty());
    Message* m = output_queue_.front();
    output_queue_.pop();
    delete m;
  }

  while (!output_queue_.empty()) {
    // Write to pipe...
    Message* m = output_queue_.front();
    BOOL ok = WriteFile(pipe_,
                        m->data(),
                        m->size(),
                        &bytes_written,
                        &output_state_.overlapped);
    if (!ok) {
      DWORD err = GetLastError();
      if (err == ERROR_IO_PENDING) {
        MessageLoopForIO::current()->RegisterIOContext(
            &output_state_.overlapped, this);
        output_state_.is_pending = true;

#ifdef IPC_MESSAGE_DEBUG_EXTRA
        DLOG(INFO) << "sent pending message @" << m << " on channel @" <<
                      this << " with type " << m->type();
#endif

        return true;
      }
      LOG(ERROR) << "pipe error: " << err;
      return false;
    }
    DCHECK(bytes_written == m->size());
    output_queue_.pop();

#ifdef IPC_MESSAGE_DEBUG_EXTRA
    DLOG(INFO) << "sent message @" << m << " on channel @" << this <<
                  " with type " << m->type();
#endif

    delete m;
  }

  return true;
}

bool Channel::ProcessPendingMessages(DWORD max_wait_msec) {
  return false;
  // TODO(darin): this code is broken and leads to busy waiting
#if 0
  DCHECK(max_wait_msec <= 0x7FFFFFFF || max_wait_msec == INFINITE);

  HANDLE events[] = {
    input_state_.overlapped.hEvent,
    output_state_.overlapped.hEvent
  };
  // Only deal with output messages if we have a connection on which to send
  const int wait_count = waiting_connect_ ? 1 : 2;
  DCHECK(wait_count <= _countof(events));

  if (max_wait_msec) {
    DWORD result = WaitForMultipleObjects(wait_count, events, FALSE,
                                          max_wait_msec);
    if (result == WAIT_TIMEOUT)
      return true;
  }

  bool rv = true;
  for (int i = 0; i < wait_count; ++i) {
    if (WaitForSingleObject(events[i], 0) == WAIT_OBJECT_0) {
      if (i == 0 && processing_incoming_) {
        rv = false;
        DLOG(WARNING) << "Would recurse into ProcessIncomingMessages";
      } else {
        OnObjectSignaled(events[i]);
      }
    }
  }
  return rv;
#endif
}

void Channel::OnIOCompleted(OVERLAPPED* context, DWORD bytes_transfered,
                            DWORD error) {
  bool ok;
  if (context == &input_state_.overlapped) {
    if (waiting_connect_) {
      ProcessConnection();
      // We may have some messages queued up to send...
      if (!output_queue_.empty() && !output_state_.is_pending)
        ProcessOutgoingMessages(NULL, 0);
      if (input_state_.is_pending)
        return;
      // else, fall-through and look for incoming messages...
    }
    // we don't support recursion through OnMessageReceived yet!
    DCHECK(!processing_incoming_);
    processing_incoming_ = true;
    ok = ProcessIncomingMessages(context, bytes_transfered);
    processing_incoming_ = false;
  } else {
    DCHECK(context == &output_state_.overlapped);
    ok = ProcessOutgoingMessages(context, bytes_transfered);
  }
  if (!ok) {
    Close();
    listener_->OnChannelError();
  }
}

}
