// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/singleton.h"
#include "base/tuple.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_media_filters.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Simulates a queue of media packets that get "demuxed" when av_read_frame()
// is called.  It also tracks the number of packets read but not released,
// which lets us test for memory leaks and handling seeks.
class PacketQueue : public Singleton<PacketQueue> {
 public:
  bool IsEmpty() {
    return packets_.empty();
  }

  void Enqueue(int stream, size_t size, uint8* data) {
    packets_.push_back(PacketTuple(stream, size, data));
  }

  void Dequeue(AVPacket* packet) {
    CHECK(!packets_.empty());
    memset(packet, 0, sizeof(*packet));
    packet->stream_index = packets_.front().a;
    packet->size = packets_.front().b;
    packet->data = packets_.front().c;
    packet->destruct = &PacketQueue::DestructPacket;
    packets_.pop_front();

    // We now have an outstanding packet which must be freed at some point.
    ++outstanding_packets_;
  }

  bool WaitForOutstandingPackets(int count) {
    const base::TimeDelta kTimedWait = base::TimeDelta::FromMilliseconds(500);
    while (outstanding_packets_ != count) {
      if (!wait_for_outstanding_packets_.TimedWait(kTimedWait)) {
        return false;
      }
    }
    return true;
  }

 private:
  static void DestructPacket(AVPacket* packet) {
    PacketQueue::get()->DestructPacket();
  }

  void DestructPacket() {
    --outstanding_packets_;
    wait_for_outstanding_packets_.Signal();
  }

  // Only allow Singleton to create and delete PacketQueue.
  friend struct DefaultSingletonTraits<PacketQueue>;

  PacketQueue()
      : outstanding_packets_(0),
        wait_for_outstanding_packets_(false, false) {
  }

  ~PacketQueue() {
    CHECK(outstanding_packets_ == 0);
  }

  // Packet queue for tests to enqueue mock packets, which are dequeued when
  // FFmpegDemuxer calls av_read_frame().
  typedef Tuple3<int, size_t, uint8*> PacketTuple;
  std::deque<PacketTuple> packets_;

  // Counts the number of packets "allocated" by av_read_frame() and "released"
  // by av_free_packet().  This should always be zero after everything is
  // cleaned up.
  int outstanding_packets_;

  // Tests can wait on this event until a specific number of outstanding packets
  // have been reached.  Used to ensure other threads release their references
  // to objects so we don't get false positive test results when comparing the
  // number of outstanding packets.
  base::WaitableEvent wait_for_outstanding_packets_;

  DISALLOW_COPY_AND_ASSIGN(PacketQueue);
};

}  // namespace

// FFmpeg mocks to remove dependency on having the DLLs present.
extern "C" {
static const size_t kMaxStreams = 3;
static AVFormatContext g_format;
static AVStream g_streams[kMaxStreams];
static AVCodecContext g_audio_codec;
static AVCodecContext g_video_codec;
static AVCodecContext g_data_codec;

// FFmpeg return codes for various functions.
static int g_av_open_input_file = 0;
static int g_av_find_stream_info = 0;
static int g_av_read_frame = 0;
static int g_av_seek_frame = 0;

// Expected values when seeking.
static base::WaitableEvent* g_seek_event = NULL;
static int64_t g_expected_seek_timestamp = 0;
static int g_expected_seek_flags = 0;

// Counts outstanding packets allocated by av_new_frame().
static int g_outstanding_packets_av_new_frame = 0;

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

int64 av_rescale_q(int64 a, AVRational bq, AVRational cq) {
  int64 num = bq.num * cq.den;
  int64 den = cq.num * bq.den;
  return a * num / den;
}

void av_free_packet(AVPacket* packet) {
  if (packet->destruct) {
    packet->destruct(packet);
    packet->data = NULL;
    packet->size = 0;
  }
}

void DestructPacket(AVPacket* packet) {
  delete [] packet->data;
  --g_outstanding_packets_av_new_frame;
}

int av_new_packet(AVPacket* packet, int size) {
  memset(packet, 0, sizeof(*packet));
  packet->data = new uint8[size];
  packet->size = size;
  packet->destruct = &DestructPacket;
  ++g_outstanding_packets_av_new_frame;
  return 0;
}

void av_free(void* ptr) {
  if (ptr) {
    EXPECT_EQ(&g_format, ptr);
  }
}

int av_read_frame(AVFormatContext* format, AVPacket* packet) {
  EXPECT_EQ(&g_format, format);
  if (g_av_read_frame == 0) {
    PacketQueue::get()->Dequeue(packet);
  }
  return g_av_read_frame;
}

int av_seek_frame(AVFormatContext *format, int stream_index, int64_t timestamp,
                  int flags) {
  EXPECT_EQ(&g_format, format);
  EXPECT_EQ(-1, stream_index);  // Should always use -1 for default stream.
  EXPECT_EQ(g_expected_seek_timestamp, timestamp);
  EXPECT_EQ(g_expected_seek_flags, flags);
  EXPECT_FALSE(g_seek_event->IsSignaled());
  g_seek_event->Signal();
  return g_av_seek_frame;
}

}  // extern "C"

