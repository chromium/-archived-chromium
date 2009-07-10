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
#include "media/base/mock_filters.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::DoAll;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

typedef std::vector<MockDemuxerStream*> MockDemuxerStreamVector;

class PipelineImplTest : public ::testing::Test {
 public:
  PipelineImplTest()
      : mocks_(new MockFilterFactory()),
        initialize_result_(false),
        seek_result_(false),
        initialize_event_(false, false),
        seek_event_(false, false) {
  }

  virtual ~PipelineImplTest() {
    // Force the pipeline to shut down its thread.
    pipeline_.Stop();
  }

 protected:
  // Called by tests after they have finished setting up MockFilterConfig.
  // Initializes the pipeline and returns true if the initialization callback
  // was executed, false otherwise.
  bool InitializeAndWait() {
    pipeline_.Start(mocks_, "",
                    NewCallback(this, &PipelineImplTest::OnInitialize));
    return initialize_event_.TimedWait(base::TimeDelta::FromMilliseconds(500));
  }

  // Issues a seek on the pipeline and returns true if the seek callback was
  // executed, false otherwise.
  bool SeekAndWait(const base::TimeDelta& time) {
    pipeline_.Seek(time, NewCallback(this, &PipelineImplTest::OnSeek));
    return seek_event_.TimedWait(base::TimeDelta::FromMilliseconds(500));
  }

  // Sets up expectations to allow the data source to initialize.
  void InitializeDataSource() {
    EXPECT_CALL(*mocks_->data_source(), Initialize(""))
        .WillOnce(DoAll(InitializationComplete(mocks_->data_source()),
                        Return(true)));
    EXPECT_CALL(*mocks_->data_source(), Stop());
  }

  // Sets up expectations to allow the demuxer to initialize.
  void InitializeDemuxer(MockDemuxerStreamVector* streams) {
    EXPECT_CALL(*mocks_->demuxer(), Initialize(mocks_->data_source()))
        .WillOnce(DoAll(InitializationComplete(mocks_->demuxer()),
                        Return(true)));
    EXPECT_CALL(*mocks_->demuxer(), GetNumberOfStreams())
        .WillRepeatedly(Return(streams->size()));
    EXPECT_CALL(*mocks_->demuxer(), Stop());

    // Configure the demuxer to return the streams.
    for (size_t i = 0; i < streams->size(); ++i) {
      scoped_refptr<DemuxerStream> stream = (*streams)[i];
      EXPECT_CALL(*mocks_->demuxer(), GetStream(i))
          .WillRepeatedly(Return(stream));
    }
  }

  // Sets up expectations to allow the video decoder to initialize.
  void InitializeVideoDecoder(MockDemuxerStream* stream) {
    EXPECT_CALL(*mocks_->video_decoder(), Initialize(stream))
        .WillOnce(DoAll(InitializationComplete(mocks_->video_decoder()),
                        Return(true)));
    EXPECT_CALL(*mocks_->video_decoder(), Stop());
  }

  // Sets up expectations to allow the audio decoder to initialize.
  void InitializeAudioDecoder(MockDemuxerStream* stream) {
    EXPECT_CALL(*mocks_->audio_decoder(), Initialize(stream))
        .WillOnce(DoAll(InitializationComplete(mocks_->audio_decoder()),
                        Return(true)));
    EXPECT_CALL(*mocks_->audio_decoder(), Stop());
  }

  // Sets up expectations to allow the video renderer to initialize.
  void InitializeVideoRenderer() {
    EXPECT_CALL(*mocks_->video_renderer(), Initialize(mocks_->video_decoder()))
        .WillOnce(DoAll(InitializationComplete(mocks_->video_renderer()),
                        Return(true)));
    EXPECT_CALL(*mocks_->video_renderer(), Stop());
  }

  // Sets up expectations to allow the audio renderer to initialize.
  void InitializeAudioRenderer() {
    EXPECT_CALL(*mocks_->audio_renderer(), Initialize(mocks_->audio_decoder()))
        .WillOnce(DoAll(InitializationComplete(mocks_->audio_renderer()),
                        Return(true)));
    EXPECT_CALL(*mocks_->audio_renderer(), Stop());
  }

  // Fixture members.
  media::PipelineImpl pipeline_;
  scoped_refptr<media::MockFilterFactory> mocks_;
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
  EXPECT_CALL(*mocks_->data_source(), Initialize(""))
      .WillOnce(Return(true));
  EXPECT_CALL(*mocks_->data_source(), Stop());

  // This test hangs during initialization by never calling
  // InitializationComplete().  Make sure we tear down the pipeline properly.
  ASSERT_FALSE(InitializeAndWait());
  EXPECT_FALSE(initialize_result_);
  EXPECT_FALSE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_OK, pipeline_.GetError());
}

TEST_F(PipelineImplTest, RequiredFilterMissing) {
  mocks_->set_creation_successful(false);

  ASSERT_TRUE(InitializeAndWait());
  EXPECT_FALSE(initialize_result_);
  EXPECT_FALSE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_ERROR_REQUIRED_FILTER_MISSING,
            pipeline_.GetError());
}

TEST_F(PipelineImplTest, URLNotFound) {
  EXPECT_CALL(*mocks_->data_source(), Initialize(""))
      .WillOnce(DoAll(Error(mocks_->data_source(),
                            PIPELINE_ERROR_URL_NOT_FOUND),
                      Return(false)));
  EXPECT_CALL(*mocks_->data_source(), Stop());

  ASSERT_TRUE(InitializeAndWait());
  EXPECT_FALSE(initialize_result_);
  EXPECT_FALSE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_ERROR_URL_NOT_FOUND, pipeline_.GetError());
}

