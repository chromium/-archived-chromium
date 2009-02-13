// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_NULL_AUDIO_RENDERER_H_
#define MEDIA_FILTERS_NULL_AUDIO_RENDERER_H_

// NullAudioRenderer effectively uses an extra thread to "throw away" the
// audio data at a rate resembling normal playback speed.  It's just like
// decoding to /dev/null!
//
// NullAudioRenderer can also be used in situations where the client has no
// audio device or we haven't written an audio implementation for a particular
// platform yet.
//
// It supports any type of MediaFormat as long as the mime type has been set to
// audio/x-uncompressed.  Playback rate is also supported and NullAudioRenderer
// will slow down and speed up accordingly.

#include <deque>

#include "base/lock.h"
#include "base/platform_thread.h"
#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filters.h"

namespace media {

class NullAudioRenderer : public AudioRenderer, PlatformThread::Delegate {
 public:
  static FilterFactory* CreateFilterFactory() {
    return new FilterFactoryImpl0<NullAudioRenderer>();
  }

  // Compatible with any audio/x-uncompressed MediaFormat.
  static bool IsMediaFormatSupported(const MediaFormat* media_format);

  // MediaFilter implementation.
  virtual void Stop();
  virtual void SetPlaybackRate(float playback_rate);

  // AudioRenderer implementation.
  virtual bool Initialize(AudioDecoder* decoder);
  virtual void SetVolume(float volume);

  // AssignableBuffer<NullAudioRenderer, BufferInterface> implementation.
  void OnAssignment(Buffer* buffer_in);

  // PlatformThread::Delegate implementation.
  virtual void ThreadMain();

 private:
  friend class FilterFactoryImpl0<NullAudioRenderer>;
  NullAudioRenderer();
  virtual ~NullAudioRenderer();

  // Posts a task on the pipeline thread to read a sample from the decoder.
  // Safe to call on any thread.
  void ScheduleRead();

  // Audio decoder.
  AudioDecoder* decoder_;

  // Current playback rate.
  float playback_rate_;

  // Queued audio data.
  typedef std::deque<Buffer*> BufferQueue;
  BufferQueue input_queue_;
  Lock input_lock_;

  // Seperate thread used to throw away data.
  PlatformThreadHandle thread_;

  // Various state flags.
  bool initialized_;
  bool shutdown_;

  DISALLOW_COPY_AND_ASSIGN(NullAudioRenderer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_NULL_AUDIO_RENDERER_H_
