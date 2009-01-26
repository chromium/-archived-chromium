// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
#define CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_

#include "media/base/filters.h"

class AudioRendererImpl : public media::AudioRenderer {
 public:
  AudioRendererImpl();

  // media::MediaFilter implementation.
  virtual void Stop();

  // media::AudioRenderer implementation.
  virtual bool Initialize(media::AudioDecoder* decoder);
  virtual void SetVolume(float volume);

 protected:
  virtual ~AudioRendererImpl();

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

#endif  // CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
