// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/base/video_frame_impl.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"

namespace media {


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

  // Cast everything to FFmpeg types.
  const uint8_t* data_in = buffer->GetData();
  const size_t size_in = buffer->GetDataSize();

  int decoded = 0;
  AVFrame yuv_frame;
  avcodec_get_frame_defaults(&yuv_frame);
  int result = avcodec_decode_video(codec_context_, &yuv_frame, &decoded,
                                    data_in, size_in);

  if (result < 0) {
    host_->Error(PIPELINE_ERROR_DECODE);
    return;
  }

  if (result == 0 || decoded == 0) {
    return;
  }

  VideoSurface::Format surface_format;
  switch (codec_context_->pix_fmt) {
    case PIX_FMT_YUV420P:
      surface_format = VideoSurface::YV12;
      break;
    case PIX_FMT_YUV422P:
      surface_format = VideoSurface::YV16;
      break;
    default:
      // TODO(scherkus): More formats here?
      NOTREACHED();
      host_->Error(PIPELINE_ERROR_DECODE);
      return;
  }
  if (!EnqueueVideoFrame(surface_format, yuv_frame)) {
    host_->Error(PIPELINE_ERROR_DECODE);
  }
}

bool FFmpegVideoDecoder::EnqueueVideoFrame(VideoSurface::Format surface_format,
                                           const AVFrame& frame) {
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
                                   const AVFrame& frame) {
  DCHECK(surface.width % 4 == 0);
  DCHECK(surface.height % 2 == 0);
  const uint8* source = frame.data[plane];
  const size_t source_stride = frame.linesize[plane];
  uint8* dest = surface.data[plane];
  const size_t dest_stride = surface.strides[plane];
  size_t bytes_per_line = surface.width;
  size_t copy_lines = surface.height;
  if (plane != VideoSurface::kYPlane) {
    bytes_per_line /= 2;
    if (surface.format == VideoSurface::YV12) {
      copy_lines /= 2;
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
