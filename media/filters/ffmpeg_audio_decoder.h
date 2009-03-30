// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.


#ifndef MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_

#include "media/base/factory.h"
#include "media/filters/decoder_base.h"

struct AVCodecContext;

namespace media {

//------------------------------------------------------------------------------

class FFmpegAudioDecoder : public DecoderBase<AudioDecoder, Buffer> {
 public:
  static FilterFactory* CreateFactory() {
    return new FilterFactoryImpl0<FFmpegAudioDecoder>();
  }

  static bool IsMediaFormatSupported(const MediaFormat& media_format);

  virtual bool OnInitialize(DemuxerStream* demuxer_stream);

  virtual void OnStop();

  virtual void OnDecode(Buffer* input);

 private:
  friend class FilterFactoryImpl0<FFmpegAudioDecoder>;
  FFmpegAudioDecoder();
  virtual ~FFmpegAudioDecoder();

  // A FFmpeg defined structure that holds decoder information, this variable
  // is initialized in OnInitialize().
  AVCodecContext* codec_context_;

  // Data buffer to carry decoded raw PCM samples. This buffer is created by
  // av_malloc and is used throughout the lifetime of this class.
  uint8* output_buffer_;

  static const size_t kOutputBufferSize;

  DISALLOW_COPY_AND_ASSIGN(FFmpegAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
