// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.


#ifndef MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_

#include "media/base/factory.h"
#include "media/filters/decoder_base.h"

namespace media {

//------------------------------------------------------------------------------

class FFmpegAudioDecoder : public DecoderBase<AudioDecoder, Buffer> {
 public:
  static FilterFactory* CreateFactory() {
    return new FilterFactoryImpl0<FFmpegAudioDecoder>();
  }

  static bool IsMediaFormatSupported(const MediaFormat& media_format);

  virtual bool OnInitialize(DemuxerStream* demuxer_stream);

  virtual void OnDecode(Buffer* input);

 private:
  friend FilterFactoryImpl0<FFmpegAudioDecoder>;
  FFmpegAudioDecoder();
  virtual ~FFmpegAudioDecoder();

  DISALLOW_COPY_AND_ASSIGN(FFmpegAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
