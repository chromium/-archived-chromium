// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/base/mock_media_filters.h"
#include "media/base/mock_filter_host.h"
#include "media/filters/test_video_renderer.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::FilterFactory;
using media::MockFilterConfig;
using media::MockFilterHost;
using media::MockPipeline;
using media::MockVideoDecoder;
using media::TestVideoRenderer;

TEST(VideoRenderer, CreateAndPlay) {
  // Prepare test data.
  MockFilterConfig config;
  scoped_refptr<FilterFactory> factory
      = MockVideoDecoder::CreateFactory(&config);
  scoped_refptr<MockVideoDecoder> decoder
      = factory->Create<MockVideoDecoder>(NULL);
  scoped_refptr<TestVideoRenderer> renderer = new TestVideoRenderer();

  // Setup our mock pipeline.
  MockPipeline pipeline;
  MockFilterHost<MockVideoDecoder> filter_host_a(&pipeline, decoder);
  MockFilterHost<TestVideoRenderer> filter_host_b(&pipeline, renderer);

  // Initialize the video renderer and run pending tasks.  It should set its
  // time update callback and scheduled its first callback time.
  EXPECT_TRUE(renderer->Initialize(decoder));
  EXPECT_FALSE(filter_host_b.IsInitialized());
  pipeline.RunAllTasks();
  EXPECT_TRUE(filter_host_b.IsInitialized());
  EXPECT_TRUE(filter_host_b.GetTimeUpdateCallback());
  EXPECT_NE(0, filter_host_b.GetScheduledCallbackTime().InMicroseconds());

  // We also expect one unique frame due to the preroll paint.
  EXPECT_EQ(1u, renderer->unique_frames());

  // Now lets simulate playing 10 frames...
  for (int i = 0; i < 10; ++i) {
    base::TimeDelta previous_time = filter_host_b.GetScheduledCallbackTime();
    size_t previous_unique_frames = renderer->unique_frames();

    // Advance time to the callback time and execute.
    pipeline.SetTime(previous_time);
    filter_host_b.GetTimeUpdateCallback()->Run(previous_time);
    pipeline.RunAllTasks();

    // Renderer should have scheduled a new callback time and painted a frame.
    EXPECT_GT(filter_host_b.GetScheduledCallbackTime().InMicroseconds(),
              previous_time.InMicroseconds());
    EXPECT_EQ(previous_unique_frames + 1, renderer->unique_frames());
  }
}
