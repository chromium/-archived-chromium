// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_ffmpeg.h"

#include "base/logging.h"
#include "media/filters/ffmpeg_common.h"

namespace media {

MockFFmpeg* MockFFmpeg::instance_ = NULL;

// static
void MockFFmpeg::set(MockFFmpeg* instance) {
  instance_ = instance;
}

// static
MockFFmpeg* MockFFmpeg::get() {
  return instance_;
}

// FFmpeg stubs that delegate to the FFmpegMock instance.
extern "C" {

AVCodec* avcodec_find_decoder(enum CodecID id) {
  return media::MockFFmpeg::get()->AVCodecFindDecoder(id);
}

int avcodec_open(AVCodecContext* avctx, AVCodec* codec) {
  return media::MockFFmpeg::get()->AVCodecOpen(avctx, codec);
}

int avcodec_thread_init(AVCodecContext* avctx, int threads) {
  return media::MockFFmpeg::get()->AVCodecThreadInit(avctx, threads);
}

void avcodec_flush_buffers(AVCodecContext* avctx) {
  NOTREACHED();
}

AVFrame* avcodec_alloc_frame() {
  NOTREACHED();
  return NULL;
}

int avcodec_decode_video2(AVCodecContext* avctx, AVFrame* picture,
                          int* got_picture_ptr, AVPacket* avpkt) {
  NOTREACHED();
  return 0;
}

void av_init_packet(AVPacket* pkt) {
  NOTREACHED();
}

}  // extern "C"

}  // namespace media
