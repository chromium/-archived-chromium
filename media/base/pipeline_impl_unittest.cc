// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/waitable_event.h"
#include "media/base/pipeline_impl.h"
#include "media/base/media_format.h"
#include "media/base/filters.h"
#include "media/base/factory.h"
#include "media/base/filter_host.h"
#include "media/base/mock_media_filters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PipelineImplTest : public testing::Test {
 protected:
  PipelineImplTest()
      : initialize_result_(false),
        seek_result_(false),
        initialize_event_(false, false),
        seek_event_(false, false) {
  }

  virtual ~PipelineImplTest() {}

  virtual void TearDown() {
    // Force the pipeline to shut down its thread.
    pipeline_.Stop();
  }

  // Called by tests after they have finished setting up MockFilterConfig.
  // Initializes the pipeline and returns true if the initialization callback
  // was executed, false otherwise.
  bool InitializeAndWait() {
    DCHECK(!filters_);
    filters_ = new media::old_mocks::MockFilterFactory(&config_);
    pipeline_.Start(filters_, "",
                    NewCallback(this, &PipelineImplTest::OnInitialize));
    return initialize_event_.TimedWait(base::TimeDelta::FromMilliseconds(500));
  }

  // Issues a seek on the pipeline and returns true if the seek callback was
  // executed, false otherwise.
  bool SeekAndWait(const base::TimeDelta& time) {
    pipeline_.Seek(time, NewCallback(this, &PipelineImplTest::OnSeek));
    return seek_event_.TimedWait(base::TimeDelta::FromMilliseconds(500));
  }

  // Fixture members.
  media::PipelineImpl pipeline_;
  scoped_refptr<media::old_mocks::MockFilterFactory> filters_;
  media::old_mocks::MockFilterConfig config_;
  bool initialize_result_;
  bool seek_result_;

 private:
  void OnInitialize(bool result) {
    initialize_result_ = result;
    initialize_event_.Signal();
  }

  void OnSeek(bool result) {
    seek_result_ = result;
    seek_event_.Signal();
  }

  // Used to wait for callbacks.
  base::WaitableEvent initialize_event_;
  base::WaitableEvent seek_event_;

  DISALLOW_COPY_AND_ASSIGN(PipelineImplTest);
};

TEST_F(PipelineImplTest, NeverInitializes) {
  config_.data_source_behavior = media::old_mocks::MOCK_DATA_SOURCE_NEVER_INIT;

  // This test hangs during initialization by never calling
  // InitializationComplete().  Make sure we tear down the pipeline properly.
  ASSERT_FALSE(InitializeAndWait());
  EXPECT_FALSE(initialize_result_);
  EXPECT_FALSE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_OK, pipeline_.GetError());
}

TEST_F(PipelineImplTest, RequiredFilterMissing) {
  config_.create_filter = false;

  ASSERT_TRUE(InitializeAndWait());
  EXPECT_FALSE(initialize_result_);
  EXPECT_FALSE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_ERROR_REQUIRED_FILTER_MISSING,
            pipeline_.GetError());
}

TEST_F(PipelineImplTest, URLNotFound) {
  config_.data_source_behavior =
      media::old_mocks::MOCK_DATA_SOURCE_URL_ERROR_IN_INIT;

  ASSERT_TRUE(InitializeAndWait());
  EXPECT_FALSE(initialize_result_);
  EXPECT_FALSE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_ERROR_URL_NOT_FOUND, pipeline_.GetError());
}

TEST_F(PipelineImplTest, NoStreams) {
  config_.has_audio = false;
  config_.has_video = false;

  ASSERT_TRUE(InitializeAndWait());
  EXPECT_FALSE(initialize_result_);
  EXPECT_FALSE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_ERROR_COULD_NOT_RENDER, pipeline_.GetError());

  EXPECT_FALSE(filters_->audio_decoder());
  EXPECT_FALSE(filters_->audio_renderer());
  EXPECT_FALSE(filters_->video_decoder());
  EXPECT_FALSE(filters_->video_renderer());
}

