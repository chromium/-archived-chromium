// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/platform_thread.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "media/base/pipeline_impl.h"
#include "media/base/media_format.h"
#include "media/base/filters.h"
#include "media/base/factory.h"
#include "media/base/filter_host.h"
#include "media/base/mock_media_filters.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::FilterFactory;
using media::FilterFactoryCollection;
using media::FilterHost;
using media::InitializationHelper;
using media::MediaFormat;
using media::MockAudioDecoder;
using media::MockAudioRenderer;
using media::MockDataSource;
using media::MockDemuxer;
using media::MockFilterConfig;
using media::MockVideoDecoder;
using media::MockVideoRenderer;
using media::PipelineImpl;

namespace {

void AddAllMockFilters(FilterFactoryCollection* factories,
                       const MockFilterConfig* config) {
  factories->AddFactory(MockDataSource::CreateFactory(config));
  factories->AddFactory(MockDemuxer::CreateFactory(config));
  factories->AddFactory(MockAudioDecoder::CreateFactory(config));
  factories->AddFactory(MockAudioRenderer::CreateFactory(config));
  factories->AddFactory(MockVideoDecoder::CreateFactory(config));
  factories->AddFactory(MockVideoRenderer::CreateFactory(config));
}

}  // namespace

// TODO(ralphl): Get rid of single character variable names in these tests.
TEST(PipelineImplTest, Initialization) {
  std::string u("");
  PipelineImpl p;
  InitializationHelper h;
  MockFilterConfig config;
  config.data_source_behavior = media::MOCK_DATA_SOURCE_NEVER_INIT;
  h.Start(&p, MockDataSource::CreateFactory(&config), u);
  h.TimedWait(base::TimeDelta::FromMilliseconds(300));
  EXPECT_TRUE(h.waiting_for_callback());
  EXPECT_FALSE(p.IsInitialized());
  EXPECT_TRUE(media::PIPELINE_OK == p.GetError());
  p.Stop();
  EXPECT_FALSE(h.waiting_for_callback());
  EXPECT_FALSE(h.callback_success_status());
  EXPECT_TRUE(media::PIPELINE_OK == p.GetError());

  config.data_source_behavior = media::MOCK_DATA_SOURCE_TASK_INIT;
  h.Start(&p, MockDataSource::CreateFactory(&config), u);
  h.TimedWait(base::TimeDelta::FromSeconds(5));
  EXPECT_FALSE(h.waiting_for_callback());
  EXPECT_FALSE(h.callback_success_status());
  EXPECT_FALSE(p.IsInitialized());
  EXPECT_FALSE(media::PIPELINE_OK == p.GetError());
  p.Stop();

  config.data_source_behavior = media::MOCK_DATA_SOURCE_ERROR_IN_INIT;
  h.Start(&p, MockDataSource::CreateFactory(&config), u);
  h.TimedWait(base::TimeDelta::FromSeconds(5));
  EXPECT_FALSE(h.waiting_for_callback());
  EXPECT_FALSE(h.callback_success_status());
  EXPECT_FALSE(p.IsInitialized());
  EXPECT_FALSE(media::PIPELINE_OK == p.GetError());
  p.Stop();
}

TEST(PipelineImplTest, MockAudioPipeline) {
  std::string url("");
  PipelineImpl p;
  MockFilterConfig config;
  config.has_video = false;
  scoped_refptr<FilterFactoryCollection> c = new FilterFactoryCollection();
  AddAllMockFilters(c, &config);
  InitializationHelper h;
  h.Start(&p, c, url);
  h.TimedWait(base::TimeDelta::FromSeconds(5));
  EXPECT_FALSE(h.waiting_for_callback());
  EXPECT_TRUE(h.callback_success_status());
  EXPECT_TRUE(p.IsInitialized());
  EXPECT_TRUE(media::PIPELINE_OK == p.GetError());
  size_t width, height;
  p.GetVideoSize(&width, &height);
  EXPECT_EQ(0u, width);
  EXPECT_EQ(0u, height);
  p.SetPlaybackRate(1.0f);
  p.SetVolume(0.5f);
  p.Stop();
  EXPECT_FALSE(p.IsInitialized());
}

TEST(PipelineImplTest, MockVideoPipeline) {
  std::string url("");
  PipelineImpl p;
  scoped_refptr<FilterFactoryCollection> c = new FilterFactoryCollection();
  MockFilterConfig config;
  AddAllMockFilters(c, &config);
  InitializationHelper h;
  h.Start(&p, c, url);
  h.TimedWait(base::TimeDelta::FromSeconds(5));
  EXPECT_FALSE(h.waiting_for_callback());
  EXPECT_TRUE(h.callback_success_status());
  EXPECT_TRUE(p.IsInitialized());
  EXPECT_TRUE(media::PIPELINE_OK == p.GetError());
  size_t width, height;
  p.GetVideoSize(&width, &height);
  EXPECT_EQ(config.video_width, width);
  EXPECT_EQ(config.video_height, height);
  p.SetPlaybackRate(1.0f);
  p.SetVolume(0.5f);
  p.Stop();
  EXPECT_FALSE(p.IsInitialized());
}

