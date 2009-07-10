// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/histogram.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "base/waitable_event.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/render_messages.h"

namespace {

void RecordRoundTripLatency(base::TimeDelta latency) {
  static ThreadSafeHistogram histogram("Audio.IPC_RoundTripLatency",
                                       1, 1000, 100);
  histogram.AddTime(latency);
}

void RecordReceiveLatency(base::TimeDelta latency) {
  static ThreadSafeHistogram histogram("Audio.IPC_Browser_ReceiveLatency",
                                       1, 500, 100);
  histogram.AddTime(latency);
}

void RecordProcessTime(base::TimeDelta latency) {
  static ThreadSafeHistogram histogram("Audio.IPC_Browser_ProcessTime",
                                       1, 100, 100);
  histogram.AddTime(latency);
}

// This constant governs the hardware audio buffer size, this value should be
// choosen carefully and is platform specific.
const int kSamplesPerHardwarePacket = 8192;

const size_t kMegabytes = 1024 * 1024;

// The following parameters limit the request buffer and packet size from the
// renderer to avoid renderer from requesting too much memory.
const size_t kMaxDecodedPacketSize = 2 * kMegabytes;
const size_t kMaxBufferCapacity = 5 * kMegabytes;
static const int kMaxChannels = 32;
static const int kMaxBitsPerSample = 64;
static const int kMaxSampleRate = 192000;

}  // namespace

//-----------------------------------------------------------------------------
// AudioRendererHost::IPCAudioSource implementations.

AudioRendererHost::IPCAudioSource::IPCAudioSource(
    AudioRendererHost* host,
    int process_id,
    int route_id,
    int stream_id,
    AudioOutputStream* stream,
    size_t hardware_packet_size,
    size_t decoded_packet_size,
    size_t buffer_capacity)
    : host_(host),
      process_id_(process_id),
      route_id_(route_id),
      stream_id_(stream_id),
      stream_(stream),
      hardware_packet_size_(hardware_packet_size),
      decoded_packet_size_(decoded_packet_size),
      buffer_capacity_(buffer_capacity),
      state_(AudioOutputStream::STATE_CREATED),
      push_source_(hardware_packet_size),
      outstanding_request_(false),
      last_copied_bytes_(0) {
}

AudioRendererHost::IPCAudioSource::~IPCAudioSource() {
  DCHECK_EQ(AudioOutputStream::STATE_STOPPED, state_);
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
        size_t decoded_packet_size,
        size_t buffer_capacity) {
  // Perform come preliminary checks on the parameters.
  // Make sure the renderer didn't ask for too much memory.
  if (buffer_capacity > kMaxBufferCapacity ||
      decoded_packet_size > kMaxDecodedPacketSize)
    return NULL;

  // Make sure the packet size and buffer capacity parameters are valid.
  if (buffer_capacity < decoded_packet_size)
    return NULL;

  if (channels <= 0 || channels > kMaxChannels)
    return NULL;

  if (sample_rate <= 0 || sample_rate > kMaxSampleRate)
    return NULL;

  if (bits_per_sample <= 0 || bits_per_sample > kMaxBitsPerSample)
    return NULL;

  // Create the stream in the first place.
  AudioOutputStream* stream =
      AudioManager::GetAudioManager()->MakeAudioStream(
          format, channels, sample_rate, bits_per_sample);

  size_t hardware_packet_size = kSamplesPerHardwarePacket * channels *
                                bits_per_sample / 8;
  if (stream && !stream->Open(hardware_packet_size)) {
    stream->Close();
    stream = NULL;
  }

  if (stream) {
    IPCAudioSource* source = new IPCAudioSource(
        host,
        process_id,
        route_id,
        stream_id,
        stream,
        hardware_packet_size,
        decoded_packet_size,
        buffer_capacity);
    // If we can open the stream, proceed with sharing the shared memory.
    base::SharedMemoryHandle foreign_memory_handle;

    // Try to create, map and share the memory for the renderer process.
    // If they all succeeded then send a message to renderer to indicate
    // success.
    if (source->shared_memory_.Create(L"",
                                      false,
                                      false,
                                      decoded_packet_size) &&
        source->shared_memory_.Map(decoded_packet_size) &&
        source->shared_memory_.ShareToProcess(process_handle,
                                              &foreign_memory_handle)) {
      host->Send(new ViewMsg_NotifyAudioStreamCreated(
          route_id, stream_id, foreign_memory_handle, decoded_packet_size));

      // Also request the first packet to kick start the pre-rolling.
      source->StartBuffering();
      return source;
    }
    source->Close();
    delete source;
  }

  host->SendErrorMessage(route_id, stream_id, 0);
  return NULL;
}

void AudioRendererHost::IPCAudioSource::Start() {
  // We can start from created or paused state.
  if (!stream_ ||
      (state_ != AudioOutputStream::STATE_CREATED &&
       state_ != AudioOutputStream::STATE_PAUSED))
    return;

  stream_->Start(this);

  // Update the state and notify renderer.
  state_ = AudioOutputStream::STATE_STARTED;
  host_->Send(new ViewMsg_NotifyAudioStreamStateChanged(
      route_id_, stream_id_, state_, 0));
}

