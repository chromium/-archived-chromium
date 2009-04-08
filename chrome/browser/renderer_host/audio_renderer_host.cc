// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/histogram.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "base/waitable_event.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"
#include "chrome/common/render_messages.h"

namespace {

void RecordIPCAudioLatency(base::TimeDelta latency) {
  // Create a histogram of minimum 1ms and maximum 1000ms with 100 buckets.
  static ThreadSafeHistogram histogram("Audio.IPCTransportLatency",
                                       1, 1000, 100);
  histogram.AddTime(latency);
}

}  // namespace

//-----------------------------------------------------------------------------
// AudioRendererHost::IPCAudioSource implementations.

AudioRendererHost::IPCAudioSource::IPCAudioSource(
    AudioRendererHost* host,
    int process_id,
    int route_id,
    int stream_id,
    AudioOutputStream* stream,
    size_t packet_size)
    : host_(host),
      process_id_(process_id),
      route_id_(route_id),
      stream_id_(stream_id),
      stream_(stream),
      packet_size_(packet_size),
      state_(AudioOutputStream::STATE_CREATED),
      stop_providing_packets_(false),
      packet_read_event_(false, false),
      last_packet_size_(0) {
}

AudioRendererHost::IPCAudioSource::~IPCAudioSource() {
}

// static
AudioRendererHost::IPCAudioSource*
    AudioRendererHost::IPCAudioSource::CreateIPCAudioSource(
        AudioRendererHost* host,
        int process_id,
        int route_id,
        int stream_id,
        base::ProcessHandle process_handle,
        AudioManager::Format format,
        int channels,
        int sample_rate,
        char bits_per_sample,
        size_t packet_size) {
  // Create the stream in the first place.
  AudioOutputStream* stream =
      AudioManager::GetAudioManager()->MakeAudioStream(
          format, channels, sample_rate, bits_per_sample);
  if (stream && !stream->Open(packet_size)) {
    stream->Close();
    stream = NULL;
  }

  if (stream) {
    IPCAudioSource* source = new IPCAudioSource(
        host, process_id, route_id, stream_id, stream, packet_size);
    // If we can open the stream, proceed with sharing the shared memory.
    base::SharedMemoryHandle foreign_memory_handle;

    // Try to create, map and share the memory for the renderer process.
    // If they all succeeded then send a message to renderer to indicate
    // success.
    if (source->shared_memory_.Create(L"", false, false, packet_size) &&
        source->shared_memory_.Map(packet_size) &&
        source->shared_memory_.ShareToProcess(process_handle,
                                              &foreign_memory_handle)) {
      host->Send(new ViewMsg_NotifyAudioStreamCreated(
          route_id, stream_id, foreign_memory_handle, packet_size));
      return source;
    }

    source->Close();
    delete source;
  }

  host->SendErrorMessage(route_id, stream_id, 0);
  return NULL;
}

void AudioRendererHost::IPCAudioSource::Start() {
  // Only perform the start logic if this source has just created.
  if (!stream_ || state_ != AudioOutputStream::STATE_CREATED)
    return;

  // We don't start the stream immediately but prefetch some initial buffers
  // so as to fill all internal buffers of the AudioOutputStream. The number
  // of buffers to prefetch can be determined by
  // AudioOutputStream::GetNumBuffers().
  if (stream_->GetNumBuffers()) {
    // If the audio output stream does have internal buffer(s), request a
    // packet from renderer and start the prefetching.
    host_->Send(new ViewMsg_RequestAudioPacket(route_id_, stream_id_));
  } else {
    // If the audio output stream does not use any internal buffers, we are
    // safe to start it here.
    state_ = AudioOutputStream::STATE_STARTED;
    stream_->Start(this);
    host_->Send(new ViewMsg_NotifyAudioStreamStateChanged(
        route_id_, stream_id_, AudioOutputStream::STATE_STARTED, 0));
  }
}

void AudioRendererHost::IPCAudioSource::Close() {
  // We need to wake up all waiting audio thread before calling stop.
  StopWaitingForPacket();

  if (!stream_)
    return;
  stream_->Stop();
  stream_->Close();
  // After stream is closed it is destroyed, so don't keep a reference to it.
  stream_ = NULL;
}

void AudioRendererHost::IPCAudioSource::SetVolume(double left, double right) {
  // TODO(hclam): maybe send an error message back to renderer if this object
  // is in a wrong state.
  if (!stream_)
    return;
  stream_->SetVolume(left, right);
}

void AudioRendererHost::IPCAudioSource::GetVolume() {
  // TODO(hclam): maybe send an error message back to renderer if this object
  // is in a wrong state.
  if (!stream_)
    return;
  double left_channel, right_channel;
  stream_->GetVolume(&left_channel, &right_channel);
  host_->Send(new ViewMsg_NotifyAudioStreamVolume(route_id_, stream_id_,
                                                  left_channel, right_channel));
}

