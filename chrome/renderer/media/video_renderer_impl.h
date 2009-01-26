// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_VIDEO_RENDERER_H_
#define CHROME_RENDERER_MEDIA_VIDEO_RENDERER_H_

#include "media/base/filters.h"

class VideoRendererImpl : public media::VideoRenderer {
 public:
  VideoRendererImpl();

  // media::MediaFilter implementation.
  virtual void Stop();

  // media::VideoRenderer implementation.
  virtual bool Initialize(media::VideoDecoder* decoder);

 protected:
  virtual ~VideoRendererImpl();

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoRendererImpl);
};

#endif  // CHROME_RENDERER_MEDIA_VIDEO_RENDERER_H_

