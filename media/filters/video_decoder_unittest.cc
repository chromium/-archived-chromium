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
#include "media/filters/test_video_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::FilterFactoryCollection;
using media::InstanceFilterFactory;
using media::MockAudioDecoder;
using media::MockAudioRenderer;
using media::MockDataSource;
using media::MockDemuxer;
using media::MockFilterConfig;
using media::MockVideoRenderer;
using media::PipelineImpl;
using media::TestVideoDecoder;
using media::VideoFrame;

TEST(VideoDecoder, CreateTestDecoder) {
  std::string url("");
  PipelineImpl p;
  scoped_refptr<TestVideoDecoder> test_decoder = new TestVideoDecoder();
  MockFilterConfig config;
  scoped_refptr<FilterFactoryCollection> c = new FilterFactoryCollection();
  c->AddFactory(MockDataSource::CreateFactory(&config));
  c->AddFactory(MockDemuxer::CreateFactory(&config));
  c->AddFactory(new InstanceFilterFactory<TestVideoDecoder>(test_decoder));
  c->AddFactory(MockAudioDecoder::CreateFactory(&config));
  c->AddFactory(MockAudioRenderer::CreateFactory(&config));
  c->AddFactory(MockVideoRenderer::CreateFactory(&config));
  media::InitializationHelper h;
  h.Start(&p, c, url);
  p.SetPlaybackRate(1.0f);
  p.Stop();
}