size_t AudioRendererHost::IPCAudioSource::OnMoreData(AudioOutputStream* stream,
                                                     void* dest,
                                                     size_t max_size) {
  base::TimeTicks tick_start = base::TimeTicks::HighResNow();
  {
    AutoLock auto_lock(lock_);
    // If we are ever stopped, don't ask for more audio packet from the
    // renderer.
    if (stop_providing_packets_)
      return 0;
  }

  // If we have an initial packet, use it immediately only in IO thread.
  // There's a case when IO thread is blocked and audio hardware thread can
  // reach here to consume initial packets.
  if (MessageLoop::current() == host_->io_loop()) {
    if (!initial_buffers_.empty()) {
      uint8* initial_packet = initial_buffers_.front().first;
      size_t initial_packet_size = initial_buffers_.front().second;
      initial_buffers_.pop_front();
      size_t copied =
          SafeCopyBuffer(dest, max_size, initial_packet, initial_packet_size);
      delete [] initial_packet;
      return copied;
    }
    NOTREACHED();
  }

  // We reach here because we ran out of initial packets, we need to ask the
  // renderer to give us more. In this case we have to wait until the renderer
  // gives us packet so we can't sleep on IO thread.
  DCHECK(MessageLoop::current() != host_->io_loop());

  // Send an IPC message to request audio packet from renderer and wait on the
  // audio hardware thread.
  host_->Send(new ViewMsg_RequestAudioPacket(route_id_, stream_id_));
  packet_read_event_.Wait();

  size_t last_packet_size = 0;
  {
     AutoLock auto_lock(lock_);
     last_packet_size = last_packet_size_;
  }

  size_t copied = SafeCopyBuffer(dest, max_size,
                                 shared_memory_.memory(), last_packet_size);
  RecordIPCAudioLatency(base::TimeTicks::HighResNow() - tick_start);
  return copied;
}

void AudioRendererHost::IPCAudioSource::OnClose(AudioOutputStream* stream) {
  StopWaitingForPacket();
}

void AudioRendererHost::IPCAudioSource::OnError(AudioOutputStream* stream,
                                                int code) {
  host_->SendErrorMessage(route_id_, stream_id_, code);
  // The following method call would cause this object to be destroyed on IO
  // thread.
  host_->DestroySource(this);
}

void AudioRendererHost::IPCAudioSource::NotifyPacketReady(size_t packet_size) {
  if (packet_size > packet_size_) {
    // If reported size is greater than capacity of the shared memory, close the
    // stream.
    host_->SendErrorMessage(route_id_, stream_id_, 0);
    // We don't need to do packet_read_event_.Signal() here because the
    // contained stream should be closed by the following call and OnClose will
    // be received.
    host_->DestroySource(this);
    return;
  }

  if (state_ == AudioOutputStream::STATE_CREATED) {
    // If we are in a created state, that means we are performing prefetching.
    uint8* packet = new uint8[packet_size];
    memcpy(packet, shared_memory_.memory(), packet_size);
    initial_buffers_.push_back(std::make_pair(packet, packet_size));
    // If there's not enough initial packets prepared, ask more.
    if (initial_buffers_.size() < stream_->GetNumBuffers()) {
      host_->Send(new ViewMsg_RequestAudioPacket(route_id_, stream_id_));
    } else {
      state_ = AudioOutputStream::STATE_STARTED;
      stream_->Start(this);
      host_->Send(new ViewMsg_NotifyAudioStreamStateChanged(
          route_id_, stream_id_, AudioOutputStream::STATE_STARTED, 0));
    }
  } else {
    AutoLock auto_lock(lock_);
    last_packet_size_ = packet_size;
    packet_read_event_.Signal();
  }
}

void AudioRendererHost::IPCAudioSource::StopWaitingForPacket() {
  AutoLock auto_lock(lock_);
  stop_providing_packets_ = true;
  last_packet_size_ = 0;
  packet_read_event_.Signal();
}

size_t AudioRendererHost::IPCAudioSource::SafeCopyBuffer(
    void* dest, size_t dest_size, const void* src, size_t src_size) {
  if (src_size > dest_size) {
    host_->SendErrorMessage(route_id_, stream_id_, 0);
    host_->DestroySource(this);
    return 0;
  }
  memcpy(dest, src, src_size);
  return src_size;
}

//-----------------------------------------------------------------------------
// AudioRendererHost implementations.

