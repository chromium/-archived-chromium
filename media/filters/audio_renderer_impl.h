// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_

// This is the default implementation of AudioRenderer, which uses the audio
// interfaces to open an audio device.  Although it cannot be used in the
// sandbox, it serves as a reference implementation and can be used in other
// applications such as the test player.

#include <deque>

#include "base/lock.h"
#include "media/audio/audio_output.h"
#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/filters/audio_renderer_base.h"

namespace media {

class AudioRendererImpl : public AudioRendererBase,
                          public AudioOutputStream::AudioSourceCallback {
 public:
  // FilterFactory provider.
  static FilterFactory* CreateFilterFactory() {
    return new FilterFactoryImpl0<AudioRendererImpl>();
  }

  static bool IsMediaFormatSupported(const MediaFormat& media_format);

  // MediaFilter implementation.
  virtual void SetPlaybackRate(float playback_rate);

  // AudioRenderer implementation.
  virtual void SetVolume(float volume);

  // AudioSourceCallback implementation.
  virtual size_t OnMoreData(AudioOutputStream* stream, void* dest, size_t len);
  virtual void OnClose(AudioOutputStream* stream);
  virtual void OnError(AudioOutputStream* stream, int code);

 protected:
  // Only allow a factory to create this class.
  friend class FilterFactoryImpl0<AudioRendererImpl>;
  AudioRendererImpl();
  virtual ~AudioRendererImpl();

  // AudioRendererBase implementation.
  virtual bool OnInitialize(const MediaFormat& media_format);
  virtual void OnStop();

 private:
  // Playback rate
  // 0.0f is paused, 0.5f is half speed, 1.0f is normal, 2.0f is double speed.
  // Rate should normally be any value between 0.5 and 3.0.
  float playback_rate_;

  // Audio output stream device.
  AudioOutputStream* stream_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_
