// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/filters/ffmpeg_video_decoder.h"

namespace media {

FFmpegVideoDecoder::FFmpegVideoDecoder()
    : DecoderBase<VideoDecoder, VideoFrame>(NULL) {
  NOTIMPLEMENTED();
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
  NOTIMPLEMENTED();
}

// static
bool FFmpegVideoDecoder::IsMediaFormatSupported(const MediaFormat& format) {
  NOTIMPLEMENTED();
  return false;
}

bool FFmpegVideoDecoder::OnInitialize(DemuxerStream* demuxer_stream) {
  NOTIMPLEMENTED();
  return false;
}

void FFmpegVideoDecoder::OnDecode(Buffer* input) {
  NOTIMPLEMENTED();
}

}  // namespace
