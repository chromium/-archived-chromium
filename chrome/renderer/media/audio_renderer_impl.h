// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
#define CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_

#include "media/base/factory.h"
#include "media/base/filters.h"

class WebMediaPlayerDelegateImpl;

class AudioRendererImpl : public media::AudioRenderer {
 public:
  AudioRendererImpl(WebMediaPlayerDelegateImpl* delegate);

  // media::MediaFilter implementation.
  virtual void Stop();

  // media::AudioRenderer implementation.
  virtual bool Initialize(media::AudioDecoder* decoder);
  virtual void SetVolume(float volume);

  // Static method for creating factory for this object.
  static media::FilterFactory* CreateFactory(
      WebMediaPlayerDelegateImpl* delegate) {
    return new media::FilterFactoryImpl1<AudioRendererImpl,
                                         WebMediaPlayerDelegateImpl*>(delegate);
  }

  // Answers question from the factory to see if we accept |format|.
  static bool IsMediaFormatSupported(const media::MediaFormat* format);

 protected:
  virtual ~AudioRendererImpl();

 private:
  WebMediaPlayerDelegateImpl* delegate_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

#endif  // CHROME_RENDERER_MEDIA_AUDIO_RENDERER_IMPL_H_
