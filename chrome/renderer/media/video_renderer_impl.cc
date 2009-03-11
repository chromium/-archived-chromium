// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/media/video_renderer_impl.h"
#include "media/base/yuv_convert.h"

VideoRendererImpl::VideoRendererImpl(WebMediaPlayerDelegateImpl* delegate)
    : delegate_(delegate),
      last_converted_frame_(NULL) {
}

bool VideoRendererImpl::OnInitialize(size_t width, size_t height) {
  video_size_.SetSize(width, height);
  bitmap_.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  if (bitmap_.allocPixels(NULL, NULL)) {
    bitmap_.eraseRGB(0x00, 0x00, 0x00);
    return true;
  }
  NOTREACHED();
  return false;
}

void VideoRendererImpl::OnPaintNeeded() {
  delegate_->PostRepaintTask();
}

// This method is always called on the renderer's thread.
void VideoRendererImpl::Paint(skia::PlatformCanvas* canvas,
                              const gfx::Rect& dest_rect) {
  scoped_refptr<media::VideoFrame> video_frame;
  GetCurrentFrame(&video_frame);
  if (video_frame.get()) {
    CopyToCurrentFrame(video_frame);
    video_frame = NULL;
  }
  SkMatrix matrix;
  matrix.setTranslate(static_cast<SkScalar>(dest_rect.x()),
                      static_cast<SkScalar>(dest_rect.y()));
  if (dest_rect.width()  != video_size_.width() ||
      dest_rect.height() != video_size_.height()) {
    matrix.preScale(
        static_cast<SkScalar>(dest_rect.width()  / video_size_.width()),
        static_cast<SkScalar>(dest_rect.height() / video_size_.height()));
  }
  canvas->drawBitmapMatrix(bitmap_, matrix, NULL);
}

void VideoRendererImpl::CopyToCurrentFrame(media::VideoFrame* video_frame) {
  base::TimeDelta timestamp = video_frame->GetTimestamp();
  if (video_frame != last_converted_frame_ ||
      timestamp != last_converted_timestamp_) {
    last_converted_frame_ = video_frame;
    last_converted_timestamp_ = timestamp;
    media::VideoSurface frame_in;
    if (video_frame->Lock(&frame_in)) {
      // TODO(hclam): Support more video formats than just YV12.
      DCHECK(frame_in.format == media::VideoSurface::YV12);
      DCHECK(frame_in.strides[media::VideoSurface::kUPlane] ==
             frame_in.strides[media::VideoSurface::kVPlane]);
      DCHECK(frame_in.planes == media::VideoSurface::kNumYUVPlanes);
      bitmap_.lockPixels();
      media::ConvertYV12ToRGB32(frame_in.data[media::VideoSurface::kYPlane],
                                frame_in.data[media::VideoSurface::kUPlane],
                                frame_in.data[media::VideoSurface::kVPlane],
                                static_cast<uint8*>(bitmap_.getPixels()),
                                frame_in.width,
                                frame_in.height,
                                frame_in.strides[media::VideoSurface::kYPlane],
                                frame_in.strides[media::VideoSurface::kUPlane],
                                bitmap_.rowBytes());
      bitmap_.unlockPixels();
      video_frame->Unlock();
    } else {
      NOTREACHED();
    }
  }
}
