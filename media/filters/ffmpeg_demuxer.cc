// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/base/filter_host.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

//
// AVPacketBuffer
//
class AVPacketBuffer : public Buffer {
 public:
  AVPacketBuffer(AVPacket* packet, const base::TimeDelta& timestamp,
                 const base::TimeDelta& duration)
      : packet_(packet) {
    DCHECK(packet);
    SetTimestamp(timestamp);
    SetDuration(duration);
  }

  virtual ~AVPacketBuffer() {
    av_free_packet(packet_.get());
  }

  // Buffer implementation.
  virtual const uint8* GetData() const {
    return reinterpret_cast<const uint8*>(packet_->data);
  }

  virtual size_t GetDataSize() const {
    return static_cast<size_t>(packet_->size);
  }

 private:
  scoped_ptr<AVPacket> packet_;

  DISALLOW_COPY_AND_ASSIGN(AVPacketBuffer);
};


//
// FFmpegDemuxerStream
//
FFmpegDemuxerStream::FFmpegDemuxerStream(FFmpegDemuxer* demuxer,
                                         AVStream* stream)
    : demuxer_(demuxer),
      av_stream_(stream) {
  DCHECK(demuxer_);

  // Determine our media format.
  switch (stream->codec->codec_type) {
    case CODEC_TYPE_AUDIO:
      media_format_.SetAsString(MediaFormat::kMimeType,
                                mime_type::kFFmpegAudio);
      media_format_.SetAsInteger(MediaFormat::kChannels,
                                 stream->codec->channels);
      media_format_.SetAsInteger(MediaFormat::kSampleRate,
                                 stream->codec->sample_rate);
      media_format_.SetAsInteger(MediaFormat::kSampleBits,
                                 stream->codec->bits_per_raw_sample);
      break;
    case CODEC_TYPE_VIDEO:
      media_format_.SetAsString(MediaFormat::kMimeType,
                                mime_type::kFFmpegVideo);
      media_format_.SetAsInteger(MediaFormat::kHeight,
                                 stream->codec->height);
      media_format_.SetAsInteger(MediaFormat::kWidth,
                                 stream->codec->width);
      break;
    default:
      NOTREACHED();
      break;
  }
  int codec_id = static_cast<int>(stream->codec->codec_id);
  media_format_.SetAsInteger(kFFmpegCodecID, codec_id);

  // Calculate the time base and duration in microseconds.
  int64 time_base_us = static_cast<int64>(av_q2d(stream->time_base) *
      base::Time::kMicrosecondsPerSecond);
  int64 duration_us = static_cast<int64>(time_base_us * stream->duration);
  time_base_ = base::TimeDelta::FromMicroseconds(time_base_us);
  duration_ = base::TimeDelta::FromMicroseconds(duration_us);
}

FFmpegDemuxerStream::~FFmpegDemuxerStream() {
  // Since |buffer_queue_| uses scoped_refptr everything will get released.
  while (!read_queue_.empty()) {
    delete read_queue_.front();
    read_queue_.pop_front();
  }
}

// static
const char* FFmpegDemuxerStream::interface_id() {
  return interface_id::kFFmpegDemuxerStream;
}

void* FFmpegDemuxerStream::QueryInterface(const char* id) {
  DCHECK(id);
  FFmpegDemuxerStream* interface_ptr = NULL;
  if (0 == strcmp(id, interface_id())) {
    interface_ptr = this;
  }
  return interface_ptr;
}

bool FFmpegDemuxerStream::HasPendingReads() {
  AutoLock auto_lock(lock_);
  return !read_queue_.empty();
}

void FFmpegDemuxerStream::EnqueuePacket(AVPacket* packet) {
  base::TimeDelta timestamp = time_base_ * packet->pts;
  base::TimeDelta duration = time_base_ * packet->duration;
  Buffer* buffer = new AVPacketBuffer(packet, timestamp, duration);
  DCHECK(buffer);
  {
    AutoLock auto_lock(lock_);
    buffer_queue_.push_back(buffer);
  }
  FulfillPendingReads();
}

const MediaFormat& FFmpegDemuxerStream::media_format() {
  return media_format_;
}

void FFmpegDemuxerStream::Read(Callback1<Buffer*>::Type* read_callback) {
  DCHECK(read_callback);
  {
    AutoLock auto_lock(lock_);
    read_queue_.push_back(read_callback);
  }
  if (FulfillPendingReads()) {
    demuxer_->SignalDemux();
  }
}

bool FFmpegDemuxerStream::FulfillPendingReads() {
  bool pending_reads = false;
  while (true) {
    scoped_refptr<Buffer> buffer;
    scoped_ptr<Callback1<Buffer*>::Type> read_callback;
    {
      AutoLock auto_lock(lock_);
      pending_reads = !read_queue_.empty();
      if (buffer_queue_.empty() || read_queue_.empty()) {
        break;
      }
      buffer = buffer_queue_.front();
      read_callback.reset(read_queue_.front());
      buffer_queue_.pop_front();
      read_queue_.pop_front();
    }
    read_callback->Run(buffer);
  }
  return pending_reads;
}


