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

#include "base/message_loop.h"
#include "base/thread.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/ipc_message_utils.h"

namespace IPC {

//-----------------------------------------------------------------------------

ChannelProxy::Context::Context(Channel::Listener* listener,
                               MessageFilter* filter,
                               MessageLoop* ipc_message_loop)
    : listener_message_loop_(MessageLoop::current()),
      listener_(listener),
      ipc_message_loop_(ipc_message_loop),
      channel_(NULL) {
  if (filter)
    filters_.push_back(filter);
}

void ChannelProxy::Context::CreateChannel(const std::wstring& id,
                                          const Channel::Mode& mode) {
  DCHECK(channel_ == NULL);
  channel_id_ = id;
  channel_ = new Channel(id, mode, this);
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnMessageReceived(const Message& message) {
#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging* logger = Logging::current();
  if (logger->Enabled())
    logger->OnPreDispatchMessage(message);
#endif

  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i]->OnMessageReceived(message)) {
#ifdef IPC_MESSAGE_LOG_ENABLED
      if (logger->Enabled())
        logger->OnPostDispatchMessage(message, channel_id_);
#endif
      return;
    }
  }

  // NOTE: This code relies on the listener's message loop not going away while
  // this thread is active.  That should be a reasonable assumption, but it
  // feels risky.  We may want to invent some more indirect way of referring to
  // a MessageLoop if this becomes a problem.

  listener_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &Context::OnDispatchMessage, message));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelConnected(int32 peer_pid) {
  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelConnected(peer_pid);

  // See above comment about using listener_message_loop_ here.
  listener_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &Context::OnDispatchConnected, peer_pid));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelError() {
  // See above comment about using listener_message_loop_ here.
  listener_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &Context::OnDispatchError));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnOpenChannel() {
  DCHECK(channel_ != NULL);

  // Assume a reference to ourselves on behalf of this thread.  This reference
  // will be released when we are closed.
  AddRef();

  if (!channel_->Connect()) {
    OnChannelError();
    return;
  }

  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnFilterAdded(channel_);
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnCloseChannel() {
  // It's okay for IPC::ChannelProxy::Close to be called more than once, which
  // would result in this branch being taken.
  if (!channel_)
    return;

  for (size_t i = 0; i < filters_.size(); ++i) {
    filters_[i]->OnChannelClosing();
    filters_[i]->OnFilterRemoved();
  }

  // We don't need the filters anymore.
  filters_.clear();

  delete channel_;
  channel_ = NULL;

  // Balance with the reference taken during startup.  This may result in
  // self-destruction.
  Release();
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnSendMessage(Message* message) {
  if (!channel_->Send(message))
    OnChannelError();
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnAddFilter(MessageFilter* filter) {
  filters_.push_back(filter);

  // If the channel has already been created, then we need to send this message
  // so that the filter gets access to the Channel.
  if (channel_)
    filter->OnFilterAdded(channel_);

  // Balances the AddRef in ChannelProxy::AddFilter.
  filter->Release();
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnRemoveFilter(MessageFilter* filter) {
  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i].get() == filter) {
      filter->OnFilterRemoved();
      filters_.erase(filters_.begin() + i);
      return;
    }
  }

  NOTREACHED() << "filter to be removed not found";
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchMessage(const Message& message) {
  if (!listener_)
    return;

#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging* logger = Logging::current();
  if (message.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(message);
    return;
  }

  if (logger->Enabled())
    logger->OnPreDispatchMessage(message);
#endif

  listener_->OnMessageReceived(message);

#ifdef IPC_MESSAGE_LOG_ENABLED
  if (logger->Enabled())
    logger->OnPostDispatchMessage(message, channel_id_);
#endif
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchConnected(int32 peer_pid) {
  if (listener_)
    listener_->OnChannelConnected(peer_pid);
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchError() {
  if (listener_)
    listener_->OnChannelError();
}

//-----------------------------------------------------------------------------

ChannelProxy::ChannelProxy(const std::wstring& channel_id, Channel::Mode mode,
                           Channel::Listener* listener, MessageFilter* filter,
                           MessageLoop* ipc_thread)
    : context_(new Context(listener, filter, ipc_thread)) {
  Init(channel_id, mode, ipc_thread, true);
}

ChannelProxy::ChannelProxy(const std::wstring& channel_id, Channel::Mode mode,
                           Channel::Listener* listener, MessageFilter* filter,
                           MessageLoop* ipc_thread, Context* context,
                           bool create_pipe_now)
    : context_(context) {
  Init(channel_id, mode, ipc_thread, create_pipe_now);
}

void ChannelProxy::Init(const std::wstring& channel_id, Channel::Mode mode,
                        MessageLoop* ipc_thread_loop, bool create_pipe_now) {
  if (create_pipe_now) {
    // Create the channel immediately.  This effectively sets up the
    // low-level pipe so that the client can connect.  Without creating
    // the pipe immediately, it is possible for a listener to attempt
    // to connect and get an error since the pipe doesn't exist yet.
    context_->CreateChannel(channel_id, mode);
  } else {
    context_->ipc_message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        context_.get(), &Context::CreateChannel, channel_id, mode));
  }

  // complete initialization on the background thread
  context_->ipc_message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      context_.get(), &Context::OnOpenChannel));
}

void ChannelProxy::Close() {
  // Clear the backpointer to the listener so that any pending calls to
  // Context::OnDispatchMessage or OnDispatchError will be ignored.  It is
  // possible that the channel could be closed while it is receiving messages!
  context_->clear();

  if (MessageLoop::current() == context_->ipc_message_loop()) {
    // We're being destructed on the IPC thread, so no need to use the message
    // loop as it might go away.
    context_->OnCloseChannel();
  } else {
    context_->ipc_message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        context_.get(), &Context::OnCloseChannel));
  }
}

bool ChannelProxy::Send(Message* message) {
#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging::current()->OnSendMessage(message, context_->channel_id());
#endif

  context_->ipc_message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      context_.get(), &Context::OnSendMessage, message));
  return true;
}

void ChannelProxy::AddFilter(MessageFilter* filter) {
  // We want to addref the filter to prevent it from
  // being destroyed before the OnAddFilter call is invoked.
  filter->AddRef();
  context_->ipc_message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      context_.get(), &Context::OnAddFilter, filter));
}

void ChannelProxy::RemoveFilter(MessageFilter* filter) {
  context_->ipc_message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      context_.get(), &Context::OnRemoveFilter, filter));
}

//-----------------------------------------------------------------------------

}  // namespace IPC
