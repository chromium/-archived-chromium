// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/singleton.h"
#include "media/base/data_buffer.h"
#include "media/base/filters.h"
#include "media/base/mock_ffmpeg.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_interfaces.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;

namespace media {

class MockFFmpegDemuxerStream : public MockDemuxerStream,
                                public AVStreamProvider {
 public:
  MockFFmpegDemuxerStream() {}

  // AVStreamProvider implementation.
  MOCK_METHOD0(GetAVStream, AVStream*());

 protected:
  virtual ~MockFFmpegDemuxerStream() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFFmpegDemuxerStream);
};

// Class that just mocks the private functions.
class DecoderPrivateMock : public FFmpegVideoDecoder {
 public:
  MOCK_METHOD3(EnqueueVideoFrame, bool(VideoSurface::Format surface_format,
                                       const TimeTuple& time,
                                       const AVFrame* frame));
  MOCK_METHOD0(EnqueueEmptyFrame, void());
  MOCK_METHOD3(CopyPlane, void(size_t plane, const VideoSurface& surface,
                               const AVFrame* frame));
  MOCK_METHOD1(GetSurfaceFormat,
               VideoSurface::Format(const AVCodecContext& codec_context));
  MOCK_METHOD3(DecodeFrame, bool(const Buffer& buffer,
                                 AVCodecContext* codec_context,
                                 AVFrame* yuv_frame));
  MOCK_METHOD4(FindPtsAndDuration, TimeTuple(const AVRational& time_base,
                                             const TimeQueue& pts_queue,
                                             const TimeTuple& last_pts,
                                             const AVFrame* frame));
  MOCK_METHOD0(SignalPipelineError, void());
};

// Fixture class to facilitate writing tests.  Takes care of setting up the
// FFmpeg, pipeline and filter host mocks.
class FFmpegVideoDecoderTest : public testing::Test {
 protected:
  static const int kWidth;
  static const int kHeight;
  static const FFmpegVideoDecoder::TimeTuple kTestPts1;
  static const FFmpegVideoDecoder::TimeTuple kTestPts2;

  FFmpegVideoDecoderTest() {
    MediaFormat media_format;
    media_format.SetAsString(MediaFormat::kMimeType, mime_type::kFFmpegVideo);

    // Create an FFmpegVideoDecoder.
    factory_ = FFmpegVideoDecoder::CreateFactory();
    decoder_ = factory_->Create<FFmpegVideoDecoder>(media_format);
    DCHECK(decoder_);

    // Inject a filter host and message loop and prepare a demuxer stream.
    decoder_->set_host(&host_);
    decoder_->set_message_loop(&message_loop_);
    demuxer_ = new StrictMock<MockFFmpegDemuxerStream>();

    // Manually set the thread id for tests that don't initialize the decoder.
    decoder_->set_thread_id(PlatformThread::CurrentId());

    // Initialize FFmpeg fixtures.
    memset(&stream_, 0, sizeof(stream_));
    memset(&codec_context_, 0, sizeof(codec_context_));
    memset(&codec_, 0, sizeof(codec_));
    memset(&yuv_frame_, 0, sizeof(yuv_frame_));

    stream_.codec = &codec_context_;
    codec_context_.width = kWidth;
    codec_context_.height = kHeight;
    buffer_ = new DataBuffer();
    buffer_->GetWritableData(1);
    end_of_stream_buffer_ = new DataBuffer();

    // Initialize MockFFmpeg.
    MockFFmpeg::set(&mock_ffmpeg_);
  }

  virtual ~FFmpegVideoDecoderTest() {
    // Call Stop() to shut down internal threads.
    decoder_->Stop();

    // Finish up any remaining tasks.
    message_loop_.RunAllPending();

    // Reset MockFFmpeg.
    MockFFmpeg::set(NULL);
  }

  // Fixture members.
  scoped_refptr<FilterFactory> factory_;
  scoped_refptr<FFmpegVideoDecoder> decoder_;
  scoped_refptr<StrictMock<MockFFmpegDemuxerStream> > demuxer_;
  scoped_refptr<DataBuffer> buffer_;
  scoped_refptr<DataBuffer> end_of_stream_buffer_;
  StrictMock<MockFilterHost> host_;
  MessageLoop message_loop_;

