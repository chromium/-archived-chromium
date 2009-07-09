// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/base/filter_host.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_glue.h"

namespace {

// Helper function to deep copy an AVPacket's data, size and timestamps.
// Returns NULL if a packet could not be cloned (i.e., out of memory).
AVPacket* ClonePacket(AVPacket* packet) {
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

}  // namespace

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
      discontinuous_(false),
      stopped_(false) {
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
  DCHECK(stopped_);
  DCHECK(read_queue_.empty());
  DCHECK(buffer_queue_.empty());
}

void* FFmpegDemuxerStream::QueryInterface(const char* id) {
  DCHECK(id);
  AVStreamProvider* interface_ptr = NULL;
  if (0 == strcmp(id, AVStreamProvider::interface_id())) {
    interface_ptr = this;
  }
  return interface_ptr;
}

bool FFmpegDemuxerStream::HasPendingReads() {
  DCHECK_EQ(PlatformThread::CurrentId(), demuxer_->thread_id_);
  DCHECK(!stopped_ || read_queue_.empty())
      << "Read queue should have been emptied if demuxing stream is stopped";
  return !read_queue_.empty();
}

base::TimeDelta FFmpegDemuxerStream::EnqueuePacket(AVPacket* packet) {
  DCHECK_EQ(PlatformThread::CurrentId(), demuxer_->thread_id_);
  base::TimeDelta timestamp = ConvertTimestamp(packet->pts);
  base::TimeDelta duration = ConvertTimestamp(packet->duration);
  if (stopped_) {
    NOTREACHED() << "Attempted to enqueue packet on a stopped stream";
    return timestamp;
  }

  // Enqueue the callback and attempt to satisfy a read immediately.
  scoped_refptr<Buffer> buffer =
      new AVPacketBuffer(packet, timestamp, duration);
  if (!buffer) {
    NOTREACHED() << "Unable to allocate AVPacketBuffer";
    return timestamp;
  }
  buffer_queue_.push_back(buffer);
  FulfillPendingRead();
  return timestamp;
}

void FFmpegDemuxerStream::FlushBuffers() {
  DCHECK_EQ(PlatformThread::CurrentId(), demuxer_->thread_id_);
  buffer_queue_.clear();
  discontinuous_ = true;
}

void FFmpegDemuxerStream::Stop() {
  DCHECK_EQ(PlatformThread::CurrentId(), demuxer_->thread_id_);
  buffer_queue_.clear();
  STLDeleteElements(&read_queue_);
  stopped_ = true;
}

const MediaFormat& FFmpegDemuxerStream::media_format() {
  return media_format_;
}

void FFmpegDemuxerStream::Read(Callback1<Buffer*>::Type* read_callback) {
  DCHECK(read_callback);
  demuxer_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &FFmpegDemuxerStream::ReadTask, read_callback));
}

void FFmpegDemuxerStream::ReadTask(Callback1<Buffer*>::Type* read_callback) {
  DCHECK_EQ(PlatformThread::CurrentId(), demuxer_->thread_id_);

  // Don't accept any additional reads if we've been told to stop.
  //
  // TODO(scherkus): it would be cleaner if we replied with an error message.
  if (stopped_) {
    delete read_callback;
    return;
  }

  // Enqueue the callback and attempt to satisfy it immediately.
  read_queue_.push_back(read_callback);
  FulfillPendingRead();

  // There are still pending reads, demux some more.
  if (HasPendingReads()) {
    demuxer_->PostDemuxTask();
  }
}

