// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "media/base/filters.h"
#include "media/base/mock_ffmpeg.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_reader.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;

namespace media {

// Fixture class to facilitate writing tests.  Takes care of setting up the
// FFmpeg, pipeline and filter host mocks.
class FFmpegDemuxerTest : public testing::Test {
 protected:
  // These constants refer to the stream ordering inside AVFormatContext.  We
  // simulate media with a data stream, audio stream and video stream.  Having
  // the data stream first forces the audio and video streams to get remapped
  // from indices {1,2} to {0,1} respectively, which covers an important test
  // case.
  enum AVStreamIndex {
    AV_STREAM_DATA,
    AV_STREAM_VIDEO,
    AV_STREAM_AUDIO,
    AV_STREAM_MAX,
  };

  // These constants refer to the stream ordering inside an initialized
  // FFmpegDemuxer based on the ordering of the AVStreamIndex constants.
  enum DemuxerStreamIndex {
    DS_STREAM_VIDEO,
    DS_STREAM_AUDIO,
    DS_STREAM_MAX,
  };

  static const int kDurations[];
  static const int kChannels;
  static const int kSampleRate;
  static const int kWidth;
  static const int kHeight;

  static const size_t kDataSize;
  static const uint8 kAudioData[];
  static const uint8 kVideoData[];
  static const uint8* kNullData;

  FFmpegDemuxerTest() {
    // Create an FFmpegDemuxer.
    factory_ = FFmpegDemuxer::CreateFilterFactory();
    MediaFormat media_format;
    media_format.SetAsString(MediaFormat::kMimeType,
                             mime_type::kApplicationOctetStream);
    demuxer_ = factory_->Create<FFmpegDemuxer>(media_format);
    DCHECK(demuxer_);

    // Inject a filter host and message loop and prepare a data source.
    demuxer_->set_filter_host(&host_);
    demuxer_->set_message_loop(&message_loop_);
    data_source_ = new StrictMock<MockDataSource>();

    // Initialize FFmpeg fixtures.
    memset(&format_context_, 0, sizeof(format_context_));
    memset(&streams_, 0, sizeof(streams_));
    memset(&codecs_, 0, sizeof(codecs_));

    // Initialize AVCodexContext structures.
    codecs_[AV_STREAM_DATA].codec_type = CODEC_TYPE_DATA;
    codecs_[AV_STREAM_DATA].codec_id = CODEC_ID_NONE;

    codecs_[AV_STREAM_VIDEO].codec_type = CODEC_TYPE_VIDEO;
    codecs_[AV_STREAM_VIDEO].codec_id = CODEC_ID_THEORA;
    codecs_[AV_STREAM_VIDEO].width = kWidth;
    codecs_[AV_STREAM_VIDEO].height = kHeight;

    codecs_[AV_STREAM_AUDIO].codec_type = CODEC_TYPE_AUDIO;
    codecs_[AV_STREAM_AUDIO].codec_id = CODEC_ID_VORBIS;
    codecs_[AV_STREAM_AUDIO].channels = kChannels;
    codecs_[AV_STREAM_AUDIO].sample_rate = kSampleRate;

    // Initialize AVStream and AVFormatContext structures.  We set the time base
    // of the streams such that duration is reported in microseconds.
    format_context_.nb_streams = AV_STREAM_MAX;
    for (size_t i = 0; i < AV_STREAM_MAX; ++i) {
      format_context_.streams[i] = &streams_[i];
      streams_[i].codec = &codecs_[i];
      streams_[i].duration = kDurations[i];
      streams_[i].time_base.den = 1 * base::Time::kMicrosecondsPerSecond;
      streams_[i].time_base.num = 1;
    }

    // Initialize MockFFmpeg.
    MockFFmpeg::set(&mock_ffmpeg_);
  }

  virtual ~FFmpegDemuxerTest() {
    // Call Stop() to shut down internal threads.
    demuxer_->Stop();

    // Finish up any remaining tasks.
    message_loop_.RunAllPending();

    // Release the reference to the demuxer.
    demuxer_ = NULL;

    // Reset MockFFmpeg.
    MockFFmpeg::set(NULL);
  }