void AudioRendererHost::IPCAudioSource::Pause() {
  // We can pause from started state.
  if (!stream_ ||
      state_ != AudioOutputStream::STATE_STARTED)
    return;

  // TODO(hclam): use stop to simulate pause, make sure the AudioOutpusStream
  // can be started again after stop.
  stream_->Stop();

  // Update the state and notify renderer.
  state_ = AudioOutputStream::STATE_PAUSED;
  host_->Send(new ViewMsg_NotifyAudioStreamStateChanged(
      route_id_, stream_id_, state_, 0));
}

void AudioRendererHost::IPCAudioSource::Close() {
  if (!stream_)
    return;

  stream_->Stop();
  stream_->Close();
  // After stream is closed it is destroyed, so don't keep a reference to it.
  stream_ = NULL;

  // Update the current state.
  state_ = AudioOutputStream::STATE_STOPPED;
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
  size_t size = push_source_.OnMoreData(stream, dest, max_size);
  {
    AutoLock auto_lock(lock_);
    last_copied_bytes_ = size;
    SubmitPacketRequest(&auto_lock);
  }
  return size;
}

void AudioRendererHost::IPCAudioSource::OnClose(AudioOutputStream* stream) {
  push_source_.OnClose(stream);
}

void AudioRendererHost::IPCAudioSource::OnError(AudioOutputStream* stream,
                                                int code) {
  host_->SendErrorMessage(route_id_, stream_id_, code);
  // The following method call would cause this object to be destroyed on IO
  // thread.
  host_->DestroySource(this);
}

void AudioRendererHost::IPCAudioSource::NotifyPacketReady(
    size_t decoded_packet_size) {
  bool ok = true;
  {
    AutoLock auto_lock(lock_);
    outstanding_request_ = false;
#ifdef IPC_MESSAGE_LOG_ENABLED
    if (IPC::Logging::current() && IPC::Logging::current()->Enabled()) {
      RecordRoundTripLatency(base::Time::Now() - outstanding_request_time_);
    }
#endif
    // If reported size is greater than capacity of the shared memory, we have
    // an error.
    if (decoded_packet_size <= decoded_packet_size_) {
      for (size_t i = 0; i < decoded_packet_size; i += hardware_packet_size_) {
        size_t size = std::min(decoded_packet_size - i, hardware_packet_size_);
        ok &= push_source_.Write(
            static_cast<char*>(shared_memory_.memory()) + i, size);
        if (!ok)
          break;
      }

      // Submit packet request if we have written something.
      if (ok)
        SubmitPacketRequest(&auto_lock);
    }
  }

  // We have received a data packet but we didn't finish writing to push source.
  // There's error an error and we should stop.
  if (!ok) {
    NOTREACHED();
  }
}

void AudioRendererHost::IPCAudioSource::SubmitPacketRequest_Locked() {
  lock_.AssertAcquired();
  // Submit a new request when these two conditions are fulfilled:
  // 1. No outstanding request
  // 2. There's space for data of the new request.
  if (!outstanding_request_ &&
      (push_source_.UnProcessedBytes() + decoded_packet_size_ <=
       buffer_capacity_)) {
    outstanding_request_ = true;
    outstanding_request_time_ = base::Time::Now();

    // This variable keeps track of the total amount of bytes buffered for
    // the associated AudioOutputStream. This value should consist of bytes
    // buffered in AudioOutputStream and those kept inside |push_source_|.
    // TODO(hclam): since we have no information about the amount of buffered
    // bytes in the hardware buffer in AudioOutputStream, we make our best
    // guess by using the amount of the last copy. This should be a good guess
    // for Windows since it does double buffering but we shold change this
    // when AudioOutputStream has the API to query remaining buffer.
    size_t buffered_bytes = last_copied_bytes_ +
                            push_source_.UnProcessedBytes();
    host_->Send(
        new ViewMsg_RequestAudioPacket(
            route_id_,
            stream_id_,
            buffered_bytes,
            outstanding_request_time_.ToInternalValue()));
  }
}

void AudioRendererHost::IPCAudioSource::SubmitPacketRequest(AutoLock* alock) {
 if (alock) {
   SubmitPacketRequest_Locked();
 } else {
   AutoLock auto_lock(lock_);
   SubmitPacketRequest_Locked();
 }
}

void AudioRendererHost::IPCAudioSource::StartBuffering() {
  SubmitPacketRequest(NULL);
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
  DCHECK(sources_.empty());
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
    IPC_MESSAGE_HANDLER(ViewHostMsg_PauseAudioStream, OnPauseStream)
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
    case ViewHostMsg_PauseAudioStream::ID:
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
      params.packet_size,
      params.buffer_capacity);

  // If we have created the source successfully, adds it to the map.
  if (source) {
    sources_.insert(
        std::make_pair(
            SourceID(source->route_id(), source->stream_id()), source));
  } else {
    SendErrorMessage(msg.routing_id(), stream_id, 0);
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

void AudioRendererHost::OnPauseStream(const IPC::Message& msg, int stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = Lookup(msg.routing_id(), stream_id);
  if (source) {
    source->Pause();
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
#ifdef IPC_MESSAGE_LOG_ENABLED
  if (IPC::Logging::current() && IPC::Logging::current()->Enabled()) {
    RecordReceiveLatency(base::Time::FromInternalValue(msg.received_time()) -
                         base::Time::FromInternalValue(msg.sent_time()));
    RecordProcessTime(base::Time::Now() -
                      base::Time::FromInternalValue(msg.received_time()));
  }
#endif
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
