// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.


#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_

#include "media/base/factory.h"
#include "media/filters/decoder_base.h"

namespace media {

//------------------------------------------------------------------------------

class FFmpegVideoDecoder : public DecoderBase<VideoDecoder, VideoFrame> {
 public:
  static FilterFactory* CreateFactory() {
    return new FilterFactoryImpl0<FFmpegVideoDecoder>();
  }

  static bool IsMediaFormatSupported(const MediaFormat& media_format);

  virtual bool OnInitialize(DemuxerStream* demuxer_stream);

  virtual void OnDecode(Buffer* input);

 private:
  friend class FilterFactoryImpl0<FFmpegVideoDecoder>;
  FFmpegVideoDecoder();
  virtual ~FFmpegVideoDecoder();

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
