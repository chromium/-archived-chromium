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

#ifndef WEBKIT_GLUE_MEDIA_VIDEO_RENDERER_IMPL_H_
#define WEBKIT_GLUE_MEDIA_VIDEO_RENDERER_IMPL_H_

#include "base/gfx/platform_canvas.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/filters/video_renderer_base.h"
#include "webkit/api/public/WebMediaPlayer.h"
#include "webkit/glue/webmediaplayer_impl.h"

namespace webkit_glue {

class VideoRendererImpl : public media::VideoRendererBase {
 public:
  // Methods for painting called by the WebMediaPlayerImpl::Proxy

  // This method is called with the same rect as the Paint method and could
  // be used by future implementations to implement an improved color space +
  // scale code on a separate thread.  Since we always do the stretch on the
  // same thread as the Paint method, we just ignore the call for now.
  virtual void SetRect(const gfx::Rect& rect);

  // Paint the current front frame on the |canvas| stretching it to fit the
  // |dest_rect|
  virtual void Paint(skia::PlatformCanvas* canvas, const gfx::Rect& dest_rect);

  // Static method for creating factory for this object.
  static media::FilterFactory* CreateFactory(WebMediaPlayerImpl::Proxy* proxy) {
    return new media::FilterFactoryImpl1<VideoRendererImpl,
                                         WebMediaPlayerImpl::Proxy*>(proxy);
  }

  // FilterFactoryImpl1 implementation.
  static bool IsMediaFormatSupported(const media::MediaFormat& media_format);

 protected:
  // Method called by VideoRendererBase during initialization.
  virtual bool OnInitialize(media::VideoDecoder* decoder);

  // Method called by the VideoRendererBase when stopping.
  virtual void OnStop();

  // Method called by the VideoRendererBase when a frame is available.
  virtual void OnFrameAvailable();

 private:
  // Only the filter factories can create instances.
  friend class media::FilterFactoryImpl1<VideoRendererImpl,
                                         WebMediaPlayerImpl::Proxy*>;
  explicit VideoRendererImpl(WebMediaPlayerImpl::Proxy* proxy);
  virtual ~VideoRendererImpl() {}

  // Determine the conditions to perform fast paint. Returns true if we can do
  // fast paint otherwise false.
  bool CanFastPaint(skia::PlatformCanvas* canvas, const gfx::Rect& dest_rect);

  // Slow paint does a YUV => RGB, and scaled blit in two separate operations.
  void SlowPaint(media::VideoFrame* video_frame,
                 skia::PlatformCanvas* canvas,
                 const gfx::Rect& dest_rect);

  // Fast paint does YUV => RGB, scaling, blitting all in one step into the
  // canvas. It's not always safe and appropriate to perform fast paint.
  // CanFastPaint() is used to determine the conditions.
  void FastPaint(media::VideoFrame* video_frame,
                 skia::PlatformCanvas* canvas,
                 const gfx::Rect& dest_rect);

  void TransformToSkIRect(const SkMatrix& matrix, const gfx::Rect& src_rect,
                          SkIRect* dest_rect);

  // Pointer to our parent object that is called to request repaints.
  scoped_refptr<WebMediaPlayerImpl::Proxy> proxy_;

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

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_VIDEO_RENDERER_IMPL_H_
