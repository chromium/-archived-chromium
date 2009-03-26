// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_media_filters.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "testing/gtest/include/gtest/gtest.h"

// FFmpeg mocks to remove dependency on having the DLLs present.
extern "C" {
static const size_t kMaxStreams = 3;
static AVFormatContext g_format;
static AVStream g_streams[kMaxStreams];
static AVCodecContext g_audio_codec;
static AVCodecContext g_video_codec;
static AVCodecContext g_data_codec;
struct AVPacket g_packet;

// FFmpeg return codes for various functions.
static int g_av_open_input_file = 0;
static int g_av_find_stream_info = 0;
static int g_av_read_frame = 0;

// Counts the number of packets "allocated" by av_read_frame and "released" by
// av_free_packet.  This should always be zero after everything is cleaned up.
static int g_oustanding_packets = 0;

int av_open_input_file(AVFormatContext** format, const char* filename,
                       AVInputFormat* input_format, int buffer_size,
                       AVFormatParameters* parameters) {
  EXPECT_FALSE(input_format) << "AVInputFormat should be NULL.";
  EXPECT_FALSE(buffer_size) << "buffer_size should be 0.";
  EXPECT_FALSE(parameters) << "AVFormatParameters should be NULL.";
  if (g_av_open_input_file < 0) {
    *format = NULL;
  } else {
    *format = &g_format;
  }
  return g_av_open_input_file;
}

int av_find_stream_info(AVFormatContext* format) {
  EXPECT_EQ(&g_format, format);
  return g_av_find_stream_info;
}

void av_free(void* ptr) {
  EXPECT_EQ(&g_format, ptr);
}

// Our packet destroying function.
void DestructPacket(AVPacket* packet) {
  --g_oustanding_packets;
}

int av_read_frame(AVFormatContext* format, AVPacket* packet) {
  EXPECT_EQ(&g_format, format);
  memcpy(packet, &g_packet, sizeof(g_packet));
  packet->destruct = &DestructPacket;
  if (g_av_read_frame == 0) {
    ++g_oustanding_packets;
  }
  return g_av_read_frame;
}

}  // extern "C"

using namespace media;

namespace {

void InitializeFFmpegMocks() {
  // Initialize function return codes.
  g_av_open_input_file = 0;
  g_av_find_stream_info = 0;
  g_av_read_frame = 0;

  // Initialize AVFormatContext structure.
  memset(&g_format, 0, sizeof(g_format));

  // Initialize AVStream structures.
  for (size_t i = 0; i < kMaxStreams; ++i) {
    memset(&g_streams[i], 0, sizeof(g_streams[i]));
    g_streams[i].time_base.den = 1 * base::Time::kMicrosecondsPerSecond;
    g_streams[i].time_base.num = 1;
  }

  // Initialize AVCodexContext structures.
  memset(&g_audio_codec, 0, sizeof(g_audio_codec));
  g_audio_codec.codec_type = CODEC_TYPE_AUDIO;
  g_audio_codec.codec_id = CODEC_ID_VORBIS;
  g_audio_codec.channels = 2;
  g_audio_codec.sample_rate = 44100;

  memset(&g_video_codec, 0, sizeof(g_video_codec));
  g_video_codec.codec_type = CODEC_TYPE_VIDEO;
  g_video_codec.codec_id = CODEC_ID_THEORA;
  g_video_codec.height = 720;
  g_video_codec.width = 1280;

  memset(&g_data_codec, 0, sizeof(g_data_codec));
  g_data_codec.codec_type = CODEC_TYPE_DATA;
  g_data_codec.codec_id = CODEC_ID_NONE;

  // Initialize AVPacket structure.
  memset(&g_packet, 0, sizeof(g_packet));
}

// Simple implementation of Assignable<Buffer> that lets us poke at values.
class TestBuffer : public Assignable<Buffer> {
 public:
  TestBuffer() : assigned_(false) {}
  virtual ~TestBuffer() {}

