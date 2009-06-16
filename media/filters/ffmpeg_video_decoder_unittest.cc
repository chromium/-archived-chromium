// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/singleton.h"
#include "media/base/filters.h"
#include "media/base/mock_ffmpeg.h"
#include "media/base/mock_filter_host.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_interfaces.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::ReturnNull;

namespace media {

class MockDemuxerStream : public DemuxerStream, public AVStreamProvider {
 public:
  // DemuxerStream interface.
  MOCK_METHOD0(media_format, const MediaFormat&());
  MOCK_METHOD1(Read, void(Callback1<Buffer*>::Type* read_callback));
  MOCK_METHOD1(QueryInterface, void*(const char* interface_id));

  // AVStreamProvider interface.
  MOCK_METHOD0(GetAVStream, AVStream*());
};

}  // namespace media

namespace media {

// Fixture class to facilitate writing tests.  Takes care of setting up the
// FFmpeg, pipeline and filter host mocks.
class FFmpegVideoDecoderTest : public testing::Test {
 protected:
  static const int kWidth;
  static const int kHeight;

  FFmpegVideoDecoderTest() {
    MediaFormat media_format;
    media_format.SetAsString(MediaFormat::kMimeType, mime_type::kFFmpegVideo);

    // Create an FFmpegVideoDecoder.
    factory_ = FFmpegVideoDecoder::CreateFactory();
    decoder_ = factory_->Create<VideoDecoder>(media_format);
    DCHECK(decoder_);

    // Prepare a filter host, pipeline and demuxer for the video decoder.
    pipeline_.reset(new MockPipeline());
    filter_host_.reset(new MockFilterHost<VideoDecoder>(pipeline_.get(),
                                                        decoder_));
    demuxer_ = new MockDemuxerStream();

    // Initialize FFmpeg fixtures.
    memset(&stream_, 0, sizeof(stream_));
    memset(&codec_context_, 0, sizeof(codec_context_));
    memset(&codec_, 0, sizeof(codec_));
    stream_.codec = &codec_context_;
    codec_context_.width = kWidth;
    codec_context_.height = kHeight;

    // Initialize MockFFmpeg.
    MockFFmpeg::set(&mock_ffmpeg_);
  }

  virtual ~FFmpegVideoDecoderTest() {
    // Call Stop() to shut down internal threads.
    decoder_->Stop();

    // Reset MockFFmpeg.
    MockFFmpeg::set(NULL);
  }

  // Fixture members.
  scoped_refptr<FilterFactory> factory_;
  scoped_refptr<VideoDecoder> decoder_;
  scoped_ptr<MockPipeline> pipeline_;
  scoped_ptr<MockFilterHost<VideoDecoder> > filter_host_;
  scoped_refptr<MockDemuxerStream> demuxer_;

  // FFmpeg fixtures.
  AVStream stream_;
  AVCodecContext codec_context_;
  AVCodec codec_;
  MockFFmpeg mock_ffmpeg_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoderTest);
};

const int FFmpegVideoDecoderTest::kWidth = 1280;
const int FFmpegVideoDecoderTest::kHeight = 720;

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

  EXPECT_FALSE(decoder_->Initialize(demuxer_));
  EXPECT_TRUE(filter_host_->WaitForError(PIPELINE_ERROR_DECODE));
  EXPECT_FALSE(filter_host_->IsInitialized());
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

  EXPECT_FALSE(decoder_->Initialize(demuxer_));
  EXPECT_TRUE(filter_host_->WaitForError(PIPELINE_ERROR_DECODE));
  EXPECT_FALSE(filter_host_->IsInitialized());
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

  EXPECT_FALSE(decoder_->Initialize(demuxer_));
  EXPECT_TRUE(filter_host_->WaitForError(PIPELINE_ERROR_DECODE));
  EXPECT_FALSE(filter_host_->IsInitialized());
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

  EXPECT_FALSE(decoder_->Initialize(demuxer_));
  EXPECT_TRUE(filter_host_->WaitForError(PIPELINE_ERROR_DECODE));
  EXPECT_FALSE(filter_host_->IsInitialized());
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

  EXPECT_TRUE(decoder_->Initialize(demuxer_));
  EXPECT_TRUE(filter_host_->WaitForInitialized());
  EXPECT_TRUE(filter_host_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());

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

}  // namespace media