using namespace media;

namespace {

// Ref counted object so we can create callbacks to call DemuxerStream::Read().
class TestReader : public base::RefCountedThreadSafe<TestReader> {
 public:
  TestReader()
      : called_(false),
        expecting_call_(false),
        wait_for_read_(false, false) {
  }

  virtual ~TestReader() {}

  void Reset() {
    EXPECT_FALSE(expecting_call_);
    expecting_call_ = false;
    called_ = false;
    buffer_ = NULL;
    wait_for_read_.Reset();
  }

  void Read(DemuxerStream* stream) {
    EXPECT_FALSE(expecting_call_);
    called_ = false;
    expecting_call_ = true;
    stream->Read(NewCallback(this, &TestReader::ReadComplete));
  }

  void ReadComplete(Buffer* buffer) {
    EXPECT_FALSE(called_);
    EXPECT_TRUE(expecting_call_);
    expecting_call_ = false;
    called_ = true;
    buffer_ = buffer;
    wait_for_read_.Signal();
  }

  bool WaitForRead() {
    return wait_for_read_.TimedWait(base::TimeDelta::FromMilliseconds(500));
  }

  // Mock getters/setters.
  Buffer* buffer() { return buffer_; }
  bool called() { return called_; }
  bool expecting_call() { return expecting_call_; }

 private:
  scoped_refptr<Buffer> buffer_;
  bool called_;
  bool expecting_call_;
  base::WaitableEvent wait_for_read_;
};

// Fixture class to facilitate writing tests.  Takes care of setting up the
// FFmpeg, pipeline and filter host mocks.
class FFmpegDemuxerTest : public testing::Test {
 protected:
  FFmpegDemuxerTest() {}
  virtual ~FFmpegDemuxerTest() {}

  virtual void SetUp() {
    InitializeFFmpegMocks();

    // Create an FFmpegDemuxer.
    factory_ = FFmpegDemuxer::CreateFilterFactory();
    MediaFormat media_format;
    media_format.SetAsString(MediaFormat::kMimeType,
                             mime_type::kApplicationOctetStream);
    demuxer_ = factory_->Create<Demuxer>(media_format);
    DCHECK(demuxer_);

    // Prepare a filter host and data source for the demuxer.
    pipeline_.reset(new MockPipeline());
    filter_host_.reset(new MockFilterHost<Demuxer>(pipeline_.get(), demuxer_));
    MockFilterConfig config;
    data_source_ = new MockDataSource(&config);
  }

  virtual void TearDown() {
    // Call Stop() to shut down internal threads.
    demuxer_->Stop();
  }

  // Fixture members.
  scoped_refptr<FilterFactory> factory_;
  scoped_refptr<Demuxer> demuxer_;
  scoped_ptr<MockPipeline> pipeline_;
  scoped_ptr< MockFilterHost<Demuxer> > filter_host_;
  scoped_refptr<MockDataSource> data_source_;

 private:
  static void InitializeFFmpegMocks() {
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
  }

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerTest);
};

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

TEST_F(FFmpegDemuxerTest, InitializeCouldNotOpen) {
  // Simulate av_open_input_fail failing.
  g_av_open_input_file = AVERROR_IO;
  g_av_find_stream_info = 0;
  EXPECT_TRUE(demuxer_->Initialize(data_source_.get()));
  EXPECT_TRUE(filter_host_->WaitForError(DEMUXER_ERROR_COULD_NOT_OPEN));
  EXPECT_FALSE(filter_host_->IsInitialized());
}

TEST_F(FFmpegDemuxerTest, InitializeCouldNotParse) {
  // Simulate av_find_stream_info failing.
  g_av_open_input_file = 0;
  g_av_find_stream_info = AVERROR_IO;

  EXPECT_TRUE(demuxer_->Initialize(data_source_.get()));
  EXPECT_TRUE(filter_host_->WaitForError(DEMUXER_ERROR_COULD_NOT_PARSE));
  EXPECT_FALSE(filter_host_->IsInitialized());
}

