// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/base/video_frame_impl.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"

namespace {

const AVRational kMicrosBase = { 1, base::Time::kMicrosecondsPerSecond };

// TODO(ajwong): Move this into a utility function file and dedup with
// FFmpegDemuxer ConvertTimestamp.
base::TimeDelta ConvertTimestamp(const AVRational& time_base, int64 timestamp) {
  int64 microseconds = av_rescale_q(timestamp, time_base, kMicrosBase);
  return base::TimeDelta::FromMicroseconds(microseconds);
}

}  // namespace

namespace media {

// Always try to use two threads for video decoding.  There is little reason
// not to since current day CPUs tend to be multi-core and we measured
// performance benefits on older machines such as P4s with hyperthreading.
//
// Handling decoding on separate threads also frees up the pipeline thread to
// continue processing. Although it'd be nice to have the option of a single
// decoding thread, FFmpeg treats having one thread the same as having zero
// threads (i.e., avcodec_decode_video() will execute on the calling thread).
// Yet another reason for having two threads :)
//
// TODO(scherkus): some video codecs might not like avcodec_thread_init() being
// called on them... should attempt to find out which ones those are!
static const int kDecodeThreads = 2;

FFmpegVideoDecoder::FFmpegVideoDecoder()
    : width_(0),
      height_(0),
      time_base_(new AVRational()),
      state_(kNormal),
      codec_context_(NULL) {
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
}

// static
bool FFmpegVideoDecoder::IsMediaFormatSupported(const MediaFormat& format) {
  std::string mime_type;
  return format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
      mime_type::kFFmpegVideo == mime_type;
}

bool FFmpegVideoDecoder::OnInitialize(DemuxerStream* demuxer_stream) {
  // Get the AVStream by querying for the provider interface.
  AVStreamProvider* av_stream_provider;
  if (!demuxer_stream->QueryInterface(&av_stream_provider)) {
    return false;
  }
  AVStream* av_stream = av_stream_provider->GetAVStream();

  width_ = av_stream->codec->width;
  height_ = av_stream->codec->height;
  *time_base_ = av_stream->time_base;

  media_format_.SetAsString(MediaFormat::kMimeType,
                            mime_type::kUncompressedVideo);
  media_format_.SetAsInteger(MediaFormat::kWidth, width_);
  media_format_.SetAsInteger(MediaFormat::kHeight, height_);

  codec_context_ = av_stream->codec;
  codec_context_->flags2 |= CODEC_FLAG2_FAST;  // Enable faster H264 decode.
  // Enable motion vector search (potentially slow), strong deblocking filter
  // for damaged macroblocks, and set our error detection sensitivity.
  codec_context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  codec_context_->error_recognition = FF_ER_CAREFUL;

  // Serialize calls to avcodec_open().
  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  {
    AutoLock auto_lock(FFmpegLock::get()->lock());
    if (!codec ||
        avcodec_thread_init(codec_context_, kDecodeThreads) < 0 ||
        avcodec_open(codec_context_, codec) < 0) {
      return false;
    }
  }
  return true;
}

void FFmpegVideoDecoder::OnSeek(base::TimeDelta time) {
  // Everything in the presentation time queue is invalid, clear the queue.
  while (!pts_queue_.empty())
    pts_queue_.pop();
}

void FFmpegVideoDecoder::OnDecode(Buffer* buffer) {
  // During decode, because reads are issued asynchronously, it is possible to
  // recieve multiple end of stream buffers since each read is acked. When the
  // first end of stream buffer is read, FFmpeg may still have frames queued
  // up in the decoder so we need to go through the decode loop until it stops
  // giving sensible data.  After that, the decoder should output empty
  // frames.  There are three states the decoder can be in:
  //
  //   kNormal: This is the starting state. Buffers are decoded. Decode errors
  //            are discarded.
  //   kFlushCodec: There isn't any more input data. Call avcodec_decode_video2
  //                until no more data is returned to flush out remaining
  //                frames. The input buffer is ignored at this point.
  //   kDecodeFinished: All calls return empty frames.
  //
  // These are the possible state transitions.
  //
  // kNormal -> kFlushCodec:
  //     When buffer->IsEndOfStream() is first true.
  // kNormal -> kDecodeFinished:
  //     A catastrophic failure occurs, and decoding needs to stop.
  // kFlushCodec -> kDecodeFinished:
  //     When avcodec_decode_video2() returns 0 data or errors out.
  //
  // If the decoding is finished, we just always return empty frames.
  if (state_ == kDecodeFinished) {
    EnqueueEmptyFrame();
    return;
  }

  // Transition to kFlushCodec on the first end of stream buffer.
  if (state_ == kNormal && buffer->IsEndOfStream()) {
    state_ = kFlushCodec;
  }

  // Push all incoming timestamps into the priority queue as long as we have
  // not yet received an end of stream buffer.  It is important that this line
  // stay below the state transition into kFlushCodec done above.
  //
  // TODO(ajwong): This push logic, along with the pop logic below needs to
  // be reevaluated to correctly handle decode errors.
  if (state_ == kNormal) {
    pts_queue_.push(buffer->GetTimestamp());
  }

  // Otherwise, attempt to decode a single frame.
  scoped_ptr_malloc<AVFrame, ScopedPtrAVFree> yuv_frame(avcodec_alloc_frame());
  if (DecodeFrame(*buffer, codec_context_, yuv_frame.get())) {
    last_pts_ = FindPtsAndDuration(*time_base_,
                                   pts_queue_,
                                   last_pts_,
                                   yuv_frame.get());

    // Pop off a pts on a successful decode since we are "using up" one
    // timestamp.
    //
    // TODO(ajwong): Do we need to pop off a pts when avcodec_decode_video2()
    // returns < 0?  The rationale is that when get_picture_ptr == 0, we skip
    // popping a pts because no frame was produced.  However, when
    // avcodec_decode_video2() returns false, it is a decode error, which
    // if it means a frame is dropped, may require us to pop one more time.
    if (!pts_queue_.empty()) {
      pts_queue_.pop();
    } else {
      NOTREACHED() << "Attempting to decode more frames than were input.";
    }

    if (!EnqueueVideoFrame(
            GetSurfaceFormat(*codec_context_), last_pts_, yuv_frame.get())) {
      // On an EnqueueEmptyFrame error, error out the whole pipeline and
      // set the state to kDecodeFinished.
      SignalPipelineError();
    }
  } else {
    // When in kFlushCodec, any errored decode, or a 0-lengthed frame,
    // is taken as a signal to stop decoding.
    if (state_ == kFlushCodec) {
      state_ = kDecodeFinished;
      EnqueueEmptyFrame();
    }
  }
}

bool FFmpegVideoDecoder::EnqueueVideoFrame(VideoSurface::Format surface_format,
                                           const TimeTuple& time,
                                           const AVFrame* frame) {
  scoped_refptr<VideoFrame> video_frame;
  VideoFrameImpl::CreateFrame(surface_format, width_, height_,
                              time.timestamp, time.duration, &video_frame);
  if (!video_frame) {
    return false;
  }

  // Copy the frame data since FFmpeg reuses internal buffers for AVFrame
  // output, meaning the data is only valid until the next
  // avcodec_decode_video() call.
  // TODO(scherkus): figure out pre-allocation/buffer cycling scheme.
  // TODO(scherkus): is there a cleaner way to figure out the # of planes?
  VideoSurface surface;
  if (!video_frame->Lock(&surface)) {
    return false;
  }
  CopyPlane(VideoSurface::kYPlane, surface, frame);
  CopyPlane(VideoSurface::kUPlane, surface, frame);
  CopyPlane(VideoSurface::kVPlane, surface, frame);
  video_frame->Unlock();
  EnqueueResult(video_frame);
  return true;
}

void FFmpegVideoDecoder::CopyPlane(size_t plane,
                                   const VideoSurface& surface,
                                   const AVFrame* frame) {
  DCHECK(surface.width % 2 == 0);
  const uint8* source = frame->data[plane];
  const size_t source_stride = frame->linesize[plane];
  uint8* dest = surface.data[plane];
  const size_t dest_stride = surface.strides[plane];
  size_t bytes_per_line = surface.width;
  size_t copy_lines = surface.height;
  if (plane != VideoSurface::kYPlane) {
    bytes_per_line /= 2;
    if (surface.format == VideoSurface::YV12) {
      copy_lines = (copy_lines + 1) / 2;
    }
  }
  DCHECK(bytes_per_line <= source_stride && bytes_per_line <= dest_stride);
  for (size_t i = 0; i < copy_lines; ++i) {
    memcpy(dest, source, bytes_per_line);
    source += source_stride;
    dest += dest_stride;
  }
}

void FFmpegVideoDecoder::EnqueueEmptyFrame() {
  scoped_refptr<VideoFrame> video_frame;
  VideoFrameImpl::CreateEmptyFrame(&video_frame);
  EnqueueResult(video_frame);
}

bool FFmpegVideoDecoder::DecodeFrame(const Buffer& buffer,
                                     AVCodecContext* codec_context,
                                     AVFrame* yuv_frame) {
  // Check for discontinuous buffer. If we receive a discontinuous buffer here,
  // flush the internal buffer of FFmpeg.
  if (buffer.IsDiscontinuous()) {
    avcodec_flush_buffers(codec_context);
  }

  // Create a packet for input data.
  // Due to FFmpeg API changes we no longer have const read-only pointers.
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8*>(buffer.GetData());
  packet.size = buffer.GetDataSize();

  // We don't allocate AVFrame on the stack since different versions of FFmpeg
  // may change the size of AVFrame, causing stack corruption.  The solution is
  // to let FFmpeg allocate the structure via avcodec_alloc_frame().
  int frame_decoded = 0;
  int result =
      avcodec_decode_video2(codec_context, yuv_frame, &frame_decoded, &packet);

  // Log the problem if we can't decode a video frame and exit early.
  if (result < 0) {
    LOG(INFO) << "Error decoding a video frame with timestamp: "
              << buffer.GetTimestamp().InMicroseconds() << " us"
              << " , duration: "
              << buffer.GetDuration().InMicroseconds() << " us"
              << " , packet size: "
              << buffer.GetDataSize() << " bytes";
    return false;
  }

  // If frame_decoded == 0, then no frame was produced.
  return frame_decoded != 0;
}

FFmpegVideoDecoder::TimeTuple FFmpegVideoDecoder::FindPtsAndDuration(
    const AVRational& time_base,
    const TimeQueue& pts_queue,
    const TimeTuple& last_pts,
    const AVFrame* frame) {
  TimeTuple pts;

  // Default repeat_pict to 0 because if there is no frame information,
  // we just assume the frame only plays for one time_base.
  int repeat_pict = 0;

  // First search the AVFrame for the pts. This is the most authoritative.
  // Make a special exclusion for the value frame->pts == 0.  Though this
  // is technically a valid value, it seems a number of ffmpeg codecs will
  // mistakenly always set frame->pts to 0.
  //
  // Oh, and we have to cast AV_NOPTS_VALUE since it ends up becoming unsigned
  // because the value they use doesn't fit in a signed 64-bit number which
  // produces a signedness comparison warning on gcc.
  if (frame &&
      (frame->pts != static_cast<int64_t>(AV_NOPTS_VALUE)) &&
      (frame->pts != 0)) {
    pts.timestamp = ConvertTimestamp(time_base, frame->pts);
    repeat_pict = frame->repeat_pict;
  } else if (!pts_queue.empty()) {
    // If the frame did not have pts, try to get the pts from the
    // |pts_queue_|.
    pts.timestamp = pts_queue.top();
  } else {
    // Unable to read the pts from anywhere. Time to guess.
    pts.timestamp = last_pts.timestamp + last_pts.duration;
  }

  // Fill in the duration while accounting for repeated frames.
  //
  // TODO(ajwong): Make sure this formula is correct.
  pts.duration = ConvertTimestamp(time_base, 1 + repeat_pict);

  return pts;
}

VideoSurface::Format FFmpegVideoDecoder::GetSurfaceFormat(
    const AVCodecContext& codec_context) {
  // J (Motion JPEG) versions of YUV are full range 0..255.
  // Regular (MPEG) YUV is 16..240.
  // For now we will ignore the distinction and treat them the same.
  switch (codec_context.pix_fmt) {
    case PIX_FMT_YUV420P:
    case PIX_FMT_YUVJ420P:
      return VideoSurface::YV12;
      break;
    case PIX_FMT_YUV422P:
    case PIX_FMT_YUVJ422P:
      return VideoSurface::YV16;
      break;
    default:
      // TODO(scherkus): More formats here?
      return VideoSurface::INVALID;
  }
}

void FFmpegVideoDecoder::SignalPipelineError() {
  host()->Error(PIPELINE_ERROR_DECODE);
  state_ = kDecodeFinished;
}

// static
bool FFmpegVideoDecoder::PtsHeapOrdering::operator()(
    const base::TimeDelta& lhs,
    const base::TimeDelta& rhs) const {
  // std::priority_queue is a max-heap. We want lower timestamps to show up
  // first so reverse the natural less-than comparison.
  return rhs < lhs;
}

}  // namespace