AudioRendererHost::AudioRendererHost(MessageLoop* message_loop)
    : process_id_(0),
      process_handle_(0),
      ipc_sender_(NULL),
      io_loop_(message_loop) {
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

// Event received when IPC channel is connected to the renderer process.
void AudioRendererHost::IPCChannelConnected(int process_id,
                                            base::ProcessHandle process_handle,
                                            IPC::Message::Sender* ipc_sender) {
  DCHECK(MessageLoop::current() == io_loop_);
  process_id_ = process_id;
  process_handle_ = process_handle;
  ipc_sender_ = ipc_sender;
}

// Event received when IPC channel is closing.
void AudioRendererHost::IPCChannelClosing() {
  DCHECK(MessageLoop::current() == io_loop_);
  ipc_sender_ = NULL;
  process_handle_ = 0;
  process_id_ = 0;
  DestroyAllSources();
}

bool AudioRendererHost::OnMessageReceived(const IPC::Message& message,
                                          bool* message_was_ok) {
  if (!IsAudioRendererHostMessage(message))
    return false;
  *message_was_ok = true;

  IPC_BEGIN_MESSAGE_MAP_EX(AudioRendererHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateAudioStream, OnCreateStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartAudioStream, OnStartStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CloseAudioStream, OnCloseStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_NotifyAudioPacketReady, OnNotifyPacketReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetAudioVolume, OnGetVolume)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetAudioVolume, OnSetVolume)
  IPC_END_MESSAGE_MAP_EX()

  return true;
}

bool AudioRendererHost::IsAudioRendererHostMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case ViewHostMsg_CreateAudioStream::ID:
    case ViewHostMsg_StartAudioStream::ID:
    case ViewHostMsg_CloseAudioStream::ID:
    case ViewHostMsg_NotifyAudioPacketReady::ID:
    case ViewHostMsg_GetAudioVolume::ID:
    case ViewHostMsg_SetAudioVolume::ID:
      return true;
    default:
      break;
    }
    return false;
}

void AudioRendererHost::OnCreateStream(
    const IPC::Message& msg, int stream_id,
    const ViewHostMsg_Audio_CreateStream& params) {
  DCHECK(MessageLoop::current() == io_loop_);
  DCHECK(Lookup(msg.routing_id(), stream_id) == NULL);

  IPCAudioSource* source = IPCAudioSource::CreateIPCAudioSource(
      this,
      process_id_,
      msg.routing_id(),
      stream_id,
      process_handle_,
      params.format,
      params.channels,
      params.sample_rate,
      params.bits_per_sample,
      params.packet_size);

  // If we have created the source successfully, adds it to the map.
  if (source) {
    sources_.insert(
        std::make_pair(
            SourceID(source->route_id(), source->stream_id()), source));
  }
}

void AudioRendererHost::OnStartStream(const IPC::Message& msg, int stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = Lookup(msg.routing_id(), stream_id);
  if (source) {
    source->Start();
  } else {
    SendErrorMessage(msg.routing_id(), stream_id, 0);
  }
}

void AudioRendererHost::OnCloseStream(const IPC::Message& msg, int stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = Lookup(msg.routing_id(), stream_id);
  if (source) {
    DestroySource(source);
  }
}

void AudioRendererHost::OnSetVolume(const IPC::Message& msg, int stream_id,
                                    double left_channel, double right_channel) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = Lookup(msg.routing_id(), stream_id);
  if (source) {
    source->SetVolume(left_channel, right_channel);
  } else {
    SendErrorMessage(msg.routing_id(), stream_id, 0);
  }
}

void AudioRendererHost::OnGetVolume(const IPC::Message& msg, int stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = Lookup(msg.routing_id(), stream_id);
  if (source) {
    source->GetVolume();
  } else {
    SendErrorMessage(msg.routing_id(), stream_id, 0);
  }
}

void AudioRendererHost::OnNotifyPacketReady(const IPC::Message& msg,
                                            int stream_id, size_t packet_size) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = Lookup(msg.routing_id(), stream_id);
  if (source) {
    source->NotifyPacketReady(packet_size);
  } else {
    SendErrorMessage(msg.routing_id(), stream_id, 0);
  }
}

void AudioRendererHost::OnInitialized() {
  DCHECK(MessageLoop::current() == io_loop_);
  // Increase the ref count of this object so it is active until we do
  // Release().
  AddRef();
}

void AudioRendererHost::OnDestroyed() {
  DCHECK(MessageLoop::current() == io_loop_);
  ipc_sender_ = NULL;
  process_handle_ = 0;
  process_id_ = 0;
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
    sources_.erase(SourceID(source->route_id(), source->stream_id()));
    source->Close();
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

AudioRendererHost::IPCAudioSource* AudioRendererHost::Lookup(int route_id,
                                                             int stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  SourceMap::iterator i = sources_.find(SourceID(route_id, stream_id));
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