TEST_F(FFmpegDemuxerTest, InitializeNoStreams) {
  // Simulate media with no parseable streams.
  EXPECT_TRUE(demuxer_->Initialize(data_source_.get()));
  EXPECT_TRUE(filter_host_->WaitForError(DEMUXER_ERROR_NO_SUPPORTED_STREAMS));
  EXPECT_FALSE(filter_host_->IsInitialized());
}

TEST_F(FFmpegDemuxerTest, InitializeDataStreamOnly) {
  // Simulate media with a data stream but no audio or video streams.
  g_format.nb_streams = 1;
  g_format.streams[0] = &g_streams[0];
  g_streams[0].codec = &g_data_codec;
  g_streams[0].duration = 10;

  EXPECT_TRUE(demuxer_->Initialize(data_source_.get()));
  EXPECT_TRUE(filter_host_->WaitForError(DEMUXER_ERROR_NO_SUPPORTED_STREAMS));
  EXPECT_FALSE(filter_host_->IsInitialized());
}

TEST_F(FFmpegDemuxerTest, InitializeStreams) {
  // Simulate media with a data stream, a video stream and audio stream.
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

  // Initialize the demuxer.
  EXPECT_TRUE(demuxer_->Initialize(data_source_.get()));
  EXPECT_TRUE(filter_host_->WaitForInitialized());
  EXPECT_TRUE(filter_host_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());

  // Since we ignore data streams, the duration should be equal to the video
  // stream's duration.
  EXPECT_EQ(g_streams[1].duration, pipeline_->GetDuration().InMicroseconds());

  // Verify that 2 out of 3 streams were created.
  EXPECT_EQ(2, demuxer_->GetNumberOfStreams());

  // First stream should be video and support FFmpegDemuxerStream interface.
  scoped_refptr<DemuxerStream> stream = demuxer_->GetStream(0);
  scoped_refptr<FFmpegDemuxerStream> ffmpeg_demuxer_stream;
  ASSERT_TRUE(stream);
  std::string mime_type;
  EXPECT_TRUE(
      stream->media_format().GetAsString(MediaFormat::kMimeType, &mime_type));
  EXPECT_STREQ(mime_type::kFFmpegVideo, mime_type.c_str());
  EXPECT_TRUE(stream->QueryInterface(&ffmpeg_demuxer_stream));
  EXPECT_TRUE(ffmpeg_demuxer_stream);
  EXPECT_EQ(&g_streams[1], ffmpeg_demuxer_stream->av_stream());

  // Second stream should be audio and support FFmpegDemuxerStream interface.
  stream = demuxer_->GetStream(1);
  ffmpeg_demuxer_stream = NULL;
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->media_format().GetAsString(MediaFormat::kMimeType,
              &mime_type));
  EXPECT_STREQ(mime_type::kFFmpegAudio, mime_type.c_str());
  EXPECT_TRUE(stream->QueryInterface(&ffmpeg_demuxer_stream));
  EXPECT_TRUE(ffmpeg_demuxer_stream);
  EXPECT_EQ(&g_streams[2], ffmpeg_demuxer_stream->av_stream());
}

