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
#include "media/filters/audio_renderer_base.h"

namespace media {

class NullAudioRenderer : public AudioRendererBase, PlatformThread::Delegate {
 public:
  // FilterFactory provider.
  static FilterFactory* CreateFilterFactory() {
    return new FilterFactoryImpl0<NullAudioRenderer>();
  }

  // Compatible with any audio/x-uncompressed MediaFormat.
  static bool IsMediaFormatSupported(const MediaFormat* media_format);

  // MediaFilter implementation.
  virtual void SetPlaybackRate(float playback_rate);

  // AudioRenderer implementation.
  virtual void SetVolume(float volume);

  // PlatformThread::Delegate implementation.
  virtual void ThreadMain();

 protected:
  // Only allow a factory to create this class.
  friend class FilterFactoryImpl0<NullAudioRenderer>;
  NullAudioRenderer();
  virtual ~NullAudioRenderer();

  // AudioRendererBase implementation.
  virtual bool OnInitialize(const MediaFormat* media_format);
  virtual void OnStop();

 private:
  // Current playback rate.
  float playback_rate_;

  // A number to convert bytes written in FillBuffer to milliseconds based on
  // the audio format.  Calculated in OnInitialize by looking at the decoder's
  // MediaFormat.
  size_t bytes_per_millisecond_;

  // A buffer passed to FillBuffer to advance playback.
  scoped_ptr<uint8> buffer_;
  size_t buffer_size_;

  // Separate thread used to throw away data.
  PlatformThreadHandle thread_;

  // Shutdown flag.
  bool shutdown_;

  DISALLOW_COPY_AND_ASSIGN(NullAudioRenderer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_NULL_AUDIO_RENDERER_H_