  // Assignable<Buffer> implementation.
  virtual void SetBuffer(Buffer* buffer) {
    buffer_ = buffer;
  }

  void OnAssignment() {
    EXPECT_FALSE(assigned_);
    assigned_ = true;
  }

  // Mock getters/setters.
  Buffer* buffer() { return buffer_; }
  bool assigned() { return assigned_; }

 private:
  scoped_refptr<Buffer> buffer_;
  bool assigned_;
};

}  // namespace

TEST(FFmpegDemuxerTest, InitializeFailure) {
  InitializeFFmpegMocks();

  // Get FFmpegDemuxer's filter factory.
  scoped_refptr<FilterFactory> factory = FFmpegDemuxer::CreateFilterFactory();

  // Should only accept application/octet-stream type.
  MediaFormat media_format;
  media_format.SetAsString(MediaFormat::kMimeType, "foo/x-bar");
  scoped_refptr<Demuxer> demuxer(factory->Create<Demuxer>(&media_format));
  ASSERT_FALSE(demuxer);
  media_format.Clear();
  media_format.SetAsString(MediaFormat::kMimeType,
                           mime_type::kApplicationOctetStream);
  demuxer = factory->Create<Demuxer>(&media_format);
  ASSERT_TRUE(demuxer);

  // Prepare a filter host and data source for the demuxer.
  MockPipeline pipeline;
  scoped_ptr< MockFilterHost<Demuxer> > filter_host;
  filter_host.reset(new MockFilterHost<Demuxer>(&pipeline, demuxer));
  MockFilterConfig config;
  scoped_refptr<MockDataSource> data_source(new MockDataSource(&config));

  // Simulate av_open_input_fail failing.
  g_av_open_input_file = AVERROR_IO;
  g_av_find_stream_info = 0;
  EXPECT_FALSE(demuxer->Initialize(data_source));
  EXPECT_FALSE(filter_host->IsInitialized());
  EXPECT_EQ(DEMUXER_ERROR_COULD_NOT_OPEN, pipeline.GetError());

  // Simulate av_find_stream_info failing.
  g_av_open_input_file = 0;
  g_av_find_stream_info = AVERROR_IO;
  demuxer = factory->Create<Demuxer>(&media_format);
  filter_host.reset(new MockFilterHost<Demuxer>(&pipeline, demuxer));
  EXPECT_FALSE(demuxer->Initialize(data_source));
  EXPECT_FALSE(filter_host->IsInitialized());
  EXPECT_EQ(DEMUXER_ERROR_COULD_NOT_PARSE, pipeline.GetError());

  // Simulate media with no parseable streams.
  InitializeFFmpegMocks();
  demuxer = factory->Create<Demuxer>(&media_format);
  filter_host.reset(new MockFilterHost<Demuxer>(&pipeline, demuxer));
  EXPECT_FALSE(demuxer->Initialize(data_source));
  EXPECT_FALSE(filter_host->IsInitialized());
  EXPECT_EQ(DEMUXER_ERROR_NO_SUPPORTED_STREAMS, pipeline.GetError());

  // Simulate media with a data stream but no audio or video streams.
  g_format.nb_streams = 1;
  g_format.streams[0] = &g_streams[0];
  g_streams[0].codec = &g_data_codec;
  g_streams[0].duration = 10;
  demuxer = factory->Create<Demuxer>(&media_format);
  filter_host.reset(new MockFilterHost<Demuxer>(&pipeline, demuxer));
  EXPECT_FALSE(demuxer->Initialize(data_source));
  EXPECT_FALSE(filter_host->IsInitialized());
  EXPECT_EQ(DEMUXER_ERROR_NO_SUPPORTED_STREAMS, pipeline.GetError());
}