  // FFmpeg fixtures.
  AVStream stream_;
  AVCodecContext codec_context_;
  AVCodec codec_;
  AVFrame yuv_frame_;
  StrictMock<MockFFmpeg> mock_ffmpeg_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoderTest);
};

const int FFmpegVideoDecoderTest::kWidth = 1280;
const int FFmpegVideoDecoderTest::kHeight = 720;
const FFmpegVideoDecoder::TimeTuple FFmpegVideoDecoderTest::kTestPts1 =
    { base::TimeDelta::FromMicroseconds(123),
      base::TimeDelta::FromMicroseconds(50) };
const FFmpegVideoDecoder::TimeTuple FFmpegVideoDecoderTest::kTestPts2 =
    { base::TimeDelta::FromMicroseconds(456),
      base::TimeDelta::FromMicroseconds(60) };

TEST(FFmpegVideoDecoderFactoryTest, Create) {
  // Should only accept video/x-ffmpeg mime type.
  scoped_refptr<FilterFactory> factory = FFmpegVideoDecoder::CreateFactory();
  MediaFormat media_format;
  media_format.SetAsString(MediaFormat::kMimeType, "foo/x-bar");
  scoped_refptr<VideoDecoder> decoder =
      factory->Create<VideoDecoder>(media_format);
  ASSERT_FALSE(decoder);

  // Try again with video/x-ffmpeg mime type.
  media_format.Clear();
  media_format.SetAsString(MediaFormat::kMimeType,
                           mime_type::kFFmpegVideo);
  decoder = factory->Create<VideoDecoder>(media_format);
  ASSERT_TRUE(decoder);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_QueryInterfaceFails) {
  // Test QueryInterface returning NULL.
  EXPECT_CALL(*demuxer_, QueryInterface(AVStreamProvider::interface_id()))
      .WillOnce(ReturnNull());
  EXPECT_CALL(host_, Error(PIPELINE_ERROR_DECODE));

  EXPECT_TRUE(decoder_->Initialize(demuxer_));
  message_loop_.RunAllPending();
}

TEST_F(FFmpegVideoDecoderTest, Initialize_FindDecoderFails) {
  // Test avcodec_find_decoder() returning NULL.
  AVStreamProvider* av_stream_provider = demuxer_;
  EXPECT_CALL(*demuxer_, QueryInterface(AVStreamProvider::interface_id()))
      .WillOnce(Return(av_stream_provider));
  EXPECT_CALL(*demuxer_, GetAVStream())
      .WillOnce(Return(&stream_));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecFindDecoder(CODEC_ID_NONE))
      .WillOnce(ReturnNull());
  EXPECT_CALL(host_, Error(PIPELINE_ERROR_DECODE));

  EXPECT_TRUE(decoder_->Initialize(demuxer_));
  message_loop_.RunAllPending();
}

TEST_F(FFmpegVideoDecoderTest, Initialize_InitThreadFails) {
  // Test avcodec_thread_init() failing.
  AVStreamProvider* av_stream_provider = demuxer_;
  EXPECT_CALL(*demuxer_, QueryInterface(AVStreamProvider::interface_id()))
      .WillOnce(Return(av_stream_provider));
  EXPECT_CALL(*demuxer_, GetAVStream())
      .WillOnce(Return(&stream_));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecFindDecoder(CODEC_ID_NONE))
      .WillOnce(Return(&codec_));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecThreadInit(&codec_context_, 2))
      .WillOnce(Return(-1));
  EXPECT_CALL(host_, Error(PIPELINE_ERROR_DECODE));

  EXPECT_TRUE(decoder_->Initialize(demuxer_));
  message_loop_.RunAllPending();
}

