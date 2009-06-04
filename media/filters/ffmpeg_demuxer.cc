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
      stream_(stream),
      discontinuous_(false) {
  DCHECK(demuxer_);

  // Determine our media format.
  switch (stream->codec->codec_type) {
    case CODEC_TYPE_AUDIO:
      media_format_.SetAsString(MediaFormat::kMimeType,
                                mime_type::kFFmpegAudio);
      break;
    case CODEC_TYPE_VIDEO:
      media_format_.SetAsString(MediaFormat::kMimeType,
                                mime_type::kFFmpegVideo);
      break;
    default:
      NOTREACHED();
      break;
  }

  // Calculate the duration.
  duration_ = ConvertTimestamp(stream->duration);
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

base::TimeDelta FFmpegDemuxerStream::EnqueuePacket(AVPacket* packet) {
  base::TimeDelta timestamp = ConvertTimestamp(packet->pts);
  base::TimeDelta duration = ConvertTimestamp(packet->duration);
  Buffer* buffer = new AVPacketBuffer(packet, timestamp, duration);
  DCHECK(buffer);
  {
    AutoLock auto_lock(lock_);
    buffer_queue_.push_back(buffer);
  }
  FulfillPendingReads();
  return timestamp;
}

void FFmpegDemuxerStream::FlushBuffers() {
  AutoLock auto_lock(lock_);
  buffer_queue_.clear();
  discontinuous_ = true;
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
    demuxer_->PostDemuxTask();
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

      // Handle discontinuities due to FlushBuffers() being called.
      if (discontinuous_) {
        buffer->SetDiscontinuous(true);
        discontinuous_ = false;
      }
    }
    read_callback->Run(buffer);
  }
  return pending_reads;
}

base::TimeDelta FFmpegDemuxerStream::ConvertTimestamp(int64 timestamp) {
  AVRational time_base = { 1, base::Time::kMicrosecondsPerSecond };
  int64 microseconds = av_rescale_q(timestamp, stream_->time_base, time_base);
  return base::TimeDelta::FromMicroseconds(microseconds);
}


//
// FFmpegDemuxer
//
FFmpegDemuxer::FFmpegDemuxer()
    : thread_("DemuxerThread") {
}

FFmpegDemuxer::~FFmpegDemuxer() {
  Stop();
  // TODO(scherkus): I believe we need to use av_close_input_file() here
  // instead of scoped_ptr_malloc calling av_free().
  //
  // Note that av_close_input_file() doesn't close the codecs so we need to
  // figure out who's responsible for closing the them.
}

void FFmpegDemuxer::PostDemuxTask() {
  thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &FFmpegDemuxer::DemuxTask));
}

void FFmpegDemuxer::Stop() {
  thread_.Stop();
}

void FFmpegDemuxer::Seek(base::TimeDelta time) {
  // TODO(hclam): by returning from this method, it is assumed that the seek
  // operation is completed and filters behind the demuxer is good to issue
  // more reads, but we are posting a task here, which makes the seek operation
  // asynchronous, should change how seek works to make it fully asynchronous.
  thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &FFmpegDemuxer::SeekTask, time));
}

bool FFmpegDemuxer::Initialize(DataSource* data_source) {
  // Start our internal demuxing thread.
  if (!thread_.Start()) {
    host_->Error(DEMUXER_ERROR_COULD_NOT_CREATE_THREAD);
    return false;
  }

  thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &FFmpegDemuxer::InititalizeTask, data_source));
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

