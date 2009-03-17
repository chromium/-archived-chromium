// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simple class that implements the VideoFrame interface with memory allocated
// on the system heap.  This class supports every format defined in the
// VideoSurface::Format enum.  The implementation attempts to properly align
// allocations for maximum system bus efficency.
#ifndef MEDIA_BASE_VIDEO_FRAME_IMPL_H_
#define MEDIA_BASE_VIDEO_FRAME_IMPL_H_

#include "media/base/buffers.h"

namespace media {

class VideoFrameImpl : public VideoFrame {
 public:
  static void CreateFrame(VideoSurface::Format format,
                          size_t width,
                          size_t height,
                          base::TimeDelta timestamp,
                          base::TimeDelta duration,
                          scoped_refptr<VideoFrame>* frame_out);

  // Implementation of VideoFrame.
  virtual bool Lock(VideoSurface* surface);
  virtual void Unlock();

 private:
  // Clients must use the static CreateFrame() method to create a new frame.
  VideoFrameImpl(VideoSurface::Format format,
                 size_t video_width,
                 size_t video_height);

  virtual ~VideoFrameImpl();

  bool AllocateRGB(size_t bytes_per_pixel);
  bool AllocateYUV();

  bool locked_;
  VideoSurface surface_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameImpl);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_IMPL_H_
