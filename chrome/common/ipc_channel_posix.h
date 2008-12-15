// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_CHANNEL_POSIX_H_
#define CHROME_COMMON_IPC_CHANNEL_POSIX_H_

#include "chrome/common/ipc_channel.h"

#include <queue>
#include <string>

#include "base/message_loop.h"

namespace IPC {

class Channel::ChannelImpl : public MessageLoopForIO::Watcher {
 public:
  // Mirror methods of Channel, see ipc_channel.h for description.
  ChannelImpl(const std::wstring& channel_id, Mode mode, Listener* listener);
  ~ChannelImpl() { Close(); }
  bool Connect();
  void Close();
  void set_listener(Listener* listener) { listener_ = listener; }
  bool Send(Message* message);
 private:
  const std::wstring PipeName(const std::wstring& channel_id) const;
  bool CreatePipe(const std::wstring& channel_id, Mode mode);

  bool ProcessIncomingMessages();
  bool ProcessOutgoingMessages();

  void OnFileCanReadWithoutBlocking(int fd);
  void OnFileCanWriteWithoutBlocking(int fd);

  Mode mode_;

  // After accepting one client connection on our server socket we want to
  // stop listening.
  MessageLoopForIO::FileDescriptorWatcher server_listen_connection_watcher_;
  MessageLoopForIO::FileDescriptorWatcher read_watcher_;
  MessageLoopForIO::FileDescriptorWatcher write_watcher_;

  // Indicates whether we're currently blocked waiting for a write to complete.
  bool is_blocked_on_write_;

  // If sending a message blocks then we use this variable
  // to keep track of where we are.
  size_t message_send_bytes_written_;

  int server_listen_pipe_;
  int pipe_;
  std::string pipe_name_;

  Listener* listener_;

  // Messages to be sent are queued here.
  std::queue<Message*> output_queue_;

  // We read from the pipe into this buffer
  char input_buf_[Channel::kReadBufferSize];

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

  ScopedRunnableMethodFactory<ChannelImpl> factory_;

  DISALLOW_COPY_AND_ASSIGN(ChannelImpl);
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_POSIX_H_
