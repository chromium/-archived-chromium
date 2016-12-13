// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <math.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/audio/audio_output.h"
#include "media/audio/simple_sources.h"

//////////////////////////////////////////////////////////////////////////////
// SineWaveAudioSource implementation.

SineWaveAudioSource::SineWaveAudioSource(Format format, int channels,
                                         double freq, double sample_freq)
    : format_(format),
      channels_(channels),
      freq_(freq),
      sample_freq_(sample_freq) {
  // TODO(cpu): support other formats.
  DCHECK((format_ == FORMAT_16BIT_LINEAR_PCM) && (channels_ == 1));
}

// The implementation could be more efficient if a lookup table is constructed
// but it is efficient enough for our simple needs.
size_t SineWaveAudioSource::OnMoreData(AudioOutputStream* stream,
                                       void* dest, size_t max_size) {
  const double kTwoPi = 2.0 * 3.141592653589;
  double f = freq_ / sample_freq_;
  int16* sin_tbl = reinterpret_cast<int16*>(dest);
  size_t len = max_size / sizeof(int16);
  // The table is filled with s(t) = 32768*sin(2PI*f*t).
  for (size_t ix = 0; ix != len; ++ix) {
    double th = kTwoPi * ix * f;
    sin_tbl[ix] = static_cast<int16>((1 << 15) * sin(th));
  }
  return max_size;
}

void SineWaveAudioSource::OnClose(AudioOutputStream* stream) {
}

void SineWaveAudioSource::OnError(AudioOutputStream* stream, int code) {
  NOTREACHED();
}

//////////////////////////////////////////////////////////////////////////////
// PushSource implementation.

PushSource::PushSource(size_t packet_size)
    : packet_size_(packet_size),
      buffered_bytes_(0),
      front_buffer_consumed_(0) {
}

PushSource::~PushSource() {
  CleanUp();
}

size_t PushSource::OnMoreData(AudioOutputStream* stream,
                              void* dest, size_t max_size) {
  size_t copied = 0;
  while (copied < max_size) {
    AutoLock auto_lock(lock_);

    // Under lock processing in this scope.
    if (!packets_.size())
      break;
    Packet packet = packets_.front();
    size_t size = std::min(max_size - copied,
                           packet.size - front_buffer_consumed_);
    memcpy(static_cast<char*>(dest) + copied,
           packet.buffer + front_buffer_consumed_,
           size);
    front_buffer_consumed_ += size;
    buffered_bytes_ -= size;
    copied += size;
    if (front_buffer_consumed_ == packet.size) {
      delete [] packet.buffer;
      packets_.pop_front();
      front_buffer_consumed_ = 0;
    }
  }
  return copied;
}

void PushSource::OnClose(AudioOutputStream* stream) {
  CleanUp();
}

void PushSource::OnError(AudioOutputStream* stream, int code) {
  NOTREACHED();
}

// TODO(cpu): Manage arbitrary large sizes.
bool PushSource::Write(const void *data, size_t len) {
  if (len == 0) {
    NOTREACHED();
    return false;
  }
  Packet packet = { new char[len], len };
  memcpy(packet.buffer, data, packet.size);
  // Under lock processing here.
  AutoLock auto_lock(lock_);
  packets_.push_back(packet);
  buffered_bytes_ += len;
  return true;
}

size_t PushSource::UnProcessedBytes() {
  AutoLock auto_lock(lock_);
  return buffered_bytes_;
}

void PushSource::CleanUp() {
  AutoLock auto_lock(lock_);
  PacketList::const_iterator it;
  for (it = packets_.begin(); it != packets_.end(); ++it) {
    delete it->buffer;
    buffered_bytes_ -= it->size;
  }
  packets_.clear();
}
