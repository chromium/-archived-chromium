// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/filters/ffmpeg_audio_decoder.h"

namespace media {

FFmpegAudioDecoder::FFmpegAudioDecoder()
    : DecoderBase<AudioDecoder, Buffer>(NULL) {
  NOTIMPLEMENTED();
}

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
  NOTIMPLEMENTED();
}

// static
bool FFmpegAudioDecoder::IsMediaFormatSupported(const MediaFormat* format) {
  NOTIMPLEMENTED();
  return false;
}

bool FFmpegAudioDecoder::OnInitialize(DemuxerStream* demuxer_stream) {
  NOTIMPLEMENTED();
  return false;
}

void FFmpegAudioDecoder::OnDecode(Buffer* input) {
  NOTIMPLEMENTED();
}

}  // namespace