TEST_F(PipelineImplTest, NoStreams) {
  MockDemuxerStreamVector streams;
  InitializeDataSource();
  InitializeDemuxer(&streams);

  ASSERT_TRUE(InitializeAndWait());
  EXPECT_FALSE(initialize_result_);
  EXPECT_FALSE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_ERROR_COULD_NOT_RENDER, pipeline_.GetError());
}

TEST_F(PipelineImplTest, AudioStream) {
  scoped_refptr<StrictMock<MockDemuxerStream> > stream =
      new StrictMock<MockDemuxerStream>("audio/x-foo");
  MockDemuxerStreamVector streams;
  streams.push_back(stream);

  InitializeDataSource();
  InitializeDemuxer(&streams);
  InitializeAudioDecoder(stream);
  InitializeAudioRenderer();

  ASSERT_TRUE(InitializeAndWait());
  EXPECT_TRUE(initialize_result_);
  EXPECT_TRUE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_OK, pipeline_.GetError());
  EXPECT_TRUE(pipeline_.IsRendered(media::mime_type::kMajorTypeAudio));
  EXPECT_FALSE(pipeline_.IsRendered(media::mime_type::kMajorTypeVideo));
}

TEST_F(PipelineImplTest, VideoStream) {
  scoped_refptr<StrictMock<MockDemuxerStream> > stream =
      new StrictMock<MockDemuxerStream>("video/x-foo");
  MockDemuxerStreamVector streams;
  streams.push_back(stream);

  InitializeDataSource();
  InitializeDemuxer(&streams);
  InitializeVideoDecoder(stream);
  InitializeVideoRenderer();

  ASSERT_TRUE(InitializeAndWait());
  EXPECT_TRUE(initialize_result_);
  EXPECT_TRUE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_OK, pipeline_.GetError());
  EXPECT_FALSE(pipeline_.IsRendered(media::mime_type::kMajorTypeAudio));
  EXPECT_TRUE(pipeline_.IsRendered(media::mime_type::kMajorTypeVideo));
}

TEST_F(PipelineImplTest, AudioVideoStream) {
  scoped_refptr<StrictMock<MockDemuxerStream> > audio_stream =
      new StrictMock<MockDemuxerStream>("audio/x-foo");
  scoped_refptr<StrictMock<MockDemuxerStream> > video_stream =
      new StrictMock<MockDemuxerStream>("video/x-foo");
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream);
  streams.push_back(video_stream);

  InitializeDataSource();
  InitializeDemuxer(&streams);
  InitializeAudioDecoder(audio_stream);
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream);
  InitializeVideoRenderer();

  ASSERT_TRUE(InitializeAndWait());
  EXPECT_TRUE(initialize_result_);
  EXPECT_TRUE(pipeline_.IsInitialized());
  EXPECT_EQ(media::PIPELINE_OK, pipeline_.GetError());
  EXPECT_TRUE(pipeline_.IsRendered(media::mime_type::kMajorTypeAudio));
  EXPECT_TRUE(pipeline_.IsRendered(media::mime_type::kMajorTypeVideo));
}

TEST_F(PipelineImplTest, Seek) {
  scoped_refptr<StrictMock<MockDemuxerStream> > audio_stream =
      new StrictMock<MockDemuxerStream>("audio/x-foo");
  scoped_refptr<StrictMock<MockDemuxerStream> > video_stream =
      new StrictMock<MockDemuxerStream>("video/x-foo");
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream);
  streams.push_back(video_stream);

  InitializeDataSource();
  InitializeDemuxer(&streams);
  InitializeAudioDecoder(audio_stream);
  InitializeAudioRenderer();
  InitializeVideoDecoder(video_stream);
  InitializeVideoRenderer();

  // Every filter should receive a call to Seek().
  base::TimeDelta expected = base::TimeDelta::FromSeconds(2000);
  EXPECT_CALL(*mocks_->data_source(), Seek(expected));
  EXPECT_CALL(*mocks_->demuxer(), Seek(expected));
  EXPECT_CALL(*mocks_->audio_decoder(), Seek(expected));
  EXPECT_CALL(*mocks_->audio_renderer(), Seek(expected));
  EXPECT_CALL(*mocks_->video_decoder(), Seek(expected));
  EXPECT_CALL(*mocks_->video_renderer(), Seek(expected));

  // Initialize then seek!
  ASSERT_TRUE(InitializeAndWait());
  EXPECT_TRUE(SeekAndWait(expected));
  EXPECT_TRUE(seek_result_);
}

TEST_F(PipelineImplTest, SetVolume) {
  scoped_refptr<StrictMock<MockDemuxerStream> > audio_stream =
      new StrictMock<MockDemuxerStream>("audio/x-foo");
  MockDemuxerStreamVector streams;
  streams.push_back(audio_stream);

  InitializeDataSource();
  InitializeDemuxer(&streams);
  InitializeAudioDecoder(audio_stream);
  InitializeAudioRenderer();

  // The audio renderer should receive a call to SetVolume().
  float expected = 0.5f;
  EXPECT_CALL(*mocks_->audio_renderer(), SetVolume(expected));

  // Initialize then set volume!
  ASSERT_TRUE(InitializeAndWait());
  pipeline_.SetVolume(expected);
}

}  // namespace media