TEST_F(FFmpegVideoDecoderTest, Initialize_OpenDecoderFails) {
  // Test avcodec_open() failing.
  AVStreamProvider* av_stream_provider = demuxer_;
  EXPECT_CALL(*demuxer_, QueryInterface(AVStreamProvider::interface_id()))
      .WillOnce(Return(av_stream_provider));
  EXPECT_CALL(*demuxer_, GetAVStream())
      .WillOnce(Return(&stream_));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecFindDecoder(CODEC_ID_NONE))
      .WillOnce(Return(&codec_));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecThreadInit(&codec_context_, 2))
      .WillOnce(Return(0));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecOpen(&codec_context_, &codec_))
      .WillOnce(Return(-1));
  EXPECT_CALL(host_, Error(PIPELINE_ERROR_DECODE));

  EXPECT_TRUE(decoder_->Initialize(demuxer_));
  message_loop_.RunAllPending();
}

TEST_F(FFmpegVideoDecoderTest, Initialize_Successful) {
  // Test successful initialization.
  AVStreamProvider* av_stream_provider = demuxer_;
  EXPECT_CALL(*demuxer_, QueryInterface(AVStreamProvider::interface_id()))
      .WillOnce(Return(av_stream_provider));
  EXPECT_CALL(*demuxer_, GetAVStream())
      .WillOnce(Return(&stream_));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecFindDecoder(CODEC_ID_NONE))
      .WillOnce(Return(&codec_));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecThreadInit(&codec_context_, 2))
      .WillOnce(Return(0));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecOpen(&codec_context_, &codec_))
      .WillOnce(Return(0));
  EXPECT_CALL(host_, InitializationComplete());

  EXPECT_TRUE(decoder_->Initialize(demuxer_));
  message_loop_.RunAllPending();

  // Test that the output media format is an uncompressed video surface that
  // matches the dimensions specified by FFmpeg.
  const MediaFormat& media_format = decoder_->media_format();
  std::string mime_type;
  int width = 0;
  int height = 0;
  EXPECT_TRUE(media_format.GetAsString(MediaFormat::kMimeType, &mime_type));
  EXPECT_STREQ(mime_type::kUncompressedVideo, mime_type.c_str());
  EXPECT_TRUE(media_format.GetAsInteger(MediaFormat::kWidth, &width));
  EXPECT_EQ(kWidth, width);
  EXPECT_TRUE(media_format.GetAsInteger(MediaFormat::kHeight, &height));
  EXPECT_EQ(kHeight, height);
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_Normal) {
  // Expect a bunch of avcodec calls.
  EXPECT_CALL(mock_ffmpeg_, AVInitPacket(_));
  EXPECT_CALL(mock_ffmpeg_,
              AVCodecDecodeVideo2(&codec_context_, &yuv_frame_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(1),  // Simulate 1 byte frame.
                      Return(0)));

  scoped_refptr<FFmpegVideoDecoder> decoder = new FFmpegVideoDecoder();
  EXPECT_TRUE(decoder->DecodeFrame(*buffer_, &codec_context_, &yuv_frame_));
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_DiscontinuousBuffer) {
  buffer_->SetDiscontinuous(true);

  // Expect a bunch of avcodec calls.
  EXPECT_CALL(mock_ffmpeg_, AVCodecFlushBuffers(&codec_context_));
  EXPECT_CALL(mock_ffmpeg_, AVInitPacket(_));
  EXPECT_CALL(mock_ffmpeg_,
              AVCodecDecodeVideo2(&codec_context_, &yuv_frame_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(1),  // Simulate 1 byte frame.
                      Return(0)));

  scoped_refptr<FFmpegVideoDecoder> decoder = new FFmpegVideoDecoder();
  EXPECT_TRUE(decoder->DecodeFrame(*buffer_, &codec_context_, &yuv_frame_));
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_0ByteFrame) {
  // Expect a bunch of avcodec calls.
  EXPECT_CALL(mock_ffmpeg_, AVInitPacket(_));
  EXPECT_CALL(mock_ffmpeg_,
              AVCodecDecodeVideo2(&codec_context_, &yuv_frame_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(0),  // Simulate 0 byte frame.
                      Return(0)));

  scoped_refptr<FFmpegVideoDecoder> decoder = new FFmpegVideoDecoder();
  EXPECT_FALSE(decoder->DecodeFrame(*buffer_, &codec_context_, &yuv_frame_));
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_DecodeError) {
  // Expect a bunch of avcodec calls.
  EXPECT_CALL(mock_ffmpeg_, AVInitPacket(_));
  EXPECT_CALL(mock_ffmpeg_,
              AVCodecDecodeVideo2(&codec_context_, &yuv_frame_, _, _))
      .WillOnce(Return(-1));

  scoped_refptr<FFmpegVideoDecoder> decoder = new FFmpegVideoDecoder();
  EXPECT_FALSE(decoder->DecodeFrame(*buffer_, &codec_context_, &yuv_frame_));
}

