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
using media::PipelineImpl;

TEST(PipelineImplTest, Initialization) {
  std::string u("");
  PipelineImpl p;
  InitializationHelper h;
  h.Start(&p, MockDataSource::CreateFactory(media::MOCK_FILTER_NEVER_INIT), u);
  h.TimedWait(base::TimeDelta::FromMilliseconds(300));
  EXPECT_TRUE(h.waiting_for_callback());
  EXPECT_FALSE(p.IsInitialized());
  EXPECT_TRUE(media::PIPELINE_OK == p.GetError());
  p.Stop();
  EXPECT_FALSE(h.waiting_for_callback());
  EXPECT_FALSE(h.callback_success_status());
  EXPECT_TRUE(media::PIPELINE_OK == p.GetError());

  h.Start(&p, MockDataSource::CreateFactory(media::MOCK_FILTER_TASK_INIT), u);
  h.TimedWait(base::TimeDelta::FromSeconds(5));
  EXPECT_FALSE(h.waiting_for_callback());
  EXPECT_FALSE(h.callback_success_status());
  EXPECT_FALSE(p.IsInitialized());
  EXPECT_FALSE(media::PIPELINE_OK == p.GetError());
  p.Stop();
}

TEST(PipelineImplTest, FullMockPipeline) {
  std::string url("");
  PipelineImpl p;
  scoped_refptr<FilterFactoryCollection> c = new FilterFactoryCollection();
  c->AddFactory(MockDataSource::CreateFactory(media::MOCK_FILTER_NORMAL_INIT));
  c->AddFactory(MockDemuxer::CreateFactory());
  c->AddFactory(MockAudioDecoder::CreateFactory());
  c->AddFactory(MockAudioRenderer::CreateFactory());
  InitializationHelper h;
  h.Start(&p, c, url);
  h.TimedWait(base::TimeDelta::FromSeconds(5));
  EXPECT_FALSE(h.waiting_for_callback());
  EXPECT_TRUE(h.callback_success_status());
  EXPECT_TRUE(p.IsInitialized());
  EXPECT_TRUE(media::PIPELINE_OK == p.GetError());
  p.SetPlaybackRate(1.0f);
  p.SetVolume(0.5f);
  p.Stop();
  EXPECT_FALSE(p.IsInitialized());
}
