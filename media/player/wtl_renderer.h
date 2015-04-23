// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// Video renderer for media player.

#ifndef MEDIA_PLAYER_WTL_RENDERER_H_
#define MEDIA_PLAYER_WTL_RENDERER_H_

#include "media/filters/video_renderer_base.h"

class WtlVideoWindow;

class WtlVideoRenderer : public media::VideoRendererBase {
 public:
  explicit WtlVideoRenderer(WtlVideoWindow* window);

  static bool IsMediaFormatSupported(const media::MediaFormat& media_format);

 protected:
  // VideoRendererBase implementation.
  virtual bool OnInitialize(media::VideoDecoder* decoder);
  virtual void OnStop();
  virtual void OnFrameAvailable();

 private:
  // Only allow to be deleted by reference counting.
  friend class scoped_refptr<WtlVideoRenderer>;
  virtual ~WtlVideoRenderer();

  WtlVideoWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(WtlVideoRenderer);
};

#endif  // MEDIA_PLAYER_WTL_RENDERER_H_
