// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "base/waitable_event.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"
#include "chrome/common/render_messages.h"

//-----------------------------------------------------------------------------
// AudioRendererHost::IPCAudioSource implementations.

AudioRendererHost::IPCAudioSource::IPCAudioSource(
    AudioRendererHost* host,
    int32 render_view_id,
    int32 stream_id,
    AudioOutputStream* stream,
    size_t packet_size)
    : host_(host),
      render_view_id_(render_view_id),
      stream_id_(stream_id),
      stream_(stream),
      closed_(false),
      packet_read_event_(false, false),
      last_packet_size_(0) {
  // Make sure we can create and map the shared memory.
  DCHECK(shared_memory_.Create(L"", false, false, packet_size) &&
         shared_memory_.Map(packet_size));
}

AudioRendererHost::IPCAudioSource::~IPCAudioSource() {
}

size_t AudioRendererHost::IPCAudioSource::OnMoreData(AudioOutputStream* stream,
                                                     void* dest,
                                                     size_t max_size) {
  host_->Send(new ViewMsg_RequestAudioPacket(render_view_id_, stream_id_));
  packet_read_event_.Wait();
  if (closed_)
    return 0;
  // Make sure it's safe to copy.
  if (last_packet_size_ > max_size){
    host_->SendErrorMessage(render_view_id_, stream_id_, 0);
    host_->DestroySource(this);
    return 0;
  }
  memcpy(dest, shared_memory_.memory(), last_packet_size_);
  return last_packet_size_;
}

void AudioRendererHost::IPCAudioSource::OnClose(AudioOutputStream* stream) {
  closed_ = true;
  packet_read_event_.Signal();
}

void AudioRendererHost::IPCAudioSource::OnError(AudioOutputStream* stream,
                                                int code) {
  host_->SendErrorMessage(render_view_id_, stream_id_, code);
  // The following method call would cause this object to be destroyed on IO
  // thread.
  host_->DestroySource(this);
}

void AudioRendererHost::IPCAudioSource::NotifyPacketReady(size_t packet_size) {
  // If reported size is greater than capacity of the shared memory, close the
  // stream.
  if (packet_size > shared_memory_.max_size()) {
    host_->SendErrorMessage(render_view_id_, stream_id_, 0);
    // We don't need to do packet_read_event_.Signal() here because the
    // contained stream should be closed by the following call and OnClose will
    // be received.
    host_->DestroySource(this);
  } else {
    last_packet_size_ = packet_size;
    packet_read_event_.Signal();
  }
}

//-----------------------------------------------------------------------------
// AudioRendererHost implementations.

AudioRendererHost::AudioRendererHost(MessageLoop* message_loop)
    : io_loop_(message_loop) {
  // Make sure we perform actual initialization operations in the thread where
  // this object should live.
  io_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioRendererHost::OnInitialized));
}

AudioRendererHost::~AudioRendererHost() {
}

void AudioRendererHost::Destroy() {
  // Post a message to the thread where this object should live and do the
  // actual operations there.
  io_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &AudioRendererHost::OnDestroyed));
}

void AudioRendererHost::CreateStream(
    base::ProcessHandle process_handle, int32 render_view_id, int32 stream_id,
    AudioManager::Format format, int channels, int sample_rate,
    int bits_per_sample, size_t packet_size) {
  DCHECK(MessageLoop::current() == io_loop_);
  DCHECK(Lookup(render_view_id, stream_id) == NULL);

  // Create the stream in the first place.
  AudioOutputStream* stream = AudioManager::GetAudioManager()->MakeAudioStream(
      format, channels, sample_rate, bits_per_sample);
  if (!stream)
    return;
  if (!stream->Open(packet_size)) {
    stream->Close();
    stream = NULL;
    return;
  }

  IPCAudioSource* source = new IPCAudioSource(
      this, render_view_id, stream_id, stream, packet_size);
  // If we can open the stream, proceed with sharing the shared memory.
  base::SharedMemoryHandle foreign_memory_handle;
  if (source->shared_memory()->ShareToProcess(process_handle,
                                              &foreign_memory_handle)) {
    sources_.insert(make_pair(SourceID(render_view_id, stream_id), source));
    Send(new ViewMsg_NotifyAudioStreamCreated(
        render_view_id, stream_id, foreign_memory_handle, packet_size));
  } else {
    DestroySource(source);
    SendErrorMessage(render_view_id, stream_id, 0);
  }
}

void AudioRendererHost::Start(int32 render_view_id, int32 stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = Lookup(render_view_id, stream_id);
  if (source) {
    source->stream()->Start(source);
    Send(new ViewMsg_NotifyAudioStreamStateChanged(
        render_view_id, stream_id, AudioOutputStream::STATE_STARTED, 0));
  } else {
    SendErrorMessage(render_view_id, stream_id, 0);
  }
}