void FFmpegDemuxer::InititalizeTask(DataSource* data_source) {
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
  DCHECK(!format_context_.get());
  AVFormatContext* context = NULL;
  int result = av_open_input_file(&context, key.c_str(), NULL, 0, NULL);

  // Remove our data source.
  FFmpegGlue::get()->RemoveDataSource(data_source);

  if (result < 0) {
    host_->Error(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  // Assign to our scoped_ptr_malloc.
  DCHECK(context);
  format_context_.reset(context);

  // Fully initialize AVFormatContext by parsing the stream a little.
  result = av_find_stream_info(format_context_.get());
  if (result < 0) {
    host_->Error(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
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
      packet_streams_.push_back(demuxer_stream);
      max_duration = std::max(max_duration, demuxer_stream->duration());
    } else {
      packet_streams_.push_back(NULL);
    }
  }
  if (streams_.empty()) {
    host_->Error(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    return;
  }

  // Good to go: set the duration and notify we're done initializing.
  host_->SetDuration(max_duration);
  host_->InitializationComplete();
}

void FFmpegDemuxer::SeekTask(base::TimeDelta time) {
  // Tell streams to flush buffers due to seeking.
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    (*iter)->FlushBuffers();
  }

  // Seek backwards if requested timestamp is behind FFmpeg's current time.
  int flags = 0;
  if (time <= current_timestamp_) {
    flags |= AVSEEK_FLAG_BACKWARD;
  }

  if (av_seek_frame(format_context_.get(), -1, time.InMicroseconds(),
                    flags) < 0) {
    // TODO(scherkus): signal error.
    NOTIMPLEMENTED();
  }
}

void FFmpegDemuxer::DemuxTask() {
  // Make sure we have work to do before demuxing.
  if (!StreamsHavePendingReads()) {
    return;
  }

  // Allocate and read an AVPacket from the media.
  scoped_ptr<AVPacket> packet(new AVPacket());
  int result = av_read_frame(format_context_.get(), packet.get());
  if (result < 0) {
    // If we have reached the end of stream, tell the downstream filters about
    // the event.
    StreamHasEnded();
    return;
  }

  // Queue the packet with the appropriate stream.
  // TODO(scherkus): should we post this back to the pipeline thread?  I'm
  // worried about downstream filters (i.e., decoders) executing on this
  // thread.
  DCHECK(packet->stream_index >= 0);
  DCHECK(packet->stream_index < static_cast<int>(packet_streams_.size()));
  FFmpegDemuxerStream* demuxer_stream = packet_streams_[packet->stream_index];
  if (demuxer_stream) {
    // Duplicate the entire packet if we're dealing with MP3 due to an issue
    // where previously demuxed packets can become corrupted by simply demuxing
    // additional packets.
    //
    // TODO(scherkus): fix the MP3 packet copying hack.
    if (demuxer_stream->av_stream()->codec->codec_id == CODEC_ID_MP3) {
      scoped_ptr<AVPacket> clone(ClonePacket(packet.get()));
      if (!clone.get()) {
        NOTREACHED();
        return;
      }
      // Free FFmpeg-allocated memory and swap original packet into |clone| so
      // that it gets deleted as |clone| goes out of scope.
      av_free_packet(packet.get());
      packet.swap(clone);
    }

    // Queue the packet with the appropriate stream.  The stream takes
    // ownership of the AVPacket.
    current_timestamp_ = demuxer_stream->EnqueuePacket(packet.release());
  } else {
    av_free_packet(packet.get());
  }

  // Create a loop by posting another task.  This allows seek and message loop
  // quit tasks to get processed.
  if (StreamsHavePendingReads()) {
    PostDemuxTask();
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

void FFmpegDemuxer::StreamHasEnded() {
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    AVPacket* packet = new AVPacket();
    memset(packet, 0, sizeof(*packet));
    (*iter)->EnqueuePacket(packet);
  }
}

AVPacket* FFmpegDemuxer::ClonePacket(AVPacket* packet) {
  scoped_ptr<AVPacket> clone(new AVPacket());
  if (!clone.get() || av_new_packet(clone.get(), packet->size) < 0) {
    return NULL;
  }
  DCHECK_EQ(clone->size, packet->size);
  clone->dts = packet->dts;
  clone->pts = packet->pts;
  clone->duration = packet->duration;
  memcpy(clone->data, packet->data, clone->size);
  return clone.release();
}

}  // namespace media
