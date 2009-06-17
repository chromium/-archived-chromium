// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_manager_mac.h"
#include "media/audio/mac/audio_output_mac.h"

#include "base/basictypes.h"
#include "base/logging.h"

// Overview of operation:
// 1) An object of PCMQueueOutAudioOutputStream is created by the AudioManager
// factory: audio_man->MakeAudioStream(). This just fills some structure.
// 2) Next some thread will call Open(), at that point the underliying OS
// queue is created and the audio buffers allocated.
// 3) Then some thread will call Start(source) At this point the source will be
// called to fill the initial buffers in the context of that same thread.
// Then the OS queue is started which will create its own thread which
// periodically will call the source for more data as buffers are being
// consumed.
// 4) At some point some thread will call Stop(), which we handle by directly
// stoping the OS queue.
// 5) One more callback to the source could be delivered in in the context of
// the queue's own thread. Data, if any will be discared.
// 6) The same thread that called stop will call Close() where we cleanup
// and notifiy the audio manager, which likley will destroy this object.

// TODO(cpu): Remove the constant for this error when snow leopard arrives.
enum {
  kAudioQueueErr_EnqueueDuringReset = -66632
};

PCMQueueOutAudioOutputStream::PCMQueueOutAudioOutputStream(
    AudioManagerMac* manager, int channels, int sampling_rate,
    char bits_per_sample)
        : format_(),
          audio_queue_(NULL),
          buffer_(),
          source_(NULL),
          manager_(manager) {
  // We must have a manager.
  DCHECK(manager_);
  // A frame is one sample across all channels. In interleaved audio the per
  // frame fields identify the set of n |channels|. In uncompressed audio, a
  // packet is always one frame.
  format_.mSampleRate = sampling_rate;
  format_.mFormatID = kAudioFormatLinearPCM;
  format_.mFormatFlags = kLinearPCMFormatFlagIsPacked |
                         kLinearPCMFormatFlagIsSignedInteger;
  format_.mBitsPerChannel = bits_per_sample;
  format_.mChannelsPerFrame = channels;
  format_.mFramesPerPacket = 1;
  format_.mBytesPerPacket = (format_.mBitsPerChannel * channels) / 8;
  format_.mBytesPerFrame = format_.mBytesPerPacket;
}

PCMQueueOutAudioOutputStream::~PCMQueueOutAudioOutputStream() {
}

void PCMQueueOutAudioOutputStream::HandleError(OSStatus err) {
  // source_ can be set to NULL from another thread. We need to cache its
  // pointer while we operate here. Note that does not mean that the source
  // has been destroyed.
  AudioSourceCallback* source = source_;
  if (source)
    source->OnError(this, static_cast<int>(err));
  NOTREACHED() << "error code " << err;
}

bool PCMQueueOutAudioOutputStream::Open(size_t packet_size) {
  if (0 == packet_size) {
    // TODO(cpu) : Impelement default buffer computation.
    return false;
  }
  // Create the actual queue object and let the OS use its own thread to
  // run its CFRunLoop.
  OSStatus err = AudioQueueNewOutput(&format_, RenderCallback, this, NULL,
                                     kCFRunLoopCommonModes, 0, &audio_queue_);
  if (err != noErr) {
    HandleError(err);
    return false;
  }
  // Allocate the hardware-managed buffers.
  for(size_t ix = 0; ix != kNumBuffers; ++ix) {
    err = AudioQueueAllocateBuffer(audio_queue_, packet_size, &buffer_[ix]);
    if (err != noErr) {
      HandleError(err);
      return false;
    }
  }
  // Set initial volume here.
  err = AudioQueueSetParameter(audio_queue_, kAudioQueueParam_Volume, 1.0);
  if (err != noErr) {
    HandleError(err);
    return false;
  }
  return true;
}