TEST_F(FFmpegVideoDecoderTest, GetSurfaceFormat) {
  AVCodecContext context;
  scoped_refptr<FFmpegVideoDecoder> decoder = new FFmpegVideoDecoder();

  // YV12 formats.
  context.pix_fmt = PIX_FMT_YUV420P;
  EXPECT_EQ(VideoSurface::YV12, decoder->GetSurfaceFormat(context));
  context.pix_fmt = PIX_FMT_YUVJ420P;
  EXPECT_EQ(VideoSurface::YV12, decoder->GetSurfaceFormat(context));

  // YV16 formats.
  context.pix_fmt = PIX_FMT_YUV422P;
  EXPECT_EQ(VideoSurface::YV16, decoder->GetSurfaceFormat(context));
  context.pix_fmt = PIX_FMT_YUVJ422P;
  EXPECT_EQ(VideoSurface::YV16, decoder->GetSurfaceFormat(context));

  // Invalid value.
  context.pix_fmt = PIX_FMT_NONE;
  EXPECT_EQ(VideoSurface::INVALID, decoder->GetSurfaceFormat(context));
}

TEST_F(FFmpegVideoDecoderTest, FindPtsAndDuration) {
  // Start with an empty timestamp queue.
  FFmpegVideoDecoder::TimeQueue pts_queue;
  scoped_refptr<FFmpegVideoDecoder> decoder = new FFmpegVideoDecoder();

  // Use 1/2 second for simple results.  Thus, calculated Durations should be
  // 500000 microseconds.
  AVRational time_base = {1, 2};

  // Setup the last known pts to be at 100 microseconds with a have 16
  // duration.
  FFmpegVideoDecoder::TimeTuple last_pts;
  last_pts.timestamp = base::TimeDelta::FromMicroseconds(100);
  last_pts.duration = base::TimeDelta::FromMicroseconds(16);

  // Simulate an uninitialized yuv_frame.
  yuv_frame_.pts = AV_NOPTS_VALUE;
  FFmpegVideoDecoder::TimeTuple result_pts =
      decoder->FindPtsAndDuration(time_base, pts_queue, last_pts, &yuv_frame_);
  EXPECT_EQ(116, result_pts.timestamp.InMicroseconds());
  EXPECT_EQ(500000, result_pts.duration.InMicroseconds());

  // Test that providing no frame has the same result as an uninitialized
  // frame.
  result_pts = decoder->FindPtsAndDuration(time_base,
                                           pts_queue,
                                           last_pts,
                                           NULL);
  EXPECT_EQ(116, result_pts.timestamp.InMicroseconds());
  EXPECT_EQ(500000, result_pts.duration.InMicroseconds());

  // Test that having pts == 0 in the frame also behaves like the pts is not 
  // provided.  This is because FFmpeg set the pts to zero when there is no
  // data for the frame, which means that value is useless to us.
  yuv_frame_.pts = 0;
  result_pts =
      decoder->FindPtsAndDuration(time_base, pts_queue, last_pts, &yuv_frame_);
  EXPECT_EQ(116, result_pts.timestamp.InMicroseconds());
  EXPECT_EQ(500000, result_pts.duration.InMicroseconds());

  // Add a pts to the timequeue and make sure it overrides estimation.
  pts_queue.push(base::TimeDelta::FromMicroseconds(123));
  result_pts = decoder->FindPtsAndDuration(time_base,
                                           pts_queue,
                                           last_pts,
                                           &yuv_frame_);
  EXPECT_EQ(123, result_pts.timestamp.InMicroseconds());
  EXPECT_EQ(500000, result_pts.duration.InMicroseconds());

  // Add a pts into the frame and make sure it overrides the timequeue.
  yuv_frame_.pts = 333;
  yuv_frame_.repeat_pict = 2;
  result_pts = decoder->FindPtsAndDuration(time_base,
                                           pts_queue,
                                           last_pts,
                                           &yuv_frame_);
  EXPECT_EQ(166500000, result_pts.timestamp.InMicroseconds());
  EXPECT_EQ(1500000, result_pts.duration.InMicroseconds());
}