TEST(FFmpegDemuxerTest, InitializeStreams) {
  // Simulate media with a data stream, a video stream and audio stream.
  InitializeFFmpegMocks();
  g_format.nb_streams = 3;
  g_format.streams[0] = &g_streams[0];
  g_format.streams[1] = &g_streams[1];
  g_format.streams[2] = &g_streams[2];
  g_streams[0].duration = 1000;
  g_streams[0].codec = &g_data_codec;
  g_streams[1].duration = 100;
  g_streams[1].codec = &g_video_codec;
  g_streams[2].duration = 10;
  g_streams[2].codec = &g_audio_codec;

  // Create our pipeline.
  MockPipeline pipeline;

  // Create our data source.
  MockFilterConfig config;
  scoped_refptr<MockDataSource> data_source = new MockDataSource(&config);
  MockFilterHost<DataSource> filter_host_a(&pipeline, data_source);
  EXPECT_TRUE(data_source->Initialize("foo"));
  EXPECT_TRUE(filter_host_a.IsInitialized());

  // Create our demuxer.
  scoped_refptr<FilterFactory> factory = FFmpegDemuxer::CreateFilterFactory();
  scoped_refptr<Demuxer> demuxer
      = factory->Create<Demuxer>(data_source->GetMediaFormat());
  EXPECT_TRUE(demuxer);
  MockFilterHost<Demuxer> filter_host_b(&pipeline, demuxer);
  EXPECT_TRUE(demuxer->Initialize(data_source));
  EXPECT_TRUE(filter_host_b.IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline.GetError());

  // Since we ignore data streams, the duration should be equal to the video
  // stream's duration.
  EXPECT_EQ(g_streams[1].duration, pipeline.GetDuration().InMicroseconds());

  // Verify that 2 out of 3 streams were created.
  EXPECT_EQ(2, demuxer->GetNumberOfStreams());

  // First stream should be video.
  scoped_refptr<DemuxerStream> stream = demuxer->GetStream(0);
  ASSERT_TRUE(stream);
  const MediaFormat* stream_format = stream->GetMediaFormat();
  std::string mime_type;
  int result;
  EXPECT_TRUE(stream_format->GetAsString(MediaFormat::kMimeType, &mime_type));
  EXPECT_STREQ(mime_type::kFFmpegVideo, mime_type.c_str());
  EXPECT_TRUE(stream_format->GetAsInteger(kFFmpegCodecID, &result));
  EXPECT_EQ(CODEC_ID_THEORA, static_cast<CodecID>(result));
  EXPECT_TRUE(stream_format->GetAsInteger(MediaFormat::kHeight, &result));
  EXPECT_EQ(g_video_codec.height, result);
  EXPECT_TRUE(stream_format->GetAsInteger(MediaFormat::kWidth, &result));
  EXPECT_EQ(g_video_codec.width, result);

  // Second stream should be audio.
  stream = demuxer->GetStream(1);
  ASSERT_TRUE(stream);
  stream_format = stream->GetMediaFormat();
  EXPECT_TRUE(stream_format->GetAsString(MediaFormat::kMimeType, &mime_type));
  EXPECT_STREQ(mime_type::kFFmpegAudio, mime_type.c_str());
  EXPECT_TRUE(stream_format->GetAsInteger(kFFmpegCodecID, &result));
  EXPECT_EQ(CODEC_ID_VORBIS, static_cast<CodecID>(result));
  EXPECT_TRUE(stream_format->GetAsInteger(MediaFormat::kChannels, &result));
  EXPECT_EQ(g_audio_codec.channels, result);
  EXPECT_TRUE(stream_format->GetAsInteger(MediaFormat::kSampleRate, &result));
  EXPECT_EQ(g_audio_codec.sample_rate, result);
}

