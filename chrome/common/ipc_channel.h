// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_CHANNEL_H__
#define CHROME_COMMON_IPC_CHANNEL_H__

#include <queue>

#include "base/message_loop.h"
#include "chrome/common/ipc_message.h"

namespace IPC {

//------------------------------------------------------------------------------

class Channel : public MessageLoopForIO::Watcher,
                public Message::Sender {
  // Security tests need access to the pipe handle.
  friend class ChannelTest;

 public:
  // Implemented by consumers of a Channel to receive messages.
  class Listener {
   public:
    // Called when a message is received.
    virtual void OnMessageReceived(const Message& message) = 0;

    // Called when the channel is connected and we have received the internal
    // Hello message from the peer.
    virtual void OnChannelConnected(int32 peer_pid) {}

    // Called when an error is detected that causes the channel to close.
    // This method is not called when a channel is closed normally.
    virtual void OnChannelError() {}
  };

  enum Mode {
    MODE_SERVER,
    MODE_CLIENT
  };

  // The maximum message size in bytes. Attempting to receive a
  // message of this size or bigger results in a channel error.
  enum {
    kMaximumMessageSize = 256 * 1024 * 1024
  };

  // Initialize a Channel.
  //
  // @param channel_id
  //   Identifies the communication Channel.
  // @param mode
  //   Specifies whether this Channel is to operate in server mode or client
  //   mode.  In server mode, the Channel is responsible for setting up the
  //   IPC object, whereas in client mode, the Channel merely connects
  //   to the already established IPC object.
  // @param listener
  //   Receives a callback on the current thread for each newly received
  //   message.
  //
  Channel(const std::wstring& channel_id, Mode mode, Listener* listener);

  ~Channel() { Close(); }

  // Connect the pipe.  On the server side, this will initiate
  // waiting for connections.  On the client, it attempts to
  // connect to a pre-existing pipe.  Note, calling Connect()
  // will not block the calling thread and may complete
  // asynchronously.
  bool Connect();

  // Close this Channel explicitly.  May be called multiple times.
  void Close();

  // Modify the Channel's listener.
  void set_listener(Listener* listener) { listener_ = listener; }

  // Send a message over the Channel to the listener on the other end.
  //
  // @param message
  //   The Message to send, which must be allocated using operator new.  This
  //   object will be deleted once the contents of the Message have been sent.
  //
  //  FIXME bug 551500: the channel does not notice failures, so if the
  //    renderer crashes, it will silently succeed, leaking the parameter.
  //    At least the leak will be fixed by...
  //
  virtual bool Send(Message* message);

  // Process any pending incoming and outgoing messages.  Wait for at most
  // max_wait_msec for pending messages if there are none.  Returns true if
  // there were no pending messages or if pending messages were successfully
  // processed.  Returns false if there are pending messages that cannot be
  // processed for some reason (e.g., because ProcessIncomingMessages would be
  // re-entered).
  // TODO(darin): Need a better way of dealing with the recursion problem.
  bool ProcessPendingMessages(DWORD max_wait_msec);

 private:
  const std::wstring PipeName(const std::wstring& channel_id) const;
  bool CreatePipe(const std::wstring& channel_id, Mode mode);
  bool ProcessConnection();
  bool ProcessIncomingMessages();
  bool ProcessOutgoingMessages();

  // MessageLoop::Watcher implementation
  virtual void OnObjectSignaled(HANDLE object);

 private:
  enum {
    BUF_SIZE = 4096
  };

  struct State {
    State();
    ~State();
    OVERLAPPED overlapped;
    bool is_pending;
  };

  State input_state_;
  State output_state_;

  HANDLE pipe_;
  Listener* listener_;

  // Messages to be sent are queued here.
  std::queue<Message*> output_queue_;

  // We read from the pipe into this buffer
  char input_buf_[BUF_SIZE];

  // Large messages that span multiple pipe buffers, get built-up using
  // this buffer.
  std::string input_overflow_buf_;

  // In server-mode, we have to wait for the client to connect before we
  // can begin reading.  We make use of the input_state_ when performing
  // the connect operation in overlapped mode.
  bool waiting_connect_;

  // This flag is set when processing incoming messages.  It is used to
  // avoid recursing through ProcessIncomingMessages, which could cause
  // problems.  TODO(darin): make this unnecessary
  bool processing_incoming_;

  // The Hello message is internal to the Channel class.  It is sent
  // by the peer when the channel is connected.  The message contains
  // just the process id (pid).  The message has a special routing_id
  // (MSG_ROUTING_NONE) and type (HELLO_MESSAGE_TYPE).
  enum {
    HELLO_MESSAGE_TYPE = MAXWORD  // Maximum value of message type (WORD),
                                  // to avoid conflicting with normal
                                  // message types, which are enumeration
                                  // constants starting from 0.
  };
};

}

#endif  // CHROME_COMMON_IPC_CHANNEL_H__