TEST_F(FFmpegVideoDecoderTest, OnDecode_TestStateTransition) {
  // Simulates a input sequence of three buffers, and six decode requests to
  // exercise the state transitions, and bookkeeping logic of OnDecode.
  //
  // We try to verify the following:
  //   1) Non-EoS buffer timestamps are pushed into the pts_queue.
  //   2) Timestamps are popped for each decoded frame.
  //   3) The last_pts_ is updated for each decoded frame.
  //   4) kDecodeFinished is never left regardless of what kind of buffer is
  //      given.
  //   5) All state transitions happen as expected.
  scoped_refptr<DecoderPrivateMock> mock_decoder =
      new StrictMock<DecoderPrivateMock>();

  // Setup decoder to buffer one frame, decode one frame, fail one frame,
  // decode one more, and then fail the last one to end decoding.
  EXPECT_CALL(*mock_decoder, DecodeFrame(_, _, _))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<2>(yuv_frame_), Return(true)))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<2>(yuv_frame_), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<2>(yuv_frame_), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_decoder, GetSurfaceFormat(_))
      .Times(3)
      .WillRepeatedly(Return(VideoSurface::YV16));
  EXPECT_CALL(*mock_decoder, EnqueueVideoFrame(_, _, _))
      .Times(3)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_decoder, FindPtsAndDuration(_, _, _, _))
      .WillOnce(Return(kTestPts1))
      .WillOnce(Return(kTestPts2))
      .WillOnce(Return(kTestPts1));
  EXPECT_CALL(*mock_decoder, EnqueueEmptyFrame())
      .Times(1);

  // Setup FFmpeg expectations for frame allocations.  We do
  // 6 decodes in this test.
  EXPECT_CALL(mock_ffmpeg_, AVCodecAllocFrame())
      .Times(6)
      .WillRepeatedly(Return(&yuv_frame_));
  EXPECT_CALL(mock_ffmpeg_, AVFree(&yuv_frame_))
      .Times(6);

  // Setup initial state and check that it is sane.
  mock_decoder->codec_context_ = &codec_context_;
  ASSERT_EQ(FFmpegVideoDecoder::kNormal, mock_decoder->state_);
  ASSERT_TRUE(base::TimeDelta() == mock_decoder->last_pts_.timestamp);
  ASSERT_TRUE(base::TimeDelta() == mock_decoder->last_pts_.duration);

  // Decode once, which should simulate a buffering call.
  mock_decoder->OnDecode(buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kNormal, mock_decoder->state_);
  ASSERT_TRUE(base::TimeDelta() == mock_decoder->last_pts_.timestamp);
  ASSERT_TRUE(base::TimeDelta() == mock_decoder->last_pts_.duration);
  EXPECT_EQ(1u, mock_decoder->pts_queue_.size());

  // Decode a second time, which should yield the first frame.
  mock_decoder->OnDecode(buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kNormal, mock_decoder->state_);
  EXPECT_TRUE(kTestPts1.timestamp == mock_decoder->last_pts_.timestamp);
  EXPECT_TRUE(kTestPts1.duration == mock_decoder->last_pts_.duration);
  EXPECT_EQ(1u, mock_decoder->pts_queue_.size());

  // Decode a third time, with a regular buffer.  The decode will error
  // out, but the state should be the same.
  mock_decoder->OnDecode(buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kNormal, mock_decoder->state_);
  EXPECT_TRUE(kTestPts1.timestamp == mock_decoder->last_pts_.timestamp);
  EXPECT_TRUE(kTestPts1.duration == mock_decoder->last_pts_.duration);
  EXPECT_EQ(2u, mock_decoder->pts_queue_.size());

  // Decode a fourth time, with an end of stream buffer.  This should
  // yield the second frame, and stay in flushing mode.
  mock_decoder->OnDecode(end_of_stream_buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kFlushCodec, mock_decoder->state_);
  EXPECT_TRUE(kTestPts2.timestamp == mock_decoder->last_pts_.timestamp);
  EXPECT_TRUE(kTestPts2.duration == mock_decoder->last_pts_.duration);
  EXPECT_EQ(1u, mock_decoder->pts_queue_.size());

  // Decode a fifth time with an end of stream buffer.  this should
  // yield the third frame.
  mock_decoder->OnDecode(end_of_stream_buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kFlushCodec, mock_decoder->state_);
  EXPECT_TRUE(kTestPts1.timestamp == mock_decoder->last_pts_.timestamp);
  EXPECT_TRUE(kTestPts1.duration == mock_decoder->last_pts_.duration);
  EXPECT_EQ(0u, mock_decoder->pts_queue_.size());

  // Decode a sixth time with an end of stream buffer.  This should
  // Move into kDecodeFinished.
  mock_decoder->OnDecode(end_of_stream_buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kDecodeFinished, mock_decoder->state_);
  EXPECT_TRUE(kTestPts1.timestamp == mock_decoder->last_pts_.timestamp);
  EXPECT_TRUE(kTestPts1.duration == mock_decoder->last_pts_.duration);
  EXPECT_EQ(0u, mock_decoder->pts_queue_.size());
}

