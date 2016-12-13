// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/base/buffers.h"
#include "media/base/yuv_convert.h"
#include "webkit/glue/media/video_renderer_impl.h"
#include "webkit/glue/webmediaplayer_impl.h"

namespace webkit_glue {

  VideoRendererImpl::VideoRendererImpl(WebMediaPlayerImpl::Proxy* proxy)
    : proxy_(proxy),
      last_converted_frame_(NULL) {
  // TODO(hclam): decide whether to do the following line in this thread or
  // in the render thread.
  proxy->SetVideoRenderer(this);
}

// static
bool VideoRendererImpl::IsMediaFormatSupported(
    const media::MediaFormat& media_format) {
  int width = 0;
  int height = 0;
  return ParseMediaFormat(media_format, &width, &height);
}

bool VideoRendererImpl::OnInitialize(media::VideoDecoder* decoder) {
  int width = 0;
  int height = 0;
  if (!ParseMediaFormat(decoder->media_format(), &width, &height))
    return false;

  video_size_.SetSize(width, height);
  bitmap_.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  if (bitmap_.allocPixels(NULL, NULL)) {
    bitmap_.eraseRGB(0x00, 0x00, 0x00);
    return true;
  }

  NOTREACHED();
  return false;
}

void VideoRendererImpl::OnStop() {
  DCHECK(proxy_);
  proxy_->SetVideoRenderer(NULL);
  proxy_ = NULL;
}

void VideoRendererImpl::OnFrameAvailable() {
  DCHECK(proxy_);
  proxy_->Repaint();
}

void VideoRendererImpl::SetRect(const gfx::Rect& rect) {
}

// This method is always called on the renderer's thread.
void VideoRendererImpl::Paint(skia::PlatformCanvas* canvas,
                              const gfx::Rect& dest_rect) {
  scoped_refptr<media::VideoFrame> video_frame;
  GetCurrentFrame(&video_frame);
  if (video_frame) {
    if (CanFastPaint(canvas, dest_rect)) {
      FastPaint(video_frame, canvas, dest_rect);
    } else {
      SlowPaint(video_frame, canvas, dest_rect);
    }
    video_frame = NULL;
  }
}

// CanFastPaint is a helper method to determine the conditions for fast
// painting. The conditions are:
// 1. No skew in canvas matrix.
// 2. No flipping nor mirroring.
// 3. Canvas has pixel format ARGB8888.
// 4. Canvas is opaque.
// TODO(hclam): The fast paint method should support flipping and mirroring.
// Disable the flipping and mirroring checks once we have it.
bool VideoRendererImpl::CanFastPaint(skia::PlatformCanvas* canvas,
                                     const gfx::Rect& dest_rect) {
  const SkMatrix& total_matrix = canvas->getTotalMatrix();
  // Perform the following checks here:
  // 1. Check for skewing factors of the transformation matrix. They should be
  //    zero.
  // 2. Check for mirroring and flipping. Make sure they are greater than zero.
  if (SkScalarNearlyZero(total_matrix.getSkewX()) &&
      SkScalarNearlyZero(total_matrix.getSkewY()) &&
      total_matrix.getScaleX() > 0 &&
      total_matrix.getScaleY() > 0) {
    // Get the properties of the SkDevice and the clip rect.
    SkDevice* device = canvas->getDevice();

    // Get the boundary of the device.
    SkIRect device_rect;
    device->getBounds(&device_rect);

    // Get the pixel config of the device.
    const SkBitmap::Config config = device->config();
    // Get the total clip rect associated with the canvas.
    const SkRegion& total_clip = canvas->getTotalClip();

    SkIRect dest_irect;
    TransformToSkIRect(canvas->getTotalMatrix(), dest_rect, &dest_irect);

    if (config == SkBitmap::kARGB_8888_Config && device->isOpaque() &&
        device_rect.contains(total_clip.getBounds())) {
      return true;
    }
  }
  return false;
}

void VideoRendererImpl::SlowPaint(media::VideoFrame* video_frame,
                                  skia::PlatformCanvas* canvas,
                                  const gfx::Rect& dest_rect) {
  // 1. Convert YUV frame to RGB.
  base::TimeDelta timestamp = video_frame->GetTimestamp();
  if (video_frame != last_converted_frame_ ||
      timestamp != last_converted_timestamp_) {
    last_converted_frame_ = video_frame;
    last_converted_timestamp_ = timestamp;
    media::VideoSurface frame_in;
    if (video_frame->Lock(&frame_in)) {
      DCHECK(frame_in.format == media::VideoSurface::YV12 ||
             frame_in.format == media::VideoSurface::YV16);
      DCHECK(frame_in.strides[media::VideoSurface::kUPlane] ==
             frame_in.strides[media::VideoSurface::kVPlane]);
      DCHECK(frame_in.planes == media::VideoSurface::kNumYUVPlanes);
      bitmap_.lockPixels();
      media::YUVType yuv_type = (frame_in.format == media::VideoSurface::YV12) ?
                                media::YV12 : media::YV16;
      media::ConvertYUVToRGB32(frame_in.data[media::VideoSurface::kYPlane],
                               frame_in.data[media::VideoSurface::kUPlane],
                               frame_in.data[media::VideoSurface::kVPlane],
                               static_cast<uint8*>(bitmap_.getPixels()),
                               frame_in.width,
                               frame_in.height,
                               frame_in.strides[media::VideoSurface::kYPlane],
                               frame_in.strides[media::VideoSurface::kUPlane],
                               bitmap_.rowBytes(),
                               yuv_type);
      bitmap_.unlockPixels();
      video_frame->Unlock();
    } else {
      NOTREACHED();
    }
  }

  // 2. Paint the bitmap to canvas.
  SkMatrix matrix;
  matrix.setTranslate(static_cast<SkScalar>(dest_rect.x()),
                      static_cast<SkScalar>(dest_rect.y()));
  if (dest_rect.width()  != video_size_.width() ||
      dest_rect.height() != video_size_.height()) {
    matrix.preScale(SkIntToScalar(dest_rect.width()) /
                    SkIntToScalar(video_size_.width()),
                    SkIntToScalar(dest_rect.height()) /
                    SkIntToScalar(video_size_.height()));
  }
  canvas->drawBitmapMatrix(bitmap_, matrix, NULL);
}

void VideoRendererImpl::FastPaint(media::VideoFrame* video_frame,
                                  skia::PlatformCanvas* canvas,
                                  const gfx::Rect& dest_rect) {
  media::VideoSurface frame_in;
  if (video_frame->Lock(&frame_in)) {
    DCHECK(frame_in.format == media::VideoSurface::YV12 ||
           frame_in.format == media::VideoSurface::YV16);
    DCHECK(frame_in.strides[media::VideoSurface::kUPlane] ==
           frame_in.strides[media::VideoSurface::kVPlane]);
    DCHECK(frame_in.planes == media::VideoSurface::kNumYUVPlanes);
    const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(true);
    media::YUVType yuv_type = (frame_in.format == media::VideoSurface::YV12) ?
                              media::YV12 : media::YV16;
    int y_shift = yuv_type;  // 1 for YV12, 0 for YV16.

    // Create a rectangle backed by SkScalar.
    SkRect scalar_dest_rect;
    scalar_dest_rect.iset(dest_rect.x(), dest_rect.y(),
                          dest_rect.right(), dest_rect.bottom());

    // Transform the destination rectangle to local coordinates.
    const SkMatrix& local_matrix = canvas->getTotalMatrix();
    SkRect local_dest_rect;
    local_matrix.mapRect(&local_dest_rect, scalar_dest_rect);

    // After projecting the destination rectangle to local coordinates, round
    // the projected rectangle to integer values, this will give us pixel values
    // of the rectangle.
    SkIRect local_dest_irect, local_dest_irect_saved;
    local_dest_rect.round(&local_dest_irect);
    local_dest_rect.round(&local_dest_irect_saved);

    // Only does the paint if the destination rect intersects with the clip
    // rect.
    if (local_dest_irect.intersect(canvas->getTotalClip().getBounds())) {
      // At this point |local_dest_irect| contains the rect that we should draw
      // to within the clipping rect.

      // Calculate the address for the top left corner of destination rect in
      // the canvas that we will draw to. The address is obtained by the base
      // address of the canvas shifted by "left" and "top" of the rect.
      uint8* dest_rect_pointer = static_cast<uint8*>(bitmap.getPixels()) +
                                 local_dest_irect.fTop * bitmap.rowBytes() +
                                 local_dest_irect.fLeft * 4;

      // Project the clip rect to the original video frame, obtains the
      // dimensions of the projected clip rect, "left" and "top" of the rect.
      // The math here are all integer math so we won't have rounding error and
      // write outside of the canvas.
      // We have the assumptions of dest_rect.width() and dest_rect.height()
      // being non-zero, these are valid assumptions since finding intersection
      // above rejects empty rectangle so we just do a DCHECK here.
      DCHECK_NE(0, dest_rect.width());
      DCHECK_NE(0, dest_rect.height());
      size_t frame_clip_width = local_dest_irect.width() *
                                frame_in.width /
                                local_dest_irect_saved.width();
      size_t frame_clip_height = local_dest_irect.height() *
                                 frame_in.height /
                                 local_dest_irect_saved.height();

      // Project the "left" and "top" of the final destination rect to local
      // coordinates of the video frame, use these values to find the offsets
      // in the video frame to start reading.
      size_t frame_clip_left = (local_dest_irect.fLeft -
                                local_dest_irect_saved.fLeft) *
                               frame_in.width /
                               local_dest_irect_saved.width();
      size_t frame_clip_top = (local_dest_irect.fTop -
                               local_dest_irect_saved.fTop) *
                              frame_in.height /
                              local_dest_irect_saved.height();

      // Use the "left" and "top" of the destination rect to locate the offset
      // in Y, U and V planes.
      size_t y_offset = frame_in.strides[media::VideoSurface::kYPlane] *
                        frame_clip_top + frame_clip_left;
      // For format YV12, there is one U, V value per 2x2 block.
      // For format YV16, there is one u, V value per 2x1 block.
      size_t uv_offset = (frame_in.strides[media::VideoSurface::kUPlane] *
                         (frame_clip_top >> y_shift)) +
                         (frame_clip_left >> 1);
      uint8* frame_clip_y = frame_in.data[media::VideoSurface::kYPlane] +
                            y_offset;
      uint8* frame_clip_u = frame_in.data[media::VideoSurface::kUPlane] +
                            uv_offset;
      uint8* frame_clip_v = frame_in.data[media::VideoSurface::kVPlane] +
                            uv_offset;
      bitmap.lockPixels();

      // TODO(hclam): do rotation and mirroring here.
      media::ScaleYUVToRGB32(frame_clip_y,
                             frame_clip_u,
                             frame_clip_v,
                             dest_rect_pointer,
                             frame_clip_width,
                             frame_clip_height,
                             local_dest_irect.width(),
                             local_dest_irect.height(),
                             frame_in.strides[media::VideoSurface::kYPlane],
                             frame_in.strides[media::VideoSurface::kUPlane],
                             bitmap.rowBytes(),
                             yuv_type,
                             media::ROTATE_0);
      bitmap.unlockPixels();
    }
    video_frame->Unlock();
  } else {
    NOTREACHED();
  }
}

void VideoRendererImpl::TransformToSkIRect(const SkMatrix& matrix,
                                           const gfx::Rect& src_rect,
                                           SkIRect* dest_rect) {
    // Transform destination rect to local coordinates.
    SkRect transformed_rect;
    SkRect skia_dest_rect;
    skia_dest_rect.iset(src_rect.x(), src_rect.y(),
                        src_rect.right(), src_rect.bottom());
    matrix.mapRect(&transformed_rect, skia_dest_rect);
    transformed_rect.round(dest_rect);
}

}  // namespace webkit_glue