//
// FFmpegDemuxer
//
FFmpegDemuxer::FFmpegDemuxer()
    : format_context_(NULL),
      thread_(NULL),
      wait_for_demux_(false, false),
      shutdown_(false) {
}

FFmpegDemuxer::~FFmpegDemuxer() {
  if (thread_) {
    shutdown_ = true;
    SignalDemux();
    PlatformThread::Join(thread_);
  }
  if (format_context_) {
    av_free(format_context_);
  }
}

void FFmpegDemuxer::SignalDemux() {
  wait_for_demux_.Signal();
}

void FFmpegDemuxer::Stop() {
  // TODO(scherkus): implement Stop().
  NOTIMPLEMENTED();
}

bool FFmpegDemuxer::Initialize(DataSource* data_source) {
  // In order to get FFmpeg to use |data_source| for file IO we must transfer
  // ownership via FFmpegGlue.  We'll add |data_source| to FFmpegGlue and pass
  // the resulting key to FFmpeg.  FFmpeg will pass the key to FFmpegGlue which
  // will take care of attaching |data_source| to an FFmpeg context.  After
  // we finish initializing the FFmpeg context we can remove |data_source| from
  // FFmpegGlue.
  //
  // Refer to media/filters/ffmpeg_glue.h for details.

  // Add our data source and get our unique key.
  std::string key = FFmpegGlue::get()->AddDataSource(data_source);

  // Open FFmpeg AVFormatContext.
  DCHECK(!format_context_);
  int result = av_open_input_file(&format_context_, key.c_str(), NULL, 0, NULL);

  // Remove our data source.
  FFmpegGlue::get()->RemoveDataSource(data_source);

  if (result < 0) {
    host_->Error(DEMUXER_ERROR_COULD_NOT_OPEN);
    return false;
  }

  // Fully initialize AVFormatContext by parsing the stream a little.
  result = av_find_stream_info(format_context_);
  if (result < 0) {
    host_->Error(DEMUXER_ERROR_COULD_NOT_PARSE);
    return false;
  }

  // Create demuxer streams for all supported streams.
  base::TimeDelta max_duration;
  for (size_t i = 0; i < format_context_->nb_streams; ++i) {
    CodecType codec_type = format_context_->streams[i]->codec->codec_type;
    if (codec_type == CODEC_TYPE_AUDIO || codec_type == CODEC_TYPE_VIDEO) {
      AVStream* stream = format_context_->streams[i];
      FFmpegDemuxerStream* demuxer_stream
          = new FFmpegDemuxerStream(this, stream);
      DCHECK(demuxer_stream);
      streams_.push_back(demuxer_stream);
      max_duration = std::max(max_duration, demuxer_stream->duration());
    }
  }
  if (streams_.empty()) {
    host_->Error(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    return false;
  }

  // We have some streams to demux so create our thread.
  if (!PlatformThread::Create(0, this, &thread_)) {
    host_->Error(DEMUXER_ERROR_COULD_NOT_CREATE_THREAD);
    return false;
  }

  // Good to go: set the duration and notify we're done initializing.
  host_->SetDuration(max_duration);
  host_->InitializationComplete();
  return true;
}

size_t FFmpegDemuxer::GetNumberOfStreams() {
  return streams_.size();
}

scoped_refptr<DemuxerStream> FFmpegDemuxer::GetStream(int stream) {
  DCHECK(stream >= 0);
  DCHECK(stream < static_cast<int>(streams_.size()));
  return streams_[stream].get();
}

void FFmpegDemuxer::ThreadMain() {
  PlatformThread::SetName("DemuxerThread");
  while (!shutdown_) {
    // Loop until we've satisfied every stream.
    while (StreamsHavePendingReads()) {
      // Allocate and read an AVPacket from the media.
      scoped_ptr<AVPacket> packet(new AVPacket());
      int result = av_read_frame(format_context_, packet.get());
      if (result < 0) {
        // TODO(scherkus): handle end of stream by marking Buffer with the end
        // of stream flag.
        NOTIMPLEMENTED();
        break;
      }

      // Queue the packet with the appropriate stream.
      // TODO(scherkus): should we post this back to the pipeline thread?  I'm
      // worried about downstream filters (i.e., decoders) executing on this
      // thread.
      DCHECK(packet->stream_index >= 0);
      DCHECK(packet->stream_index < static_cast<int>(streams_.size()));
      FFmpegDemuxerStream* demuxer_stream = streams_[packet->stream_index];
      demuxer_stream->EnqueuePacket(packet.release());
    }

    // Wait until we're signaled to either shutdown or satisfy more reads.
    wait_for_demux_.Wait();
  }
}

bool FFmpegDemuxer::StreamsHavePendingReads() {
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if ((*iter)->HasPendingReads()) {
      return true;
    }
  }
  return false;
}

}  // namespace media
