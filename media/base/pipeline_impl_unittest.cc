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
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

// Used for setting expectations on pipeline callbacks.  Using a StrictMock
// also lets us test for missing callbacks.
class CallbackHelper {
 public:
  CallbackHelper() {}
  virtual ~CallbackHelper() {}

  MOCK_METHOD1(OnInitialize, void(bool result));
  MOCK_METHOD1(OnSeek, void(bool result));
  MOCK_METHOD1(OnStop, void(bool result));

 private:
  DISALLOW_COPY_AND_ASSIGN(CallbackHelper);
};

// TODO(scherkus): even though some filters are initialized on separate
// threads these test aren't flaky... why?  It's because filters' Initialize()
// is executed on |message_loop_| and the mock filters instantly call
// InitializationComplete(), which keeps the pipeline humming along.  If
// either filters don't call InitializationComplete() immediately or filter
// initialization is moved to a separate thread this test will become flaky.
class PipelineImplTest : public ::testing::Test {
 public:
  PipelineImplTest()
      : pipeline_(&message_loop_),
        mocks_(new MockFilterFactory()) {
  }

  virtual ~PipelineImplTest() {
    if (!pipeline_.IsRunning()) {
      return;
    }

    // Expect a stop callback if we were started.
    EXPECT_CALL(callbacks_, OnStop(true));
    pipeline_.Stop(NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                               &CallbackHelper::OnStop));
    message_loop_.RunAllPending();
  }

 protected:
  // Sets up expectations to allow the data source to initialize.
  void InitializeDataSource() {
    EXPECT_CALL(*mocks_->data_source(), Initialize(""))
        .WillOnce(DoAll(InitializationComplete(mocks_->data_source()),
                        Return(true)));
    EXPECT_CALL(*mocks_->data_source(), Stop());
  }

  // Sets up expectations to allow the demuxer to initialize.
  typedef std::vector<MockDemuxerStream*> MockDemuxerStreamVector;
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

  // Sets up expectations on the callback and initializes the pipeline.  Called
  // afters tests have set expectations any filters they wish to use.
  void InitializePipeline(bool callback_result) {
    // Expect an initialization callback.
    EXPECT_CALL(callbacks_, OnInitialize(callback_result));
    pipeline_.Start(mocks_, "",
                    NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                                &CallbackHelper::OnInitialize));
    message_loop_.RunAllPending();
  }

  // Fixture members.
  StrictMock<CallbackHelper> callbacks_;
  MessageLoop message_loop_;
  PipelineImpl pipeline_;
  scoped_refptr<media::MockFilterFactory> mocks_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PipelineImplTest);
};

TEST_F(PipelineImplTest, NeverInitializes) {
  EXPECT_CALL(*mocks_->data_source(), Initialize(""))
      .WillOnce(Return(true));
  EXPECT_CALL(*mocks_->data_source(), Stop());

  // This test hangs during initialization by never calling
  // InitializationComplete().  StrictMock<> will ensure that the callback is
  // never executed.
  pipeline_.Start(mocks_, "",
                  NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                              &CallbackHelper::OnInitialize));
  message_loop_.RunAllPending();

  EXPECT_FALSE(pipeline_.IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_.GetError());

  // Because our callback will get executed when the test tears down, we'll
  // verify that nothing has been called, then set our expectation for the call
  // made during tear down.
  Mock::VerifyAndClear(&callbacks_);
  EXPECT_CALL(callbacks_, OnInitialize(false));
}

TEST_F(PipelineImplTest, RequiredFilterMissing) {
  mocks_->set_creation_successful(false);

  InitializePipeline(false);
  EXPECT_FALSE(pipeline_.IsInitialized());
  EXPECT_EQ(PIPELINE_ERROR_REQUIRED_FILTER_MISSING,
            pipeline_.GetError());
}

TEST_F(PipelineImplTest, URLNotFound) {
  EXPECT_CALL(*mocks_->data_source(), Initialize(""))
      .WillOnce(DoAll(Error(mocks_->data_source(),
                            PIPELINE_ERROR_URL_NOT_FOUND),
                      Return(false)));
  EXPECT_CALL(*mocks_->data_source(), Stop());

  InitializePipeline(false);
  EXPECT_FALSE(pipeline_.IsInitialized());
  EXPECT_EQ(PIPELINE_ERROR_URL_NOT_FOUND, pipeline_.GetError());
}

TEST_F(PipelineImplTest, NoStreams) {
  MockDemuxerStreamVector streams;
  InitializeDataSource();
  InitializeDemuxer(&streams);

  InitializePipeline(false);
  EXPECT_FALSE(pipeline_.IsInitialized());
  EXPECT_EQ(PIPELINE_ERROR_COULD_NOT_RENDER, pipeline_.GetError());
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

  InitializePipeline(true);
  EXPECT_TRUE(pipeline_.IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_.GetError());
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

  InitializePipeline(true);
  EXPECT_TRUE(pipeline_.IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_.GetError());
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

  InitializePipeline(true);
  EXPECT_TRUE(pipeline_.IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_.GetError());
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

  // We expect a successful seek callback.
  EXPECT_CALL(callbacks_, OnSeek(true));

  // Initialize then seek!
  InitializePipeline(true);
  pipeline_.Seek(expected,
                 NewCallback(reinterpret_cast<CallbackHelper*>(&callbacks_),
                             &CallbackHelper::OnSeek));
  message_loop_.RunAllPending();
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
  InitializePipeline(true);
  pipeline_.SetVolume(expected);
}

}  // namespace media