TEST_F(FFmpegVideoDecoderTest, OnDecode_EnqueueVideoFrameError) {
  scoped_refptr<DecoderPrivateMock> mock_decoder =
      new StrictMock<DecoderPrivateMock>();

  // Setup decoder to decode one frame, but then fail on enqueue.
  EXPECT_CALL(*mock_decoder, DecodeFrame(_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(yuv_frame_), Return(true)));
  EXPECT_CALL(*mock_decoder, GetSurfaceFormat(_))
      .WillOnce(Return(VideoSurface::YV16));
  EXPECT_CALL(*mock_decoder, EnqueueVideoFrame(_, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_decoder, FindPtsAndDuration(_, _, _, _))
      .WillOnce(Return(kTestPts1));
  EXPECT_CALL(*mock_decoder, SignalPipelineError());

  // Setup FFmpeg expectations for frame allocations.
  EXPECT_CALL(mock_ffmpeg_, AVCodecAllocFrame())
      .WillOnce(Return(&yuv_frame_));
  EXPECT_CALL(mock_ffmpeg_, AVFree(&yuv_frame_));

  // Attempt the decode.
  buffer_->GetWritableData(1);
  mock_decoder->codec_context_ = &codec_context_;
  mock_decoder->OnDecode(buffer_);
}

TEST_F(FFmpegVideoDecoderTest, OnDecode_FinishEnqueuesEmptyFrames) {
  scoped_refptr<DecoderPrivateMock> mock_decoder =
      new StrictMock<DecoderPrivateMock>();

  // Move the decoder into the finished state for this test.
  mock_decoder->state_ = FFmpegVideoDecoder::kDecodeFinished;

  // Expect 2 calls, make two calls.
  EXPECT_CALL(*mock_decoder, EnqueueEmptyFrame()).Times(3);
  mock_decoder->OnDecode(NULL);
  mock_decoder->OnDecode(buffer_);
  mock_decoder->OnDecode(end_of_stream_buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kDecodeFinished, mock_decoder->state_);
}

TEST_F(FFmpegVideoDecoderTest, TimeQueue_Ordering) {
  FFmpegVideoDecoder::TimeQueue queue;
  queue.push(kTestPts1.timestamp);
  queue.push(kTestPts2.timestamp);
  queue.push(kTestPts1.timestamp);

  EXPECT_TRUE(kTestPts1.timestamp == queue.top());
  queue.pop();
  EXPECT_TRUE(kTestPts1.timestamp == queue.top());
  queue.pop();
  EXPECT_TRUE(kTestPts2.timestamp == queue.top());
  queue.pop();
}

}  // namespace media