  // Sets up MockFFmpeg to allow FFmpegDemuxer to successfully initialize.
  void InitializeDemuxerMocks() {
    EXPECT_CALL(*MockFFmpeg::get(), AVOpenInputFile(_, _, NULL, 0, NULL))
        .WillOnce(DoAll(SetArgumentPointee<0>(&format_context_), Return(0)));
    EXPECT_CALL(*MockFFmpeg::get(), AVFindStreamInfo(&format_context_))
        .WillOnce(Return(0));
    EXPECT_CALL(*MockFFmpeg::get(), AVCloseInputFile(&format_context_));
  }

  // Initializes both MockFFmpeg and FFmpegDemuxer.
  void InitializeDemuxer() {
    InitializeDemuxerMocks();

    // We expect a successful initialization.
    EXPECT_CALL(host_, InitializationComplete());

    // Since we ignore data streams, the duration should be equal to the longest
    // supported stream's duration (audio, in this case).
    base::TimeDelta expected_duration =
        base::TimeDelta::FromMicroseconds(kDurations[AV_STREAM_AUDIO]);
    EXPECT_CALL(host_, SetDuration(expected_duration));

    EXPECT_TRUE(demuxer_->Initialize(data_source_.get()));
    message_loop_.RunAllPending();
  }

  // Fixture members.
  scoped_refptr<FilterFactory> factory_;
  scoped_refptr<FFmpegDemuxer> demuxer_;
  scoped_refptr<StrictMock<MockDataSource> > data_source_;
  StrictMock<MockFilterHost> host_;
  MessageLoop message_loop_;

  // FFmpeg fixtures.
  AVFormatContext format_context_;
  AVCodecContext codecs_[AV_STREAM_MAX];
  AVStream streams_[AV_STREAM_MAX];
  MockFFmpeg mock_ffmpeg_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerTest);
};

// These durations are picked so that the demuxer chooses the longest supported
// stream, which would be 30 in this case for the audio stream.
const int FFmpegDemuxerTest::kDurations[AV_STREAM_MAX] = {100, 20, 30};
const int FFmpegDemuxerTest::kChannels = 2;
const int FFmpegDemuxerTest::kSampleRate = 44100;
const int FFmpegDemuxerTest::kWidth = 1280;
const int FFmpegDemuxerTest::kHeight = 720;

const size_t FFmpegDemuxerTest::kDataSize = 4;
const uint8 FFmpegDemuxerTest::kAudioData[kDataSize] = {0, 1, 2, 3};
const uint8 FFmpegDemuxerTest::kVideoData[kDataSize] = {4, 5, 6, 7};
const uint8* FFmpegDemuxerTest::kNullData = NULL;

TEST(FFmpegDemuxerFactoryTest, Create) {
  // Should only accept application/octet-stream type.
  scoped_refptr<FilterFactory> factory = FFmpegDemuxer::CreateFilterFactory();
  MediaFormat media_format;
  media_format.SetAsString(MediaFormat::kMimeType, "foo/x-bar");
  scoped_refptr<Demuxer> demuxer(factory->Create<Demuxer>(media_format));
  ASSERT_FALSE(demuxer);

  // Try again with application/octet-stream mime type.
  media_format.Clear();
  media_format.SetAsString(MediaFormat::kMimeType,
                           mime_type::kApplicationOctetStream);
  demuxer = factory->Create<Demuxer>(media_format);
  ASSERT_TRUE(demuxer);
}

TEST_F(FFmpegDemuxerTest, Initialize_OpenFails) {
  // Simulate av_open_input_file() failing.
  EXPECT_CALL(*MockFFmpeg::get(), AVOpenInputFile(_, _, NULL, 0, NULL))
      .WillOnce(Return(-1));
  EXPECT_CALL(host_, Error(DEMUXER_ERROR_COULD_NOT_OPEN));

  EXPECT_TRUE(demuxer_->Initialize(data_source_.get()));
  message_loop_.RunAllPending();
}

TEST_F(FFmpegDemuxerTest, Initialize_ParseFails) {
  // Simulate av_find_stream_info() failing.
  EXPECT_CALL(*MockFFmpeg::get(), AVOpenInputFile(_, _, NULL, 0, NULL))
      .WillOnce(DoAll(SetArgumentPointee<0>(&format_context_), Return(0)));
  EXPECT_CALL(*MockFFmpeg::get(), AVFindStreamInfo(&format_context_))
      .WillOnce(Return(AVERROR_IO));
  EXPECT_CALL(*MockFFmpeg::get(), AVCloseInputFile(&format_context_));
  EXPECT_CALL(host_, Error(DEMUXER_ERROR_COULD_NOT_PARSE));

  EXPECT_TRUE(demuxer_->Initialize(data_source_.get()));
  message_loop_.RunAllPending();
}

