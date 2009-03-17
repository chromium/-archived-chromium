// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/buffers.h"
#include "media/base/mock_media_filters.h"
#include "media/base/video_frame_impl.h"
#include "media/base/yuv_convert.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::VideoFrameImpl;
using media::VideoSurface;

namespace {

// Given a |yv12_frame| this method converts the YV12 frame to RGBA and
// makes sure that all the pixels of the RBG frame equal |expect_rgb_color|.
void ExpectFrameColor(media::VideoFrame* yv12_frame, uint32 expect_rgb_color) {
  // On linux and mac builds if you directly compare using EXPECT_EQ and use
  // the VideoSurface::kNumxxxPlanes constants, it generates an error when
  // linking.  These are declared so that we can compare against locals.
  const size_t expect_yuv_planes = VideoSurface::kNumYUVPlanes;
  const size_t expect_rgb_planes = VideoSurface::kNumRGBPlanes;

  VideoSurface yuv_surface;
  EXPECT_TRUE(yv12_frame->Lock(&yuv_surface));
  EXPECT_EQ(yuv_surface.format, VideoSurface::YV12);
  EXPECT_EQ(yuv_surface.planes, expect_yuv_planes);
  EXPECT_EQ(yuv_surface.strides[VideoSurface::kUPlane],
            yuv_surface.strides[VideoSurface::kVPlane]);

  scoped_refptr<media::VideoFrame> rgb_frame;
  media::VideoFrameImpl::CreateFrame(VideoSurface::RGBA,
                                     yuv_surface.width,
                                     yuv_surface.height,
                                     yv12_frame->GetTimestamp(),
                                     yv12_frame->GetDuration(),
                                     &rgb_frame);
  media::VideoSurface rgb_surface;
  EXPECT_TRUE(rgb_frame->Lock(&rgb_surface));
  EXPECT_EQ(rgb_surface.width, yuv_surface.width);
  EXPECT_EQ(rgb_surface.height, yuv_surface.height);
  EXPECT_EQ(rgb_surface.planes, expect_rgb_planes);

  media::ConvertYV12ToRGB32(yuv_surface.data[VideoSurface::kYPlane],
                            yuv_surface.data[VideoSurface::kUPlane],
                            yuv_surface.data[VideoSurface::kVPlane],
                            rgb_surface.data[VideoSurface::kRGBPlane],
                            rgb_surface.width,
                            rgb_surface.height,
                            yuv_surface.strides[VideoSurface::kYPlane],
                            yuv_surface.strides[VideoSurface::kUPlane],
                            rgb_surface.strides[VideoSurface::kRGBPlane]);

  for (size_t row = 0; row < rgb_surface.height; ++row) {
    uint32* rgb_row_data = reinterpret_cast<uint32*>(
        rgb_surface.data[VideoSurface::kRGBPlane] +
        (rgb_surface.strides[VideoSurface::kRGBPlane] * row));
    for (size_t col = 0; col < rgb_surface.width; ++col) {
      EXPECT_EQ(rgb_row_data[col], expect_rgb_color);
    }
  }
  rgb_frame->Unlock();
  yv12_frame->Unlock();
}

}  // namespace


TEST(VideoFrameImpl, Basic) {
  const size_t kWidth = 64;
  const size_t kHeight = 48;
  const base::TimeDelta kTimestampA = base::TimeDelta::FromMicroseconds(1337);
  const base::TimeDelta kDurationA = base::TimeDelta::FromMicroseconds(1667);
  const base::TimeDelta kTimestampB = base::TimeDelta::FromMicroseconds(1234);
  const base::TimeDelta kDurationB = base::TimeDelta::FromMicroseconds(5678);

  // Create a YV12 Video Frame.
  scoped_refptr<media::VideoFrame> frame;
  media::VideoFrameImpl::CreateFrame(media::VideoSurface::YV12, kWidth, kHeight,
                                     kTimestampA, kDurationA, &frame);
  ASSERT_TRUE(frame);

  // Test StreamSample implementation.
  EXPECT_TRUE(kTimestampA == frame->GetTimestamp());
  EXPECT_TRUE(kDurationA == frame->GetDuration());
  EXPECT_FALSE(frame->IsEndOfStream());
  EXPECT_FALSE(frame->IsDiscontinuous());
  frame->SetTimestamp(kTimestampB);
  frame->SetDuration(kDurationB);
  EXPECT_TRUE(kTimestampB == frame->GetTimestamp());
  EXPECT_TRUE(kDurationB == frame->GetDuration());
  frame->SetEndOfStream(true);
  EXPECT_TRUE(frame->IsEndOfStream());
  frame->SetEndOfStream(false);
  EXPECT_FALSE(frame->IsEndOfStream());
  frame->SetDiscontinuous(true);
  EXPECT_TRUE(frame->IsDiscontinuous());
  frame->SetDiscontinuous(false);
  EXPECT_FALSE(frame->IsDiscontinuous());

  // Test VideoFrame implementation.
  media::MockVideoDecoder::InitializeYV12Frame(frame, 0.0f);
  ExpectFrameColor(frame, 0xFF000000);
  media::MockVideoDecoder::InitializeYV12Frame(frame, 1.0f);
  ExpectFrameColor(frame, 0xFFFFFFFF);
}
