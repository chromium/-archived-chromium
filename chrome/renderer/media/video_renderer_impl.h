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

#ifndef CHROME_RENDERER_MEDIA_VIDEO_RENDERER_IMPL_H_
#define CHROME_RENDERER_MEDIA_VIDEO_RENDERER_IMPL_H_

#include "base/gfx/platform_canvas.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "chrome/renderer/webmediaplayer_delegate_impl.h"
#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/filters/video_renderer_base.h"
#include "webkit/glue/webmediaplayer_delegate.h"

class VideoRendererImpl : public media::VideoRendererBase {
 public:
  // Methods for painting called by the WebMediaPlayerDelegateImpl

  // This method is called with the same rect as the Paint method and could
  // be used by future implementations to implement an improved color space +
  // scale code on a seperate thread.  Since we always do the streach on the
  // same thread as the Paint method, we just ignore the call for now.
  virtual void SetRect(const gfx::Rect& rect) {}

  // Paint the current front frame on the |canvas| streaching it to fit the
  // |dest_rect|
  virtual void Paint(skia::PlatformCanvas* canvas, const gfx::Rect& dest_rect);

  // Static method for creating factory for this object.
  static media::FilterFactory* CreateFactory(
      WebMediaPlayerDelegateImpl* delegate) {
    return new media::FilterFactoryImpl1<
        VideoRendererImpl, WebMediaPlayerDelegateImpl*>(delegate);
  }

 private:
  // Method called by base class during initialization.
  virtual bool OnInitialize(size_t width, size_t height);

  // Method called by the VideoRendererBase when a repaint is needed.
  virtual void OnPaintNeeded();

  friend class media::FilterFactoryImpl1<VideoRendererImpl,
                                         WebMediaPlayerDelegateImpl*>;

  // Constructor and destructor are private.  Only the filter factory is
  // allowed to create instances.
  explicit VideoRendererImpl(WebMediaPlayerDelegateImpl* delegate);
  virtual ~VideoRendererImpl() {}

  // Internal method used by the Paint method to convert the specified video
  // frame to RGB, placing the converted pixels in the |current_frame_| bitmap.
  void CopyToCurrentFrame(media::VideoFrame* video_frame);

  // Pointer to our parent object that is called to request repaints.
  WebMediaPlayerDelegateImpl* delegate_;

  // An RGB bitmap used to convert the video frames.
  SkBitmap bitmap_;

  // These two members are used to determine if the |bitmap_| contains
  // an already converted image of the current frame.  IMPORTANT NOTE:  The
  // value of |last_converted_frame_| must only be used for comparison purposes,
  // and it should be assumed that the value of the pointer is INVALID unless
  // it matches the pointer returned from GetCurrentFrame().  Even then, just
  // to make sure, we compare the timestamp to be sure the bits in the
  // |current_frame_bitmap_| are valid.
  media::VideoFrame* last_converted_frame_;
  base::TimeDelta last_converted_timestamp_;

  // The size of the video.
  gfx::Size video_size_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererImpl);
};

#endif  // CHROME_RENDERER_MEDIA_VIDEO_RENDERER_IMPL_H_