TEST_F(FFmpegDemuxerTest, Initialize_NoStreams) {
  // Simulate media with no parseable streams.
  {
    SCOPED_TRACE("");
    InitializeDemuxerMocks();
  }
  EXPECT_CALL(host_, Error(DEMUXER_ERROR_NO_SUPPORTED_STREAMS));
  format_context_.nb_streams = 0;

  EXPECT_TRUE(demuxer_->Initialize(data_source_.get()));
  message_loop_.RunAllPending();
}

TEST_F(FFmpegDemuxerTest, Initialize_DataStreamOnly) {
  // Simulate media with a data stream but no audio or video streams.
  {
    SCOPED_TRACE("");
    InitializeDemuxerMocks();
  }
  EXPECT_CALL(host_, Error(DEMUXER_ERROR_NO_SUPPORTED_STREAMS));
  EXPECT_EQ(format_context_.streams[0], &streams_[AV_STREAM_DATA]);
  format_context_.nb_streams = 1;

  EXPECT_TRUE(demuxer_->Initialize(data_source_.get()));
  message_loop_.RunAllPending();
}

TEST_F(FFmpegDemuxerTest, Initialize_Successful) {
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  // Verify that our demuxer streams were created from our AVStream structures.
  EXPECT_EQ(DS_STREAM_MAX, static_cast<int>(demuxer_->GetNumberOfStreams()));

  // First stream should be video and support the FFmpegDemuxerStream interface.
  scoped_refptr<DemuxerStream> stream = demuxer_->GetStream(DS_STREAM_VIDEO);
  AVStreamProvider* av_stream_provider = NULL;
  ASSERT_TRUE(stream);
  std::string mime_type;
  EXPECT_TRUE(
      stream->media_format().GetAsString(MediaFormat::kMimeType, &mime_type));
  EXPECT_STREQ(mime_type::kFFmpegVideo, mime_type.c_str());
  EXPECT_TRUE(stream->QueryInterface(&av_stream_provider));
  EXPECT_TRUE(av_stream_provider);
  EXPECT_EQ(&streams_[AV_STREAM_VIDEO], av_stream_provider->GetAVStream());

  // Other stream should be audio and support the FFmpegDemuxerStream interface.
  stream = demuxer_->GetStream(DS_STREAM_AUDIO);
  av_stream_provider = NULL;
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->media_format().GetAsString(MediaFormat::kMimeType,
              &mime_type));
  EXPECT_STREQ(mime_type::kFFmpegAudio, mime_type.c_str());
  EXPECT_TRUE(stream->QueryInterface(&av_stream_provider));
  EXPECT_TRUE(av_stream_provider);
  EXPECT_EQ(&streams_[AV_STREAM_AUDIO], av_stream_provider->GetAVStream());
}