TEST(FFmpegDemuxerTest, Read) {
  // Prepare some test data.
  const int kAudio = 0;
  const int kVideo = 1;
  const size_t kDataSize = 4;
  uint8 audio_data[kDataSize] = {0, 1, 2, 3};
  uint8 video_data[kDataSize] = {4, 5, 6, 7};

  // Simulate media with a an audio stream and video stream.
  InitializeFFmpegMocks();
  g_format.nb_streams = 2;
  g_format.streams[kAudio] = &g_streams[kAudio];
  g_format.streams[kVideo] = &g_streams[kVideo];
  g_streams[kAudio].duration = 10;
  g_streams[kAudio].codec = &g_audio_codec;
  g_streams[kVideo].duration = 10;
  g_streams[kVideo].codec = &g_video_codec;

  // Create our pipeline.
  MockPipeline pipeline;

  // Create our data source.
  MockFilterConfig config;
  scoped_refptr<MockDataSource> data_source = new MockDataSource(&config);
  MockFilterHost<DataSource> filter_host_a(&pipeline, data_source);
  EXPECT_TRUE(data_source->Initialize("foo"));
  EXPECT_TRUE(filter_host_a.IsInitialized());

  // Create our demuxer.
  scoped_refptr<FilterFactory> factory = FFmpegDemuxer::CreateFilterFactory();
  scoped_refptr<Demuxer> demuxer
      = factory->Create<Demuxer>(data_source->GetMediaFormat());
  EXPECT_TRUE(demuxer);
  MockFilterHost<Demuxer> filter_host_b(&pipeline, demuxer);
  EXPECT_TRUE(demuxer->Initialize(data_source));
  EXPECT_TRUE(filter_host_b.IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline.GetError());

  // Verify both streams were created.
  EXPECT_EQ(2, demuxer->GetNumberOfStreams());

  // Get our streams.
  scoped_refptr<DemuxerStream> audio_stream = demuxer->GetStream(kAudio);
  scoped_refptr<DemuxerStream> video_stream = demuxer->GetStream(kVideo);
  ASSERT_TRUE(audio_stream);
  ASSERT_TRUE(video_stream);

  // Both streams should support FFmpegDemuxerStream interface.
  scoped_refptr<FFmpegDemuxerStream> ffmpeg_demuxer_stream;
  EXPECT_TRUE(audio_stream->QueryInterface(&ffmpeg_demuxer_stream));
  EXPECT_TRUE(ffmpeg_demuxer_stream);
  ffmpeg_demuxer_stream = NULL;
  EXPECT_TRUE(video_stream->QueryInterface(&ffmpeg_demuxer_stream));
  EXPECT_TRUE(ffmpeg_demuxer_stream);

  // Prepare our test audio packet.
  g_packet.stream_index = kAudio;
  g_packet.data = audio_data;
  g_packet.size = kDataSize;

  // Attempt a read from the audio stream and run the message loop until done.
  scoped_refptr<TestBuffer> buffer(new TestBuffer());
  audio_stream->Read(buffer);
  pipeline.RunAllTasks();
  EXPECT_TRUE(buffer->assigned());
  EXPECT_TRUE(buffer->buffer());
  EXPECT_EQ(audio_data, (uint8*)buffer->buffer()->GetData());
  EXPECT_EQ(kDataSize, buffer->buffer()->GetDataSize());

  // Prepare our test video packet.
  g_packet.stream_index = kVideo;
  g_packet.data = video_data;
  g_packet.size = kDataSize;

  // Attempt a read from the video stream and run the message loop until done.
  buffer = new TestBuffer();
  video_stream->Read(buffer);
  pipeline.RunAllTasks();
  EXPECT_TRUE(buffer->assigned());
  EXPECT_TRUE(buffer->buffer());
  EXPECT_EQ(video_data, (uint8*)buffer->buffer()->GetData());
  EXPECT_EQ(kDataSize, buffer->buffer()->GetDataSize());

  // Simulate end of stream.
  g_av_read_frame = AVERROR_IO;

  // Attempt a read from the audio stream and run the message loop until done.
  buffer = new TestBuffer();
  audio_stream->Read(buffer);
  pipeline.RunAllTasks();
  EXPECT_FALSE(buffer->assigned());
  EXPECT_FALSE(buffer->buffer());

  // Manually release buffer, which should release any remaining AVPackets.
  buffer = NULL;
  EXPECT_EQ(0, g_oustanding_packets);
}
