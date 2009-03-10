// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  virtual const char* GetData() const {
    return reinterpret_cast<const char*>(packet_->data);
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
                                         const AVStream& stream)
    : demuxer_(demuxer) {
  DCHECK(demuxer_);

  // Determine our media format.
  switch (stream.codec->codec_type) {
    case CODEC_TYPE_AUDIO:
      media_format_.SetAsString(MediaFormat::kMimeType,
                                mime_type::kFFmpegAudio);
      media_format_.SetAsInteger(MediaFormat::kChannels,
                                 stream.codec->channels);
      media_format_.SetAsInteger(MediaFormat::kSampleRate,
                                 stream.codec->sample_rate);
      break;
    case CODEC_TYPE_VIDEO:
      media_format_.SetAsString(MediaFormat::kMimeType,
                                mime_type::kFFmpegVideo);
      media_format_.SetAsInteger(MediaFormat::kHeight,
                                 stream.codec->height);
      media_format_.SetAsInteger(MediaFormat::kWidth,
                                 stream.codec->width);
      break;
    default:
      NOTREACHED();
      break;
  }
  int codec_id = static_cast<int>(stream.codec->codec_id);
  media_format_.SetAsInteger(kFFmpegCodecID, codec_id);

  // Calculate the time base and duration in microseconds.
  int64 time_base_us = static_cast<int64>(av_q2d(stream.time_base) *
      base::Time::kMicrosecondsPerSecond);
  int64 duration_us = static_cast<int64>(time_base_us * stream.duration);
  time_base_ = base::TimeDelta::FromMicroseconds(time_base_us);
  duration_ = base::TimeDelta::FromMicroseconds(duration_us);
}

FFmpegDemuxerStream::~FFmpegDemuxerStream() {
  // Since |input_queue_| and |output_queue_| use scoped_refptr everything
  // should get released.
}

bool FFmpegDemuxerStream::HasPendingReads() {
  AutoLock auto_lock(lock_);
  return !output_queue_.empty();
}

void FFmpegDemuxerStream::EnqueuePacket(AVPacket* packet) {
  base::TimeDelta timestamp = time_base_ * packet->pts;
  base::TimeDelta duration = time_base_ * packet->duration;
  Buffer* buffer = new AVPacketBuffer(packet, timestamp, duration);
  DCHECK(buffer);
  {
    AutoLock auto_lock(lock_);
    input_queue_.push_back(buffer);
  }
  FulfillPendingReads();
}

const MediaFormat* FFmpegDemuxerStream::GetMediaFormat() {
  return &media_format_;
}

void FFmpegDemuxerStream::Read(Assignable<Buffer>* buffer) {
  DCHECK(buffer);
  {
    AutoLock auto_lock(lock_);
    output_queue_.push_back(scoped_refptr< Assignable<Buffer> >(buffer));
  }
  if (FulfillPendingReads()) {
    demuxer_->ScheduleDemux();
  }
}

bool FFmpegDemuxerStream::FulfillPendingReads() {
  bool pending_reads = false;
  while (true) {
    scoped_refptr<Buffer> buffer_in;
    scoped_refptr< Assignable<Buffer> > buffer_out;
    {
      AutoLock auto_lock(lock_);
      pending_reads = !output_queue_.empty();
      if (input_queue_.empty() || output_queue_.empty()) {
        break;
      }
      buffer_in = input_queue_.front();
      buffer_out = output_queue_.front();
      input_queue_.pop_front();
      output_queue_.pop_front();
    }
    buffer_out->SetBuffer(buffer_in);
    buffer_out->OnAssignment();
  }
  return pending_reads;
}


//
// FFmpegDemuxer
//
FFmpegDemuxer::FFmpegDemuxer()
    : demuxing_(false),
      format_context_(NULL) {
}

FFmpegDemuxer::~FFmpegDemuxer() {
  if (format_context_) {
    av_free(format_context_);
  }
  while (!streams_.empty()) {
    delete streams_.back();
    streams_.pop_back();
  }
}

void FFmpegDemuxer::ScheduleDemux() {
  if (!demuxing_) {
    demuxing_ = true;
    host_->PostTask(NewRunnableMethod(this, &FFmpegDemuxer::Demux));
  }
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
          = new FFmpegDemuxerStream(this, *stream);
      DCHECK(demuxer_stream);
      streams_.push_back(demuxer_stream);
      max_duration = std::max(max_duration, demuxer_stream->duration());
    }
  }
  if (streams_.empty()) {
    host_->Error(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    return false;
  }

  // We have at least one supported stream, set the duration and notify we're
  // done initializing.
  host_->SetDuration(max_duration);
  host_->InitializationComplete();
  return true;
}

size_t FFmpegDemuxer::GetNumberOfStreams() {
  return streams_.size();
}

DemuxerStream* FFmpegDemuxer::GetStream(int stream) {
  DCHECK(stream >= 0);
  DCHECK(stream < static_cast<int>(streams_.size()));
  return streams_[stream];
}

void FFmpegDemuxer::Demux() {
  DCHECK(demuxing_);

  // Loop until we've satisfied every stream.
  while (StreamsHavePendingReads()) {
    // Allocate and read an AVPacket from the media.
    scoped_ptr<AVPacket> packet(new AVPacket());
    int result = av_read_frame(format_context_, packet.get());
    if (result < 0) {
      // TODO(scherkus): handle end of stream by marking Buffer with the end of
      // stream flag.
      NOTIMPLEMENTED();
      break;
    }

    // Queue the packet with the appropriate stream.
    DCHECK(packet->stream_index >= 0);
    DCHECK(packet->stream_index < static_cast<int>(streams_.size()));
    FFmpegDemuxerStream* demuxer_stream = streams_[packet->stream_index];
    demuxer_stream->EnqueuePacket(packet.release());
  }

  // Finished demuxing.
  demuxing_ = false;
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