TEST_F(FFmpegDemuxerTest, Read) {
  // We're testing the following:
  //
  //   1) The demuxer immediately frees packets it doesn't care about and keeps
  //      reading until it finds a packet it cares about.
  //   2) The demuxer doesn't free packets that we read from it.
  //   3) On end of stream, the demuxer queues end of stream packets on every
  //      stream.
  //
  // Since we can't test which packets are being freed, we use check points to
  // infer that the correct packets have been freed.
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  // Get our streams.
  scoped_refptr<DemuxerStream> video = demuxer_->GetStream(DS_STREAM_VIDEO);
  scoped_refptr<DemuxerStream> audio = demuxer_->GetStream(DS_STREAM_AUDIO);
  ASSERT_TRUE(video);
  ASSERT_TRUE(audio);

  // Expect all calls in sequence.
  InSequence s;

  // The demuxer will read a data packet which will get immediately freed,
  // followed by reading an audio packet...
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacket(AV_STREAM_DATA, kNullData, 0));
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).WillOnce(FreePacket());
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacket(AV_STREAM_AUDIO, kAudioData, kDataSize));

  // ...then we'll free it with some sanity checkpoints...
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(1));
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).WillOnce(FreePacket());
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(2));

  // ...then we'll read a video packet...
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacket(AV_STREAM_VIDEO, kVideoData, kDataSize));

  // ...then we'll free it with some sanity checkpoints...
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(3));
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).WillOnce(FreePacket());
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(4));

  // ...then we'll simulate end of stream.  Note that a packet isn't "created"
  // in this situation so there is no outstanding packet.   However an end of
  // stream packet is created for each stream, which means av_free_packet()
  // will still be called twice.
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(Return(AVERROR_IO));
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_));
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(5));
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_));
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(6));

  // Attempt a read from the audio stream and run the message loop until done.
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_FALSE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(kAudioData, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // We shouldn't have freed the audio packet yet.
  MockFFmpeg::get()->CheckPoint(1);

  // Manually release the last reference to the buffer.
  reader->Reset();
  message_loop_.RunAllPending();
  MockFFmpeg::get()->CheckPoint(2);

  // Attempt a read from the video stream and run the message loop until done.
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_FALSE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(kVideoData, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // We shouldn't have freed the video packet yet.
  MockFFmpeg::get()->CheckPoint(3);

  // Manually release the last reference to the buffer and verify it was freed.
  reader->Reset();
  message_loop_.RunAllPending();
  MockFFmpeg::get()->CheckPoint(4);

  // We should now expect an end of stream buffer in both the audio and video
  // streams.

  // Attempt a read from the audio stream and run the message loop until done.
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_TRUE(reader->buffer()->IsEndOfStream());
  EXPECT_EQ(NULL, reader->buffer()->GetData());
  EXPECT_EQ(0u, reader->buffer()->GetDataSize());

  // Manually release buffer, which should release any remaining AVPackets.
  reader->Reset();
  message_loop_.RunAllPending();
  MockFFmpeg::get()->CheckPoint(5);

  // Attempt a read from the audio stream and run the message loop until done.
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_TRUE(reader->buffer()->IsEndOfStream());
  EXPECT_EQ(NULL, reader->buffer()->GetData());
  EXPECT_EQ(0u, reader->buffer()->GetDataSize());

  // Manually release buffer, which should release any remaining AVPackets.
  reader->Reset();
  message_loop_.RunAllPending();
  MockFFmpeg::get()->CheckPoint(6);
}

TEST_F(FFmpegDemuxerTest, Seek) {
  // We're testing the following:
  //
  //   1) The demuxer frees all queued packets when it receives a Seek().
  //   2) The demuxer queues a single discontinuous packet on every stream.
  //
  // Since we can't test which packets are being freed, we use check points to
  // infer that the correct packets have been freed.
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  // Get our streams.
  scoped_refptr<DemuxerStream> video = demuxer_->GetStream(DS_STREAM_VIDEO);
  scoped_refptr<DemuxerStream> audio = demuxer_->GetStream(DS_STREAM_AUDIO);
  ASSERT_TRUE(video);
  ASSERT_TRUE(audio);

  // Expected values.
  const int64 kExpectedTimestamp = 1234;
  const int64 kExpectedFlags = 0;

  // Expect all calls in sequence.
  InSequence s;

  // First we'll read a video packet that causes two audio packets to be queued
  // inside FFmpegDemuxer...
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacket(AV_STREAM_AUDIO, kAudioData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacket(AV_STREAM_AUDIO, kAudioData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacket(AV_STREAM_VIDEO, kVideoData, kDataSize));

  // ...then we'll release our video packet...
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).WillOnce(FreePacket());
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(1));

  // ...then we'll seek, which should release the previously queued packets...
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).WillOnce(FreePacket());
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).WillOnce(FreePacket());

  // ...then we'll expect the actual seek call...
  EXPECT_CALL(*MockFFmpeg::get(),
      AVSeekFrame(&format_context_, -1, kExpectedTimestamp, kExpectedFlags))
      .WillOnce(Return(0));
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(2));

  // ...followed by two audio packet reads we'll trigger...
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacket(AV_STREAM_AUDIO, kAudioData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).WillOnce(FreePacket());
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacket(AV_STREAM_AUDIO, kAudioData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).WillOnce(FreePacket());

  // ...followed by two video packet reads...
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacket(AV_STREAM_VIDEO, kVideoData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).WillOnce(FreePacket());
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacket(AV_STREAM_VIDEO, kVideoData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).WillOnce(FreePacket());

  // ...and finally a sanity checkpoint to make sure everything was released.
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(3));

  // Read a video packet and release it.
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_FALSE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(kVideoData, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // Release the video packet and verify the other packets are still queued.
  reader->Reset();
  message_loop_.RunAllPending();
  MockFFmpeg::get()->CheckPoint(1);

  // Now issue a simple forward seek, which should discard queued packets.
  demuxer_->Seek(base::TimeDelta::FromMicroseconds(kExpectedTimestamp));
  message_loop_.RunAllPending();
  MockFFmpeg::get()->CheckPoint(2);

  // The next read from each stream should now be discontinuous, but subsequent
  // reads should not.

  // Audio read #1, should be discontinuous.
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_TRUE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(kAudioData, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // Audio read #2, should not be discontinuous.
  reader->Reset();
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_FALSE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(kAudioData, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // Video read #1, should be discontinuous.
  reader->Reset();
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_TRUE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(kVideoData, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // Video read #2, should not be discontinuous.
  reader->Reset();
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_FALSE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(kVideoData, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // Manually release the last reference to the buffer and verify it was freed.
  reader->Reset();
  message_loop_.RunAllPending();
  MockFFmpeg::get()->CheckPoint(3);
}

TEST_F(FFmpegDemuxerTest, MP3Hack) {
  // This tests our deep-copying workaround for FFmpeg's MP3 demuxer.  When we
  // fix the root cause this test will fail and should be removed.
  //
  // TODO(scherkus): according to the documentation, deep-copying the packet is
  // actually the correct action -- remove this test when we fix our demuxer.

  // Simulate an MP3 stream.
  codecs_[AV_STREAM_AUDIO].codec_id = CODEC_ID_MP3;
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  // Get our stream.
  scoped_refptr<DemuxerStream> audio = demuxer_->GetStream(DS_STREAM_AUDIO);
  ASSERT_TRUE(audio);

  // Expect all calls in sequence.
  InSequence s;

  // We'll read an MP3 packet and allocate a new packet, then instantly free
  // the original packet due to deep copying...
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacket(AV_STREAM_AUDIO, kAudioData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVNewPacket(_, _)).WillOnce(NewPacket());
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).WillOnce(FreePacket());

  // ...then we'll have a sanity checkpoint...
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(1));

  // ...then we'll free the deep copied packet.
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).WillOnce(FreePacket());
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(2));

  // Audio read should perform a deep copy on the packet and instantly release
  // the original packet.  The data pointers should not be the same, but the
  // contents should match.
  scoped_refptr<DemuxerStreamReader> reader = new DemuxerStreamReader();
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_FALSE(reader->buffer()->IsDiscontinuous());
  EXPECT_NE(kAudioData, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());
  EXPECT_EQ(0, memcmp(kAudioData, reader->buffer()->GetData(), kDataSize));

  // We shouldn't have freed the MP3 packet yet.
  MockFFmpeg::get()->CheckPoint(1);

  // Manually release the last reference to the buffer and verify it was freed.
  reader->Reset();
  message_loop_.RunAllPending();
  MockFFmpeg::get()->CheckPoint(2);
}