void PCMQueueOutAudioOutputStream::Close() {
  // It is valid to call Close() before calling Open(), thus audio_queue_
  // might be NULL.
  if (audio_queue_) {
    OSStatus err = 0;
    for (size_t ix = 0; ix != kNumBuffers; ++ix) {
      if (buffer_[ix]) {
        err = AudioQueueFreeBuffer(audio_queue_, buffer_[ix]);
        if (err != noErr) {
          HandleError(err);
          break;
        }
      }
    }
    err = AudioQueueDispose(audio_queue_, true);
    if (err != noErr)
      HandleError(err);
  }
  // Inform the audio manager that we have been closed. This can cause our
  // destruction.
  manager_->ReleaseStream(this);
}

void PCMQueueOutAudioOutputStream::Stop() {
  // We request a synchronous stop, so the next call can take some time. In
  // the windows implementation we block here as well.
  source_ = NULL;
  // We set the source to null to signal to the data queueing thread it can stop
  // queueing data, however at most one callback might still be in flight which
  // could attempt to enqueue right after the next call. Rather that trying to
  // use a lock we rely on the internal Mac queue lock so the enqueue might
  // succeed or might fail but it won't crash or leave the queue itself in an
  // inconsistent state.
  OSStatus err = AudioQueueStop(audio_queue_, true);
  if (err != noErr)
    HandleError(err);
}

void PCMQueueOutAudioOutputStream::SetVolume(double left_level,
                                             double right_level) {
  // TODO(cpu): Implement.
}

void PCMQueueOutAudioOutputStream::GetVolume(double* left_level,
                                             double* right_level) {
  // TODO(cpu): Implement.
}

size_t PCMQueueOutAudioOutputStream::GetNumBuffers() {
  return kNumBuffers;
}

// Note to future hackers of this function: Do not add locks here because we
// call out to third party source that might do crazy things including adquire
// external locks or somehow re-enter here because its legal for them to call
// some audio functions.
void PCMQueueOutAudioOutputStream::RenderCallback(void* p_this,
                                                  AudioQueueRef queue,
                                                  AudioQueueBufferRef buffer) {
  PCMQueueOutAudioOutputStream* audio_stream =
      static_cast<PCMQueueOutAudioOutputStream*>(p_this);
  // Call the audio source to fill the free buffer with data. Not having a
  // source means that the queue has been closed. This is not an error.
  AudioSourceCallback* source = audio_stream->source_;
  if (!source)
    return;
  size_t capacity = buffer->mAudioDataBytesCapacity;
  size_t filled = source->OnMoreData(audio_stream, buffer->mAudioData, capacity);
  if (filled > capacity) {
    // User probably overran our buffer.
    audio_stream->HandleError(0);
    return;
  }
  buffer->mAudioDataByteSize = filled;
  if (NULL == queue)
    return;
  // Queue the audio data to the audio driver.
  OSStatus err = AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
  if (err != noErr) {
    if (err == kAudioQueueErr_EnqueueDuringReset) {
      // This is the error you get if you try to enqueue a buffer and the
      // queue has been closed. Not really a problem if indeed the queue
      // has been closed.
      if (!audio_stream->source_)
        return;
    }
    audio_stream->HandleError(err);
  }
}

void PCMQueueOutAudioOutputStream::Start(AudioSourceCallback* callback) {
  DCHECK(callback);
  OSStatus err = AudioQueueStart(audio_queue_, NULL);
  if (err != noErr) {
    HandleError(err);
    return;
  }
  source_ = callback;
  // Ask the source to pre-fill all our buffers before playing.
  for(size_t ix = 0; ix != kNumBuffers; ++ix) {
    RenderCallback(this, NULL, buffer_[ix]);
  }
  // Queue the buffers to the audio driver, sounds starts now.
  for(size_t ix = 0; ix != kNumBuffers; ++ix) {
    err = AudioQueueEnqueueBuffer(audio_queue_, buffer_[ix], 0, NULL);
    if (err != noErr) {
      HandleError(err);
      return;
    }
  }
}

