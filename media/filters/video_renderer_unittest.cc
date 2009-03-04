// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "media/base/factory.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"
#include "media/base/mock_media_filters.h"
#include "media/base/pipeline_impl.h"
#include "media/filters/test_video_renderer.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::FilterFactoryCollection;
using media::InstanceFilterFactory;
using media::MockAudioDecoder;
using media::MockAudioRenderer;
using media::MockDataSource;
using media::MockDemuxer;
using media::MockFilterConfig;
using media::MockVideoDecoder;
using media::PipelineImpl;
using media::TestVideoRenderer;
using media::VideoFrame;

TEST(VideoRenderer, DISABLED_CreateTestRenderer) {
  base::TimeDelta test_time = base::TimeDelta::FromMilliseconds(500);
  std::string url("");
  PipelineImpl p;
  scoped_refptr<TestVideoRenderer> test_renderer = new TestVideoRenderer();
  MockFilterConfig config;
  scoped_refptr<FilterFactoryCollection> c = new FilterFactoryCollection();
  c->AddFactory(MockDataSource::CreateFactory(&config));
  c->AddFactory(MockDemuxer::CreateFactory(&config));
  c->AddFactory(MockAudioDecoder::CreateFactory(&config));
  c->AddFactory(MockAudioRenderer::CreateFactory(&config));
  c->AddFactory(MockVideoDecoder::CreateFactory(&config));
  c->AddFactory(new InstanceFilterFactory<TestVideoRenderer>(test_renderer));
  media::InitializationHelper h;
  h.Start(&p, c, url);
  h.Wait();
  EXPECT_TRUE(p.IsInitialized());
  p.SetPlaybackRate(1.0f);
  PlatformThread::Sleep(static_cast<int>(test_time.InMilliseconds()));
  p.Stop();
  // Allow a decent amount of variability here.  We expect 15 or 16 frames
  // but for now make sure it's within a reasonable range.
  // TODO(ralphl): This test is now DISABLED because sometimes on linux we
  // only get the first frame.  Investigate why, but for now disabled.
  int64 num_expected_frames = test_time / config.frame_duration;
  EXPECT_GT(test_renderer->unique_frames(), num_expected_frames - 3);
  EXPECT_LT(test_renderer->unique_frames(), num_expected_frames + 3);
}

TEST(VideoRenderer, SingleVideoFrame) {
  base::TimeDelta test_time = base::TimeDelta::FromMilliseconds(100);
  std::string url("");
  PipelineImpl p;
  scoped_refptr<TestVideoRenderer> test_renderer = new TestVideoRenderer();
  MockFilterConfig config;
  config.media_duration = config.frame_duration;
  scoped_refptr<FilterFactoryCollection> c = new FilterFactoryCollection();
  c->AddFactory(MockDataSource::CreateFactory(&config));
  c->AddFactory(MockDemuxer::CreateFactory(&config));
  c->AddFactory(MockAudioDecoder::CreateFactory(&config));
  c->AddFactory(MockAudioRenderer::CreateFactory(&config));
  c->AddFactory(MockVideoDecoder::CreateFactory(&config));
  c->AddFactory(new InstanceFilterFactory<TestVideoRenderer>(test_renderer));
  media::InitializationHelper h;
  h.Start(&p, c, url);
  h.TimedWait(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(p.IsInitialized());
  p.SetPlaybackRate(1.0f);
  PlatformThread::Sleep(static_cast<int>(test_time.InMilliseconds()));
  p.Stop();
  EXPECT_EQ(test_renderer->unique_frames(), 1u);
  EXPECT_EQ(test_renderer->paint_called(), 1u);
}