TEST_F(FFmpegDemuxerTest, ReadAndSeek) {
  // Prepare some test data.
  const int kPacketData = 0;
  const int kPacketAudio = 1;
  const int kPacketVideo = 2;
  const int kAudio = 0;
  const int kVideo = 1;
  const size_t kDataSize = 4;
  uint8 audio_data[kDataSize] = {0, 1, 2, 3};
  uint8 video_data[kDataSize] = {4, 5, 6, 7};

  // Simulate media with a data stream, audio stream and video stream.  Having
  // the data stream first forces the audio and video streams to get remapped
  // from indices {1,2} to {0,1} respectively, which covers an important test
  // case.
  g_format.nb_streams = 3;
  g_format.streams[kPacketData] = &g_streams[0];
  g_format.streams[kPacketAudio] = &g_streams[1];
  g_format.streams[kPacketVideo] = &g_streams[2];
  g_streams[0].duration = 10;
  g_streams[0].codec = &g_data_codec;
  g_streams[1].duration = 10;
  g_streams[1].codec = &g_audio_codec;
  g_streams[2].duration = 10;
  g_streams[2].codec = &g_video_codec;

  // Initialize the demuxer.
  EXPECT_TRUE(demuxer_->Initialize(data_source_.get()));
  EXPECT_TRUE(filter_host_->WaitForInitialized());
  EXPECT_TRUE(filter_host_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());

  // Verify both streams were created.
  EXPECT_EQ(2, demuxer_->GetNumberOfStreams());

  // Get our streams.
  scoped_refptr<DemuxerStream> audio_stream = demuxer_->GetStream(kAudio);
  scoped_refptr<DemuxerStream> video_stream = demuxer_->GetStream(kVideo);
  ASSERT_TRUE(audio_stream);
  ASSERT_TRUE(video_stream);

  // Prepare data packets, which should all get immediately released.
  PacketQueue::get()->Enqueue(kPacketData, kDataSize, audio_data);
  PacketQueue::get()->Enqueue(kPacketData, kDataSize, audio_data);
  PacketQueue::get()->Enqueue(kPacketData, kDataSize, audio_data);

  // Prepare our test audio packet.
  PacketQueue::get()->Enqueue(kPacketAudio, kDataSize, audio_data);

  // Attempt a read from the audio stream and run the message loop until done.
  scoped_refptr<TestReader> reader(new TestReader());
  reader->Read(audio_stream);
  pipeline_->RunAllTasks();
  EXPECT_TRUE(reader->WaitForRead());
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_FALSE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(audio_data, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // Prepare our test video packet.
  PacketQueue::get()->Enqueue(kPacketVideo, kDataSize, video_data);

  // Attempt a read from the video stream and run the message loop until done.
  reader->Reset();
  reader->Read(video_stream);
  pipeline_->RunAllTasks();
  EXPECT_TRUE(reader->WaitForRead());
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_FALSE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(video_data, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // Manually release buffer, which should release any remaining AVPackets.
  reader = NULL;
  EXPECT_TRUE(PacketQueue::get()->WaitForOutstandingPackets(0));

  //----------------------------------------------------------------------------
  // Seek tests.
  EXPECT_FALSE(g_seek_event);
  g_seek_event = new base::WaitableEvent(false, false);

  // Let's trigger a simple forward seek with no outstanding packets.
  g_expected_seek_timestamp = 1234;
  g_expected_seek_flags = 0;
  demuxer_->Seek(base::TimeDelta::FromMicroseconds(g_expected_seek_timestamp));
  EXPECT_TRUE(g_seek_event->TimedWait(base::TimeDelta::FromSeconds(1)));

  // The next read from each stream should now be discontinuous, but subsequent
  // reads should not.

  // Prepare our test audio packet.
  PacketQueue::get()->Enqueue(kPacketAudio, kDataSize, audio_data);
  PacketQueue::get()->Enqueue(kPacketAudio, kDataSize, audio_data);

  // Audio read #1, should be discontinuous.
  reader = new TestReader();
  reader->Read(audio_stream);
  pipeline_->RunAllTasks();
  EXPECT_TRUE(reader->WaitForRead());
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_TRUE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(audio_data, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // Audio read #2, should not be discontinuous.
  reader->Reset();
  reader->Read(audio_stream);
  pipeline_->RunAllTasks();
  EXPECT_TRUE(reader->WaitForRead());
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_FALSE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(audio_data, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // Prepare our test video packet.
  PacketQueue::get()->Enqueue(kPacketVideo, kDataSize, video_data);
  PacketQueue::get()->Enqueue(kPacketVideo, kDataSize, video_data);

  // Video read #1, should be discontinuous.
  reader->Reset();
  reader->Read(video_stream);
  pipeline_->RunAllTasks();
  EXPECT_TRUE(reader->WaitForRead());
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_TRUE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(video_data, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // Video read #2, should not be discontinuous.
  reader->Reset();
  reader->Read(video_stream);
  pipeline_->RunAllTasks();
  EXPECT_TRUE(reader->WaitForRead());
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_FALSE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(video_data, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // Manually release buffer, which should release any remaining AVPackets.
  reader = NULL;
  EXPECT_TRUE(PacketQueue::get()->WaitForOutstandingPackets(0));

  // Let's trigger another simple forward seek, but with outstanding packets.
  // The outstanding packets should get freed after the Seek() is issued.
  PacketQueue::get()->Enqueue(kPacketAudio, kDataSize, audio_data);
  PacketQueue::get()->Enqueue(kPacketAudio, kDataSize, audio_data);
  PacketQueue::get()->Enqueue(kPacketAudio, kDataSize, audio_data);
  PacketQueue::get()->Enqueue(kPacketVideo, kDataSize, video_data);

  // Attempt a read from video stream, which will force the demuxer to queue
  // the audio packets preceding the video packet.
  reader = new TestReader();
  reader->Read(video_stream);
  pipeline_->RunAllTasks();
  EXPECT_TRUE(reader->WaitForRead());
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_FALSE(reader->buffer()->IsDiscontinuous());
  EXPECT_EQ(video_data, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());

  // Manually release video buffer, remaining audio packets are outstanding.
  reader = NULL;
  EXPECT_TRUE(PacketQueue::get()->WaitForOutstandingPackets(3));

  // Trigger the seek.
  g_expected_seek_timestamp = 1234;
  g_expected_seek_flags = 0;
  demuxer_->Seek(base::TimeDelta::FromMicroseconds(g_expected_seek_timestamp));
  EXPECT_TRUE(g_seek_event->TimedWait(base::TimeDelta::FromSeconds(1)));

  // All outstanding packets should have been freed.
  EXPECT_TRUE(PacketQueue::get()->WaitForOutstandingPackets(0));

  // Clean up.
  delete g_seek_event;
  g_seek_event = NULL;

  //----------------------------------------------------------------------------
  // End of stream tests.

  // Simulate end of stream.
  g_av_read_frame = AVERROR_IO;

  // Attempt a read from the audio stream and run the message loop until done.
  reader = new TestReader();
  reader->Read(audio_stream);
  pipeline_->RunAllTasks();
  EXPECT_FALSE(reader->WaitForRead());
  EXPECT_FALSE(reader->called());
  EXPECT_FALSE(reader->buffer());

  // Manually release buffer, which should release any remaining AVPackets.
  reader = NULL;
  EXPECT_TRUE(PacketQueue::get()->WaitForOutstandingPackets(0));
}

// This tests our deep-copying workaround for FFmpeg's MP3 demuxer.  When we fix
// the root cause this test will fail and should be removed.
TEST_F(FFmpegDemuxerTest, MP3Hack) {
  // Prepare some test data.
  const int kPacketAudio = 0;  // Stream index relative to the container.
  const int kAudio = 0;        // Stream index relative to Demuxer::GetStream().
  const size_t kDataSize = 4;
  uint8 audio_data[kDataSize] = {0, 1, 2, 3};

  // Simulate media with a single MP3 audio stream.
  g_format.nb_streams = 1;
  g_format.streams[kPacketAudio] = &g_streams[0];
  g_streams[0].duration = 10;
  g_streams[0].codec = &g_audio_codec;
  g_audio_codec.codec_id = CODEC_ID_MP3;

  // Initialize the demuxer.
  EXPECT_TRUE(demuxer_->Initialize(data_source_.get()));
  EXPECT_TRUE(filter_host_->WaitForInitialized());
  EXPECT_TRUE(filter_host_->IsInitialized());
  EXPECT_EQ(PIPELINE_OK, pipeline_->GetError());

  // Verify the stream was created.
  EXPECT_EQ(1, demuxer_->GetNumberOfStreams());
  scoped_refptr<DemuxerStream> audio_stream = demuxer_->GetStream(kAudio);
  ASSERT_TRUE(audio_stream);

  // Prepare our test audio packet.
  PacketQueue::get()->Enqueue(kPacketAudio, kDataSize, audio_data);

  // Audio read should perform a deep copy on the packet and instantly release
  // the original packet.  The data pointers should not be the same, but the
  // contents should match.
  scoped_refptr<TestReader> reader = new TestReader();
  reader->Read(audio_stream);
  pipeline_->RunAllTasks();
  EXPECT_TRUE(reader->WaitForRead());
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_FALSE(reader->buffer()->IsDiscontinuous());
  EXPECT_NE(audio_data, reader->buffer()->GetData());
  EXPECT_EQ(kDataSize, reader->buffer()->GetDataSize());
  EXPECT_EQ(0, memcmp(audio_data, reader->buffer()->GetData(), kDataSize));

  // Original AVPacket from the queue should have been released due to copying.
  EXPECT_TRUE(PacketQueue::get()->WaitForOutstandingPackets(0));
  EXPECT_EQ(1, g_outstanding_packets_av_new_frame);

  // Now release our reference, which should destruct the packet allocated by
  // av_new_packet().
  reader = NULL;
  EXPECT_EQ(0, g_outstanding_packets_av_new_frame);
}

}  // namespace
