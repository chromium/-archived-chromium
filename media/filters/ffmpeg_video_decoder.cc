// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/base/video_frame_impl.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"

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
    : DecoderBase<VideoDecoder, VideoFrame>(NULL),
      width_(0),
      height_(0) {
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
  scoped_refptr<FFmpegDemuxerStream> ffmpeg_demuxer_stream;
  if (!demuxer_stream->QueryInterface(&ffmpeg_demuxer_stream)) {
    return false;
  }

  AVStream* av_stream = ffmpeg_demuxer_stream->av_stream();

  width_ = av_stream->codec->width;
  height_ = av_stream->codec->height;

  media_format_.SetAsString(MediaFormat::kMimeType,
                            mime_type::kUncompressedVideo);
  media_format_.SetAsInteger(MediaFormat::kWidth, width_);
  media_format_.SetAsInteger(MediaFormat::kHeight, height_);

  codec_context_ = ffmpeg_demuxer_stream->av_stream()->codec;
  codec_context_->flags2 |= CODEC_FLAG2_FAST;  // Enable faster H264 decode.
  // Enable motion vector search (potentially slow), strong deblocking filter
  // for damaged macroblocks, and set our error detection sensitivity.
  codec_context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  codec_context_->error_recognition = FF_ER_CAREFUL;
  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec ||
      avcodec_thread_init(codec_context_, kDecodeThreads) < 0 ||
      avcodec_open(codec_context_, codec) < 0) {
    host_->Error(media::PIPELINE_ERROR_DECODE);
    return false;
  }
  return true;
}

void FFmpegVideoDecoder::OnDecode(Buffer* buffer) {
  // Check for end of stream.
  // TODO(scherkus): check for end of stream.

  // Queue the incoming timestamp.
  TimeTuple times;
  times.timestamp = buffer->GetTimestamp();
  times.duration = buffer->GetDuration();
  time_queue_.push(times);

  // Create a packet for input data.
  // Due to FFmpeg API changes we no longer have const read-only pointers.
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8*>(buffer->GetData());
  packet.size = buffer->GetDataSize();

  // We don't allocate AVFrame on the stack since different versions of FFmpeg
  // may change the size of AVFrame, causing stack corruption.  The solution is
  // to let FFmpeg allocate the structure via avcodec_alloc_frame().
  int decoded = 0;
  scoped_ptr_malloc<AVFrame, ScopedPtrAVFree> yuv_frame(avcodec_alloc_frame());
  int result = avcodec_decode_video2(codec_context_, yuv_frame.get(), &decoded,
                                     &packet);

  // Log the problem if we can't decode a video frame.
  if (result < 0) {
    LOG(INFO) << "Error decoding a video frame with timestamp: "
              << buffer->GetTimestamp().InMicroseconds() << " us"
              << " , duration: "
              << buffer->GetDuration().InMicroseconds() << " us"
              << " , packet size: "
              << buffer->GetDataSize() << " bytes";
  }

  // Check for a decoded frame instead of checking the return value of
  // avcodec_decode_video(). We don't need to stop the pipeline on
  // decode errors.
  if (!decoded)
    return;

  // J (Motion JPEG) versions of YUV are full range 0..255.
  // Regular (MPEG) YUV is 16..240.
  // For now we will ignore the distinction and treat them the same.

  VideoSurface::Format surface_format;
  switch (codec_context_->pix_fmt) {
    case PIX_FMT_YUV420P:
    case PIX_FMT_YUVJ420P:
      surface_format = VideoSurface::YV12;
      break;
    case PIX_FMT_YUV422P:
    case PIX_FMT_YUVJ422P:
      surface_format = VideoSurface::YV16;
      break;
    default:
      // TODO(scherkus): More formats here?
      NOTREACHED();
      host_->Error(PIPELINE_ERROR_DECODE);
      return;
  }
  if (!EnqueueVideoFrame(surface_format, yuv_frame.get())) {
    host_->Error(PIPELINE_ERROR_DECODE);
  }
}

bool FFmpegVideoDecoder::EnqueueVideoFrame(VideoSurface::Format surface_format,
                                           const AVFrame* frame) {
  // Dequeue the next time tuple and create a VideoFrame object with
  // that timestamp and duration.
  TimeTuple time = time_queue_.top();
  time_queue_.pop();

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

}  // namespace