void AudioRendererHost::Close(int32 render_view_id, int32 stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = Lookup(render_view_id, stream_id);
  // When we get called here, audio renderer in renderer process has been
  // destroyed, we don't bother to an error message back, just destroy if we
  // can.
  if (source) {
    DestroySource(source);
  }
}

void AudioRendererHost::SetVolume(int32 render_view_id, int32 stream_id,
    double left_channel, double right_channel) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = Lookup(render_view_id, stream_id);
  if (source) {
    source->stream()->SetVolume(left_channel, right_channel);
  } else {
    SendErrorMessage(render_view_id, stream_id, 0);
  }
}

void AudioRendererHost::GetVolume(int32 render_view_id, int32 stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = Lookup(render_view_id, stream_id);
  if (source) {
    double left_channel, right_channel;
    source->stream()->GetVolume(&left_channel, &right_channel);
    Send(new ViewMsg_NotifyAudioStreamVolume(
        render_view_id, stream_id, left_channel, right_channel));
  } else {
    SendErrorMessage(render_view_id, stream_id, 0);
  }
}

void AudioRendererHost::NotifyPacketReady(int32 render_view_id,
                                          int32 stream_id, size_t packet_size) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = Lookup(render_view_id, stream_id);
  if (source) {
    source->NotifyPacketReady(packet_size);
  } else {
    SendErrorMessage(render_view_id, stream_id, 0);
  }
}

// Event received when IPC channel is connected to the renderer process.
void AudioRendererHost::IPCChannelConnected(IPC::Message::Sender* ipc_sender) {
  DCHECK(MessageLoop::current() == io_loop_);
  ipc_sender_ = ipc_sender;
}

// Event received when IPC channel is closing.
void AudioRendererHost::IPCChannelClosing() {
  DCHECK(MessageLoop::current() == io_loop_);
  ipc_sender_ = NULL;
  DestroyAllSources();
}

void AudioRendererHost::OnInitialized() {
  DCHECK(MessageLoop::current() == io_loop_);
  // Increase the ref count of this object so it is active until we do
  // Release().
  AddRef();
  // Also create the AudioManager singleton in this thread.
  // TODO(hclam): figure out a better location to initialize the AudioManager
  // singleton. The following method call seems to create a memory leak.
  // AudioManager::GetAudioManager();
}

void AudioRendererHost::OnDestroyed() {
  DCHECK(MessageLoop::current() == io_loop_);
  ipc_sender_ = NULL;
  DestroyAllSources();
  // Decrease the reference to this object, which may lead to self-destruction.
  Release();
}

void AudioRendererHost::OnSend(IPC::Message* message) {
  DCHECK(MessageLoop::current() == io_loop_);
  if (ipc_sender_) {
    ipc_sender_->Send(message);
  }
}

void AudioRendererHost::OnDestroySource(IPCAudioSource* source) {
  DCHECK(MessageLoop::current() == io_loop_);
  if (source) {
    sources_.erase(SourceID(source->render_view_id(), source->stream_id()));
    source->stream()->Stop();
    source->stream()->Close();
    delete source;
  }
}

void AudioRendererHost::DestroyAllSources() {
  DCHECK(MessageLoop::current() == io_loop_);
  std::vector<IPCAudioSource*> sources;
  for (SourceMap::iterator i = sources_.begin(); i != sources_.end(); ++i) {
    sources.push_back(i->second);
  }
  for (size_t i = 0; i < sources.size(); ++i) {
    DestroySource(sources[i]);
  }
  DCHECK(sources_.empty());
}

AudioRendererHost::IPCAudioSource* AudioRendererHost::Lookup(int render_view_id,
                                                             int stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  SourceMap::iterator i = sources_.find(SourceID(render_view_id, stream_id));
  if (i != sources_.end())
    return i->second;
  return NULL;
}

void AudioRendererHost::Send(IPC::Message* message) {
  if (MessageLoop::current() == io_loop_) {
    OnSend(message);
  } else {
    // TODO(hclam): make sure it's always safe to post a task to IO loop.
    // It is possible that IO message loop is destroyed but there's still some
    // dangling audio hardware threads that try to call this method.
    io_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &AudioRendererHost::OnSend, message));
  }
}

void AudioRendererHost::SendErrorMessage(int32 render_view_id,
                                         int32 stream_id, int info) {
  Send(new ViewMsg_NotifyAudioStreamStateChanged(
      render_view_id, stream_id, AudioOutputStream::STATE_ERROR, info));
}

void AudioRendererHost::DestroySource(IPCAudioSource* source) {
  if (MessageLoop::current() == io_loop_) {
    OnDestroySource(source);
  } else {
    // TODO(hclam): make sure it's always safe to post a task to IO loop.
    // It is possible that IO message loop is destroyed but there's still some
    // dangling audio hardware threads that try to call this method.
    io_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioRendererHost::OnDestroySource, source));
  }
}