void FFmpegDemuxerStream::FulfillPendingRead() {
  DCHECK_EQ(PlatformThread::CurrentId(), demuxer_->thread_id_);
  if (buffer_queue_.empty() || read_queue_.empty()) {
    return;
  }

  // Dequeue a buffer and pending read pair.
  scoped_refptr<Buffer> buffer = buffer_queue_.front();
  scoped_ptr<Callback1<Buffer*>::Type> read_callback(read_queue_.front());
  buffer_queue_.pop_front();
  read_queue_.pop_front();

  // Handle discontinuities due to FlushBuffers() being called.
  //
  // TODO(scherkus): get rid of |discontinuous_| and use buffer flags.
  if (discontinuous_) {
    buffer->SetDiscontinuous(true);
    discontinuous_ = false;
  }

  // Execute the callback.
  read_callback->Run(buffer);
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
    : format_context_(NULL),
      thread_id_(NULL) {
}

FFmpegDemuxer::~FFmpegDemuxer() {
  // In this destructor, we clean up resources held by FFmpeg. It is ugly to
  // close the codec contexts here because the corresponding codecs are opened
  // in the decoder filters. By reaching this point, all filters should have
  // stopped, so this is the only safe place to do the global clean up.
  // TODO(hclam): close the codecs in the corresponding decoders.
  AutoLock auto_lock(FFmpegLock::get()->lock());
  if (!format_context_)
    return;

  // Iterate each stream and destroy each one of them.
  int streams = format_context_->nb_streams;
  for (int i = 0; i < streams; ++i) {
    AVStream* stream = format_context_->streams[i];

    // The conditions for calling avcodec_close():
    // 1. AVStream is alive.
    // 2. AVCodecContext in AVStream is alive.
    // 3. AVCodec in AVCodecContext is alive.
    // Notice that closing a codec context without prior avcodec_open() will
    // result in a crash in FFmpeg.
    if (stream && stream->codec && stream->codec->codec) {
      stream->discard = AVDISCARD_ALL;
      avcodec_close(stream->codec);
    }
  }

  // Then finally cleanup the format context.
  av_close_input_file(format_context_);
  format_context_ = NULL;
}

void FFmpegDemuxer::PostDemuxTask() {
  message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &FFmpegDemuxer::DemuxTask));
}

void FFmpegDemuxer::Stop() {
  // Post a task to notify the streams to stop as well.
  message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &FFmpegDemuxer::StopTask));
}

void FFmpegDemuxer::Seek(base::TimeDelta time) {
  // TODO(hclam): by returning from this method, it is assumed that the seek
  // operation is completed and filters behind the demuxer is good to issue
  // more reads, but we are posting a task here, which makes the seek operation
  // asynchronous, should change how seek works to make it fully asynchronous.
  message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &FFmpegDemuxer::SeekTask, time));
}

bool FFmpegDemuxer::Initialize(DataSource* data_source) {
  message_loop()->PostTask(FROM_HERE,
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

  // Grab the thread id for debugging.
  DCHECK(!thread_id_);
  thread_id_ = PlatformThread::CurrentId();

  // Add our data source and get our unique key.
  std::string key = FFmpegGlue::get()->AddDataSource(data_source);

  // Open FFmpeg AVFormatContext.
  DCHECK(!format_context_);
  AVFormatContext* context = NULL;
  int result = av_open_input_file(&context, key.c_str(), NULL, 0, NULL);

  // Remove our data source.
  FFmpegGlue::get()->RemoveDataSource(data_source);

  if (result < 0) {
    host()->Error(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  DCHECK(context);
  format_context_ = context;

  // Serialize calls to av_find_stream_info().
  {
    AutoLock auto_lock(FFmpegLock::get()->lock());

    // Fully initialize AVFormatContext by parsing the stream a little.
    result = av_find_stream_info(format_context_);
    if (result < 0) {
      host()->Error(DEMUXER_ERROR_COULD_NOT_PARSE);
      return;
    }
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
    host()->Error(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    return;
  }

  // Good to go: set the duration and notify we're done initializing.
  host()->SetDuration(max_duration);
  host()->InitializationComplete();
}

void FFmpegDemuxer::SeekTask(base::TimeDelta time) {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_id_);

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

  if (av_seek_frame(format_context_, -1, time.InMicroseconds(),
                    flags) < 0) {
    // TODO(scherkus): signal error.
    NOTIMPLEMENTED();
  }
}

void FFmpegDemuxer::DemuxTask() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_id_);

  // Make sure we have work to do before demuxing.
  if (!StreamsHavePendingReads()) {
    return;
  }

  // Allocate and read an AVPacket from the media.
  scoped_ptr<AVPacket> packet(new AVPacket());
  int result = av_read_frame(format_context_, packet.get());
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
    if (demuxer_stream->GetAVStream()->codec->codec_id == CODEC_ID_MP3) {
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

void FFmpegDemuxer::StopTask() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_id_);
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    (*iter)->Stop();
  }
}

bool FFmpegDemuxer::StreamsHavePendingReads() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_id_);
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if ((*iter)->HasPendingReads()) {
      return true;
    }
  }
  return false;
}

void FFmpegDemuxer::StreamHasEnded() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_id_);
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    AVPacket* packet = new AVPacket();
    memset(packet, 0, sizeof(*packet));
    (*iter)->EnqueuePacket(packet);
  }
}

}  // namespace media
