// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// The video renderer implementation to be use by the media pipeline. It lives
// inside video renderer thread and also WebKit's main thread. We need to be
// extra careful about members shared by two different threads, especially
// video frame buffers.
//
// Methods called from WebKit's main thread:
//   Paint()
//   SetRect()

#ifndef CHROME_RENDERER_MEDIA_VIDEO_RENDERER_H_
#define CHROME_RENDERER_MEDIA_VIDEO_RENDERER_H_

#include "base/gfx/platform_canvas.h"
#include "base/gfx/rect.h"
#include "media/base/factory.h"
#include "media/base/filters.h"

class WebMediaPlayerDelegateImpl;

class VideoRendererImpl : public media::VideoRenderer {
 public:
  VideoRendererImpl(WebMediaPlayerDelegateImpl* delegate);

  // media::MediaFilter implementation.
  virtual void Stop();

  // media::VideoRenderer implementation.
  virtual bool Initialize(media::VideoDecoder* decoder);

  // Called from WebMediaPlayerDelegateImpl from WebKit's main thread.
  void Paint(skia::PlatformCanvas* canvas, const gfx::Rect& rect);

  // Called from WebMediaPlayerDelegateImpl from WebKit's main thread.
  void SetRect(const gfx::Rect& rect);

  // Static method for creating factory for this object.
  static media::FilterFactory* CreateFactory(
      WebMediaPlayerDelegateImpl* delegate) {
    return new media::FilterFactoryImpl1<
        VideoRendererImpl, WebMediaPlayerDelegateImpl*>(delegate);
  }

  // Answers question from the factory to see if we accept |format|.
  static bool IsMediaFormatSupported(const media::MediaFormat* format);

 protected:
  virtual ~VideoRendererImpl();

 private:
  WebMediaPlayerDelegateImpl* delegate_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererImpl);
};

#endif  // CHROME_RENDERER_MEDIA_VIDEO_RENDERER_H_

