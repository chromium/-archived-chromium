// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_impl.h"

namespace media {

// static
void VideoFrameImpl::CreateFrame(VideoSurface::Format format,
                                 size_t width,
                                 size_t height,
                                 base::TimeDelta timestamp,
                                 base::TimeDelta duration,
                                 scoped_refptr<VideoFrame>* frame_out) {
  DCHECK(width > 0 && height > 0);
  DCHECK(width * height < 100000000);
  DCHECK(frame_out);
  bool alloc_worked = false;
  scoped_refptr<VideoFrameImpl> frame =
      new VideoFrameImpl(format, width, height);
  if (frame) {
    frame->SetTimestamp(timestamp);
    frame->SetDuration(duration);
    switch (format) {
      case VideoSurface::RGB555:
      case VideoSurface::RGB565:
        alloc_worked = frame->AllocateRGB(2u);
        break;
      case VideoSurface::RGB24:
        alloc_worked = frame->AllocateRGB(3u);
        break;
      case VideoSurface::RGB32:
      case VideoSurface::RGBA:
        alloc_worked = frame->AllocateRGB(4u);
        break;
      case VideoSurface::YV12:
      case VideoSurface::YV16:
        alloc_worked = frame->AllocateYUV();
        break;
      default:
        NOTREACHED();
        alloc_worked = false;
        break;
    }
  }
  *frame_out = alloc_worked ? frame : NULL;
}

// static
void VideoFrameImpl::CreateEmptyFrame(scoped_refptr<VideoFrame>* frame_out) {
  *frame_out = new VideoFrameImpl(VideoSurface::EMPTY, 0, 0);
}

static inline size_t RoundUp(size_t value, size_t alignment) {
  // Check that |alignment| is a power of 2.
  DCHECK((alignment + (alignment - 1)) == (alignment | (alignment - 1)));
  return ((value + (alignment - 1)) & ~(alignment-1));
}

bool VideoFrameImpl::AllocateRGB(size_t bytes_per_pixel) {
  // Round up to align at a 64-bit (8 byte) boundary for each row.  This
  // is sufficient for MMX reads (movq).
  size_t bytes_per_row = RoundUp(surface_.width * bytes_per_pixel, 8);
  surface_.planes = VideoSurface::kNumRGBPlanes;
  surface_.strides[VideoSurface::kRGBPlane] = bytes_per_row;
  surface_.data[VideoSurface::kRGBPlane] = new uint8[bytes_per_row *
                                                     surface_.height];
  DCHECK(surface_.data[VideoSurface::kRGBPlane]);
  DCHECK(!(reinterpret_cast<int>(surface_.data[VideoSurface::kRGBPlane]) & 7));
  COMPILE_ASSERT(0 == VideoSurface::kRGBPlane, RGB_data_must_be_index_0);
  return (NULL != surface_.data[VideoSurface::kRGBPlane]);
}

bool VideoFrameImpl::AllocateYUV() {
  DCHECK(surface_.format == VideoSurface::YV12 ||
         surface_.format == VideoSurface::YV16);
  // Align Y rows at 32-bit (4 byte) boundaries.  The stride for both YV12 and
  // YV16 is 1/2 of the stride of Y.  For YV12, every row of bytes for U and V
  // applies to two rows of Y (one byte of UV for 4 bytes of Y), so in the
  // case of YV12 the strides are identical for the same width surface, but the
  // number of bytes allocated for YV12 is 1/2 the amount for U & V as YV16.
  // We also round the height of the surface allocated to be an even number
  // to avoid any potential of faulting by code that attempts to access the Y
  // values of the final row, but assumes that the last row of U & V applies to
  // a full two rows of Y.
  size_t alloc_height = RoundUp(surface_.height, 2);
  size_t y_bytes_per_row = RoundUp(surface_.width, 4);
  size_t uv_stride = RoundUp(y_bytes_per_row / 2, 4);
  size_t y_bytes = alloc_height * y_bytes_per_row;
  size_t uv_bytes = alloc_height * uv_stride;
  if (surface_.format == VideoSurface::YV12) {
    uv_bytes /= 2;
  }
  uint8* data = new uint8[y_bytes + (uv_bytes * 2)];
  if (data) {
    surface_.planes = VideoSurface::kNumYUVPlanes;
    COMPILE_ASSERT(0 == VideoSurface::kYPlane, y_plane_data_must_be_index_0);
    surface_.data[VideoSurface::kYPlane] = data;
    surface_.data[VideoSurface::kUPlane] = data + y_bytes;
    surface_.data[VideoSurface::kVPlane] = data + y_bytes + uv_bytes;
    surface_.strides[VideoSurface::kYPlane] = y_bytes_per_row;
    surface_.strides[VideoSurface::kUPlane] = uv_stride;
    surface_.strides[VideoSurface::kVPlane] = uv_stride;
    return true;
  }
  NOTREACHED();
  return false;
}

VideoFrameImpl::VideoFrameImpl(VideoSurface::Format format,
                               size_t width,
                               size_t height) {
  locked_ = false;
  memset(&surface_, 0, sizeof(surface_));
  surface_.format = format;
  surface_.width = width;
  surface_.height = height;
}

VideoFrameImpl::~VideoFrameImpl() {
  // In multi-plane allocations, only a single block of memory is allocated
  // on the heap, and other |data| pointers point inside the same, single block
  // so just delete index 0.
  delete[] surface_.data[0];
}

bool VideoFrameImpl::Lock(VideoSurface* surface) {
  DCHECK(!locked_);
  DCHECK_NE(surface_.format, VideoSurface::EMPTY);
  if (locked_) {
    memset(surface, 0, sizeof(*surface));
    return false;
  }
  locked_ = true;
  COMPILE_ASSERT(sizeof(*surface) == sizeof(surface_), surface_size_mismatch);
  memcpy(surface, &surface_, sizeof(*surface));
  return true;
}

void VideoFrameImpl::Unlock() {
  DCHECK(locked_);
  DCHECK_NE(surface_.format, VideoSurface::EMPTY);
  locked_ = false;
}

bool VideoFrameImpl::IsEndOfStream() const {
  return surface_.format == VideoSurface::EMPTY;
}

}  // namespace media