TEST_F(PipelineImplTest, AudioStream) {
  config_.has_video = false;

  ASSERT_TRUE(InitializeAndWait());
  EXPECT_TRUE(initialize_result_);
  EXPECT_TRUE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_OK, pipeline_.GetError());

  size_t width, height;
  pipeline_.GetVideoSize(&width, &height);
  EXPECT_EQ(0u, width);
  EXPECT_EQ(0u, height);
  EXPECT_TRUE(pipeline_.IsRendered(media::mime_type::kMajorTypeAudio));
  EXPECT_FALSE(pipeline_.IsRendered(media::mime_type::kMajorTypeVideo));

  EXPECT_TRUE(filters_->audio_decoder());
  EXPECT_TRUE(filters_->audio_renderer());
  EXPECT_FALSE(filters_->video_decoder());
  EXPECT_FALSE(filters_->video_renderer());
}

TEST_F(PipelineImplTest, VideoStream) {
  config_.has_audio = false;

  ASSERT_TRUE(InitializeAndWait());
  EXPECT_TRUE(initialize_result_);
  EXPECT_TRUE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_OK, pipeline_.GetError());

  size_t width, height;
  pipeline_.GetVideoSize(&width, &height);
  EXPECT_EQ(config_.video_width, width);
  EXPECT_EQ(config_.video_height, height);
  EXPECT_FALSE(pipeline_.IsRendered(media::mime_type::kMajorTypeAudio));
  EXPECT_TRUE(pipeline_.IsRendered(media::mime_type::kMajorTypeVideo));

  EXPECT_FALSE(filters_->audio_decoder());
  EXPECT_FALSE(filters_->audio_renderer());
  EXPECT_TRUE(filters_->video_decoder());
  EXPECT_TRUE(filters_->video_renderer());
}

TEST_F(PipelineImplTest, AudioVideoStream) {
  ASSERT_TRUE(InitializeAndWait());
  EXPECT_TRUE(initialize_result_);
  EXPECT_TRUE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_OK, pipeline_.GetError());

  size_t width, height;
  pipeline_.GetVideoSize(&width, &height);
  EXPECT_EQ(config_.video_width, width);
  EXPECT_EQ(config_.video_height, height);
  EXPECT_TRUE(pipeline_.IsRendered(media::mime_type::kMajorTypeAudio));
  EXPECT_TRUE(pipeline_.IsRendered(media::mime_type::kMajorTypeVideo));

  EXPECT_TRUE(filters_->audio_decoder());
  EXPECT_TRUE(filters_->audio_renderer());
  EXPECT_TRUE(filters_->video_decoder());
  EXPECT_TRUE(filters_->video_renderer());
}

TEST_F(PipelineImplTest, Seek) {
  ASSERT_TRUE(InitializeAndWait());

  // Seek and verify callback returned true.
  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  EXPECT_TRUE(SeekAndWait(expected));
  EXPECT_TRUE(seek_result_);

  // Verify every filter received the seek.
  // TODO(scherkus): implement whatever it takes so I can use EXPECT_EQ with
  // base::TimeDelta.
  EXPECT_TRUE(expected == filters_->data_source()->seek_time());
  EXPECT_TRUE(expected == filters_->demuxer()->seek_time());
  EXPECT_TRUE(expected == filters_->audio_decoder()->seek_time());
  EXPECT_TRUE(expected == filters_->audio_renderer()->seek_time());
  EXPECT_TRUE(expected == filters_->video_decoder()->seek_time());
  EXPECT_TRUE(expected == filters_->video_renderer()->seek_time());
}

// Try to execute Start()/Stop() on the Pipeline many times and very fast. This
// test is trying to simulate the situation where the pipeline can get dead
// locked very easily by quickly calling Start()/Stop().
TEST_F(PipelineImplTest, StressTestPipelineStartStop) {
  media::old_mocks::MockFilterConfig config;
  const int kTimes = 1000;
  for (int i = 0; i < kTimes; ++i) {
    scoped_refptr<media::old_mocks::MockFilterFactory> factory =
        new media::old_mocks::MockFilterFactory(&config);
    media::PipelineImpl pipeline;
    pipeline.Start(factory.get(), "", NULL);
    pipeline.Stop();
  }
}

// TODO(ralphl): Add a unit test that makes sure that the mock audio filter
// is actually called on a SetVolume() call to the pipeline.  I almost checked
// in code that broke this, but all unit tests were passing.

}  // namespace
