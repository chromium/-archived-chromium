// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/base/data_buffer.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"

namespace media {

const size_t FFmpegAudioDecoder::kOutputBufferSize =
    AVCODEC_MAX_AUDIO_FRAME_SIZE;

FFmpegAudioDecoder::FFmpegAudioDecoder()
    : DecoderBase<AudioDecoder, Buffer>(NULL),
      codec_context_(NULL),
      output_buffer_(NULL) {
}

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
}

// static
bool FFmpegAudioDecoder::IsMediaFormatSupported(const MediaFormat& format) {
  int channels, sample_bits, sample_rate;
  std::string mime_type;
  if (format.GetAsInteger(MediaFormat::kChannels, &channels) &&
      format.GetAsInteger(MediaFormat::kSampleBits, &sample_bits) &&
      format.GetAsInteger(MediaFormat::kSampleRate, &sample_rate) &&
      format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
      mime_type::kFFmpegAudio == mime_type) {
    return true;
  }
  return false;
}

bool FFmpegAudioDecoder::OnInitialize(DemuxerStream* demuxer_stream) {
  scoped_refptr<FFmpegDemuxerStream> ffmpeg_demuxer_stream;

  // Try to obtain a reference to FFmpegDemuxer.
  if (!demuxer_stream->
          QueryInterface<FFmpegDemuxerStream>(&ffmpeg_demuxer_stream))
    return false;

  // Setting the media format.
  // TODO(hclam): Reuse the information provided by the demuxer for now, we may
  // need to wait until the first buffer is decoded to know the correct
  // information.
  media_format_.SetAsInteger(MediaFormat::kChannels,
      ffmpeg_demuxer_stream->av_stream()->codec->channels);
  media_format_.SetAsInteger(MediaFormat::kSampleBits,
      ffmpeg_demuxer_stream->av_stream()->codec->bits_per_raw_sample);
  media_format_.SetAsInteger(MediaFormat::kSampleRate,
      ffmpeg_demuxer_stream->av_stream()->codec->sample_rate);
  media_format_.SetAsString(MediaFormat::kMimeType,
      mime_type::kUncompressedAudio);

  // Grab the codec context from ffmpeg demuxer.
  codec_context_ = ffmpeg_demuxer_stream->av_stream()->codec;

  // Prepare the output buffer.
  output_buffer_ = static_cast<uint8*>(av_malloc(kOutputBufferSize));
  if (!output_buffer_) {
    host_->Error(PIPELINE_ERROR_OUT_OF_MEMORY);
    return false;
  }
  return true;
}

void FFmpegAudioDecoder::OnStop() {
  if (output_buffer_)
    av_free(output_buffer_);
}

void FFmpegAudioDecoder::OnDecode(Buffer* input) {
  const uint8_t* input_buffer = input->GetData();
  size_t input_buffer_size = input->GetDataSize();

  int output_buffer_size = kOutputBufferSize;
  int result = avcodec_decode_audio2(codec_context_,
                                     reinterpret_cast<int16_t*>(output_buffer_),
                                     &output_buffer_size,
                                     input_buffer,
                                     input_buffer_size);

  if (result < 0 || output_buffer_size > kOutputBufferSize) {
    host_->Error(PIPELINE_ERROR_DECODE);
  } else if (result == 0) {
    // TODO(scherkus): does this mark EOS? Do we want to fulfill a read request
    // with zero size?
  } else {
    DataBuffer* result_buffer = new DataBuffer();
    memcpy(result_buffer->GetWritableData(output_buffer_size),
      output_buffer_, output_buffer_size);
    EnqueueResult(result_buffer);
  }
}

}  // namespace
