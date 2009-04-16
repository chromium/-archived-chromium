// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/audio_message_filter.h"

AudioMessageFilter::AudioMessageFilter(int32 route_id)
    : channel_(NULL),
      route_id_(route_id),
      message_loop_(NULL) {
}

AudioMessageFilter::~AudioMessageFilter() {
}

// Called on the IPC thread.
bool AudioMessageFilter::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }
  message->set_routing_id(route_id_);
  return channel_->Send(message);
}

bool AudioMessageFilter::OnMessageReceived(const IPC::Message& message) {
  if (message.routing_id() != route_id_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AudioMessageFilter, message)
    IPC_MESSAGE_HANDLER(ViewMsg_RequestAudioPacket, OnRequestPacket)
    IPC_MESSAGE_HANDLER(ViewMsg_NotifyAudioStreamCreated, OnStreamCreated)
    IPC_MESSAGE_HANDLER(ViewMsg_NotifyAudioStreamStateChanged,
                        OnStreamStateChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_NotifyAudioStreamVolume, OnStreamVolume)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AudioMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  // Captures the message loop for IPC.
  message_loop_ = MessageLoop::current();
  channel_ = channel;
}

void AudioMessageFilter::OnFilterRemoved() {
  channel_ = NULL;
}

void AudioMessageFilter::OnChannelClosing() {
  channel_ = NULL;
}

void AudioMessageFilter::OnRequestPacket(int stream_id) {
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio packet request for a non-existent or removed"
        " audio renderer.";
    return;
  }
  delegate->OnRequestPacket();
}

void AudioMessageFilter::OnStreamCreated(int stream_id,
                                         base::SharedMemoryHandle handle,
                                         int length) {
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
        " audio renderer.";
    return;
  }
  delegate->OnCreated(handle, length);
}

void AudioMessageFilter::OnStreamStateChanged(int stream_id,
                                              AudioOutputStream::State state,
                                              int info) {
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
        " audio renderer.";
    return;
  }
  delegate->OnStateChanged(state, info);
}

void AudioMessageFilter::OnStreamVolume(int stream_id,
                                        double left, double right) {
  Delegate* delegate = delegates_.Lookup(stream_id);
  if (!delegate) {
    DLOG(WARNING) << "Got audio stream event for a non-existent or removed"
        " audio renderer.";
    return;
  }
  delegate->OnVolume(left, right);
}

int32 AudioMessageFilter::AddDelegate(Delegate* delegate) {
  return delegates_.Add(delegate);
}

void AudioMessageFilter::RemoveDelegate(int32 id) {
  delegates_.Remove(id);
}
