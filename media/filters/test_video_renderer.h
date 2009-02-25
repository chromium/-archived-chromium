// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// Simple test class used by unit tests.  Tests create the filter on the test's
// thread and then use InstanceFilterFactory to force the test's instance to
// be returned to the pipeline.

#ifndef MEDIA_FILTERS_TEST_VIDEO_RENDERER_H_
#define MEDIA_FILTERS_TEST_VIDEO_RENDERER_H_

#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/filters/video_renderer_base.h"

namespace media {

class TestVideoRenderer : public VideoRendererBase {
 public:
  TestVideoRenderer()
    : last_frame_(NULL),
      paint_called_(0),
      unique_frames_(0) {
  }

  virtual bool OnInitialize(size_t width, size_t height) { return true; }

  virtual void OnPaintNeeded() {
    ++paint_called_;
    scoped_refptr<VideoFrame> frame;
    GetCurrentFrame(&frame);
    if (frame.get()) {
      VideoSurface video_surface;
      EXPECT_TRUE(frame->Lock(&video_surface));
      frame->Unlock();
      if (frame != last_frame_) {
        ++unique_frames_;
        last_frame_ = frame;
        last_timestamp_ = frame->GetTimestamp();
      }
    }
  }

  size_t unique_frames() { return unique_frames_; }
  size_t paint_called() { return paint_called_; }
  base::TimeDelta last_timestamp() { return last_timestamp_; }

  static bool IsMediaFormatSupported(const MediaFormat* format) {
    return VideoRendererBase::IsMediaFormatSupported(format);
  }

 private:
  friend class scoped_refptr<TestVideoRenderer>;
  virtual ~TestVideoRenderer() {}

  VideoFrame* last_frame_;
  size_t paint_called_;
  size_t unique_frames_;
  base::TimeDelta last_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(TestVideoRenderer);
};

}  // namespace

#endif  // MEDIA_FILTERS_TEST_VIDEO_RENDERER_H_
