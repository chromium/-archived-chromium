// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/process.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"

AudioRendererHost::IPCAudioSource::IPCAudioSource(
    AudioRendererHost* host,
    int32 id,
    AudioOutputStream* stream,
    IPC::Message::Sender* sender,
    base::ProcessHandle process,
    size_t packet_size)
    : host_(host),
      id_(id),
      stream_(stream),
      sender_(sender) {
  // Make sure we can create and map the shared memory.
  DCHECK(shared_memory_.Create(L"", false, false, packet_size) &&
         shared_memory_.Map(packet_size));
  // Also make sure we can create the buffer and share to render process.
  DCHECK(shared_memory_.ShareToProcess(process, &foreign_memory_handle_));
}

AudioRendererHost::IPCAudioSource::~IPCAudioSource() {
}

size_t AudioRendererHost::IPCAudioSource::OnMoreData(AudioOutputStream* stream,
                                                     void* dest,
                                                     size_t max_size) {
  // TODO(hclam): send an IPC message to renderer provided with a
  // SharedMemoryHandle for audio renderer inside render process to fill in.
  // We should sleep until we receive a notification from render process that
  // the buffer is ready or this source is closed or an error is encountered.
  // Stuff do there here:
  // 1. Prepare the SharedMemory.
  // 2. Send an IPC.
  // 3. Wait until we receive notification.
  return 0;
}

void AudioRendererHost::IPCAudioSource::OnClose(AudioOutputStream* stream) {
  // TODO(hclam): should set a flag here and wakes up the thread waiting for
  // audio buffer. Should also make sure this call come from the thread as this
  // object is created.
  // Stuff to do here:
  // 1. Send an IPC to renderer about close complete.
  // 2. Remove this object from host.
  host_->DestroySource(id_);
}

void AudioRendererHost::IPCAudioSource::OnError(AudioOutputStream* stream,
                                                int code) {
  // TODO(hclam): make sure this method is received in the same thread as this
  // object is created and remove this object and from the map gracefully.
  // Stuff to do here:
  // 1. Send an IPC to renderer about the error.
  // 2. Close the stream so it would get closed.
  // TODO(cpu): make sure it is safe to close() in this method.
  stream_->Close();
}

void AudioRendererHost::IPCAudioSource::NotifyPacketReady() {
  // TODO(hclam): wake the thread waiting for buffer.
}

AudioRendererHost::AudioRendererHost(MessageLoop* message_loop)
    : next_id_(INVALID_ID+1),
      io_loop_(message_loop) {
  // Make sure we perform actual initialization operations in the thread where
  // this object should live.
  io_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioRendererHost::OnInitialized));
}

AudioRendererHost::~AudioRendererHost() {
}

int AudioRendererHost::CreateStream(
    IPC::Message::Sender* sender, base::ProcessHandle handle,
    AudioManager::Format format, int channels, int sample_rate,
    int bits_per_sample, size_t packet_size) {
  DCHECK(MessageLoop::current() == io_loop_);

  // Create the stream in the first place.
  AudioOutputStream* stream = AudioManager::GetAudioManager()->MakeAudioStream(
      format, channels, sample_rate, bits_per_sample);
  if (!stream)
    return INVALID_ID;

  // Try to open the stream if we can create it.
  if (stream->Open(packet_size)) {
    // Create the containing IPCAudioSource and save it to the map.
    IPCAudioSource* source =
        new IPCAudioSource(this, next_id_++, stream, sender, handle,
                           packet_size);
    sources_.AddWithID(source, source->id());
    return source->id();
  }
  return INVALID_ID;
}

bool AudioRendererHost::Start(int stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = sources_.Lookup(stream_id);
  if (source) {
    source->stream()->Start(source);
    return true;
  }
  return false;
}

bool AudioRendererHost::Stop(int stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = sources_.Lookup(stream_id);
  if (source) {
    source->stream()->Stop();
    return true;
  }
  return false;
}

bool AudioRendererHost::Close(int stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = sources_.Lookup(stream_id);
  if (source) {
    source->stream()->Close();
    return true;
  }
  return false;
}

bool AudioRendererHost::SetVolume(
    int stream_id, double left_channel, double right_channel) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = sources_.Lookup(stream_id);
  if (source) {
    source->stream()->SetVolume(left_channel, right_channel);
  }
  return false;
}

bool AudioRendererHost::GetVolume(
    int stream_id, double* left_channel, double* right_channel) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = sources_.Lookup(stream_id);
  if (source) {
    source->stream()->GetVolume(left_channel, right_channel);
    return true;
  }
  return false;
}

void AudioRendererHost::NotifyPacketReady(int stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = sources_.Lookup(stream_id);
  if (source) {
    source->NotifyPacketReady();
  }
}

void AudioRendererHost::DestroyAllStreams() {
  DCHECK(MessageLoop::current() == io_loop_);
  // TODO(hclam): iterate on the map, close and delete every stream, and clear
  // the map.
}

void AudioRendererHost::DestroySource(int stream_id) {
  DCHECK(MessageLoop::current() == io_loop_);
  IPCAudioSource* source = sources_.Lookup(stream_id);
  if (source) {
    sources_.Remove(stream_id);
    delete source;
  }
}

void AudioRendererHost::Destroy() {
  // Post a message to the thread where this object should live and do the
  // actual operations there.
  io_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &AudioRendererHost::OnDestroyed));
}

void AudioRendererHost::OnInitialized() {
  DCHECK(MessageLoop::current() == io_loop_);

  // Increase the ref count of this object so it is active until we do
  // Release().
  AddRef();

  // Also create the AudioManager singleton in this thread.
  // TODO(hclam): figure out a better location to initialize the AudioManager
  // singleton.
  AudioManager::GetAudioManager();
}

void AudioRendererHost::OnDestroyed() {
  DCHECK(MessageLoop::current() == io_loop_);

  // Destroy audio streams only in the thread it should happen.
  // TODO(hclam): make sure we don't call IPC::Message::Sender inside
  // IPCAudioSource because it is most likely be destroyed.
  DestroyAllStreams();

  // Decrease the reference to this object, which may lead to self-destruction.
  Release();
}
