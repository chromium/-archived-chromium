// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/base/data_buffer.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"

namespace media {

// Size of the decoded audio buffer.
const size_t FFmpegAudioDecoder::kOutputBufferSize =
    AVCODEC_MAX_AUDIO_FRAME_SIZE;

FFmpegAudioDecoder::FFmpegAudioDecoder()
    : codec_context_(NULL) {
}

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
}

// static
bool FFmpegAudioDecoder::IsMediaFormatSupported(const MediaFormat& format) {
  std::string mime_type;
  return format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
      mime_type::kFFmpegAudio == mime_type;
}

bool FFmpegAudioDecoder::OnInitialize(DemuxerStream* demuxer_stream) {
  // Get the AVStream by querying for the provider interface.
  AVStreamProvider* av_stream_provider;
  if (!demuxer_stream->QueryInterface(&av_stream_provider)) {
    return false;
  }
  AVStream* av_stream = av_stream_provider->GetAVStream();

  // Grab the AVStream's codec context and make sure we have sensible values.
  codec_context_ = av_stream->codec;
  DCHECK_GT(codec_context_->channels, 0);
  DCHECK_GT(av_get_bits_per_sample_format(codec_context_->sample_fmt), 0);
  DCHECK_GT(codec_context_->sample_rate, 0);

  // Serialize calls to avcodec_open().
  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  {
    AutoLock auto_lock(FFmpegLock::get()->lock());
    if (!codec || avcodec_open(codec_context_, codec) < 0) {
      return false;
    }
  }

  // When calling avcodec_find_decoder(), |codec_context_| might be altered by
  // the decoder to give more accurate values of the output format of the
  // decoder. So set the media format after a decoder is allocated.
  // TODO(hclam): Reuse the information provided by the demuxer for now, we may
  // need to wait until the first buffer is decoded to know the correct
  // information.
  media_format_.SetAsInteger(MediaFormat::kChannels, codec_context_->channels);
  media_format_.SetAsInteger(MediaFormat::kSampleBits,
      av_get_bits_per_sample_format(codec_context_->sample_fmt));
  media_format_.SetAsInteger(MediaFormat::kSampleRate,
      codec_context_->sample_rate);
  media_format_.SetAsString(MediaFormat::kMimeType,
      mime_type::kUncompressedAudio);

  // Prepare the output buffer.
  output_buffer_.reset(static_cast<uint8*>(av_malloc(kOutputBufferSize)));
  if (!output_buffer_.get()) {
    host()->Error(PIPELINE_ERROR_OUT_OF_MEMORY);
    return false;
  }
  return true;
}

void FFmpegAudioDecoder::OnStop() {
}

void FFmpegAudioDecoder::OnDecode(Buffer* input) {
  // Check for discontinuous buffer. If we receive a discontinuous buffer here,
  // flush the internal buffer of FFmpeg.
  if (input->IsDiscontinuous()) {
    avcodec_flush_buffers(codec_context_);
  }

  // Due to FFmpeg API changes we no longer have const read-only pointers.
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8*>(input->GetData());
  packet.size = input->GetDataSize();

  int16_t* output_buffer = reinterpret_cast<int16_t*>(output_buffer_.get());
  int output_buffer_size = kOutputBufferSize;
  int result = avcodec_decode_audio3(codec_context_,
                                     output_buffer,
                                     &output_buffer_size,
                                     &packet);

  // TODO(ajwong): Consider if kOutputBufferSize should just be an int instead
  // of a size_t.
  if (result < 0 ||
      output_buffer_size < 0 ||
      static_cast<size_t>(output_buffer_size) > kOutputBufferSize) {
    host()->Error(PIPELINE_ERROR_DECODE);
    return;
  }

  // If we have decoded something, enqueue the result.
  if (output_buffer_size) {
    DataBuffer* result_buffer = new DataBuffer();
    memcpy(result_buffer->GetWritableData(output_buffer_size),
           output_buffer, output_buffer_size);

    // Determine the duration if the demuxer couldn't figure it out, otherwise
    // copy it over.
    if (input->GetDuration().InMicroseconds() == 0) {
      result_buffer->SetDuration(CalculateDuration(output_buffer_size));
    } else {
      result_buffer->SetDuration(input->GetDuration());
    }

    // Copy over the timestamp.
    result_buffer->SetTimestamp(input->GetTimestamp());

    EnqueueResult(result_buffer);
    return;
  }

  // Three conditions to meet to declare end of stream for this decoder:
  // 1. FFmpeg didn't read anything.
  // 2. FFmpeg didn't output anything.
  // 3. An end of stream buffer is received.
  if (result == 0 && output_buffer_size == 0 && input->IsEndOfStream()) {
    DataBuffer* result_buffer = new DataBuffer();
    result_buffer->SetTimestamp(input->GetTimestamp());
    result_buffer->SetDuration(input->GetDuration());
    EnqueueResult(result_buffer);
  }
}

base::TimeDelta FFmpegAudioDecoder::CalculateDuration(size_t size) {
  int64 denominator = codec_context_->channels *
      av_get_bits_per_sample_format(codec_context_->sample_fmt) / 8 *
      codec_context_->sample_rate;
  double microseconds = size /
      (denominator / static_cast<double>(base::Time::kMicrosecondsPerSecond));
  return base::TimeDelta::FromMicroseconds(static_cast<int64>(microseconds));
}

}  // namespace