// A mocked callback specialization for calling Read().  Since RunWithParams()
// is mocked we don't need to pass in object or method pointers.
typedef CallbackImpl<FFmpegDemuxerTest, void (FFmpegDemuxerTest::*)(Buffer*),
                     Tuple1<Buffer*> > ReadCallback;
class MockReadCallback : public ReadCallback {
 public:
  MockReadCallback()
      : ReadCallback(NULL, NULL) {
  }

  virtual ~MockReadCallback() {
    OnDelete();
  }

  MOCK_METHOD0(OnDelete, void());
  MOCK_METHOD1(RunWithParams, void(const Tuple1<Buffer*>& params));
};

TEST_F(FFmpegDemuxerTest, Stop) {
  // Tests that calling Read() on a stopped demuxer immediately deletes the
  // callback.
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  // Create our mocked callback.  The demuxer will take ownership of this
  // pointer.
  scoped_ptr<StrictMock<MockReadCallback> > callback(
      new StrictMock<MockReadCallback>());

  // Get our stream.
  scoped_refptr<DemuxerStream> audio = demuxer_->GetStream(DS_STREAM_AUDIO);
  ASSERT_TRUE(audio);

  // Stop the demuxer.
  demuxer_->Stop();

  // Expect all calls in sequence.
  InSequence s;

  // The callback should be immediately deleted.  We'll use a checkpoint to
  // verify that it has indeed been deleted.
  EXPECT_CALL(*callback, OnDelete());
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(1));

  // Attempt the read...
  audio->Read(callback.release());
  message_loop_.RunAllPending();

  // ...and verify that |callback| was deleted.
  MockFFmpeg::get()->CheckPoint(1);
}

}  // namespace media
