// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_ffmpeg.h"

#include "base/logging.h"
#include "media/filters/ffmpeg_common.h"

namespace media {

MockFFmpeg* MockFFmpeg::instance_ = NULL;

MockFFmpeg::MockFFmpeg()
    : outstanding_packets_(0) {
}

MockFFmpeg::~MockFFmpeg() {
  CHECK(!outstanding_packets_)
      << "MockFFmpeg destroyed with outstanding packets";
}

void MockFFmpeg::inc_outstanding_packets() {
  ++outstanding_packets_;
}

void MockFFmpeg::dec_outstanding_packets() {
  CHECK(outstanding_packets_ > 0);
  --outstanding_packets_;
}

// static
void MockFFmpeg::set(MockFFmpeg* instance) {
  instance_ = instance;
}

// static
MockFFmpeg* MockFFmpeg::get() {
  return instance_;
}

// static
void MockFFmpeg::DestructPacket(AVPacket* packet) {
  delete [] packet->data;
  packet->data = NULL;
  packet->size = 0;
}

// FFmpeg stubs that delegate to the FFmpegMock instance.
extern "C" {

AVCodec* avcodec_find_decoder(enum CodecID id) {
  return media::MockFFmpeg::get()->AVCodecFindDecoder(id);
}

int avcodec_open(AVCodecContext* avctx, AVCodec* codec) {
  return media::MockFFmpeg::get()->AVCodecOpen(avctx, codec);
}

int avcodec_close(AVCodecContext* avctx) {
  return media::MockFFmpeg::get()->AVCodecClose(avctx);
}

int avcodec_thread_init(AVCodecContext* avctx, int threads) {
  return media::MockFFmpeg::get()->AVCodecThreadInit(avctx, threads);
}

void avcodec_flush_buffers(AVCodecContext* avctx) {
  return media::MockFFmpeg::get()->AVCodecFlushBuffers(avctx);
}

AVFrame* avcodec_alloc_frame() {
  return media::MockFFmpeg::get()->AVCodecAllocFrame();
}

int avcodec_decode_video2(AVCodecContext* avctx, AVFrame* picture,
                          int* got_picture_ptr, AVPacket* avpkt) {
  return media::MockFFmpeg::get()->
      AVCodecDecodeVideo2(avctx, picture, got_picture_ptr, avpkt);
}

int av_open_input_file(AVFormatContext** format, const char* filename,
                       AVInputFormat* input_format, int buffer_size,
                       AVFormatParameters* parameters) {
  return media::MockFFmpeg::get()->AVOpenInputFile(format, filename,
                                                   input_format, buffer_size,
                                                   parameters);
}

void av_close_input_file(AVFormatContext* format) {
  media::MockFFmpeg::get()->AVCloseInputFile(format);
}

int av_find_stream_info(AVFormatContext* format) {
  return media::MockFFmpeg::get()->AVFindStreamInfo(format);
}

int64 av_rescale_q(int64 a, AVRational bq, AVRational cq) {
  // Because this is a math function there's little point in mocking it, so we
  // implement a cheap version that's capable of overflowing.
  int64 num = bq.num * cq.den;
  int64 den = cq.num * bq.den;
  return a * num / den;
}

int av_read_frame(AVFormatContext* format, AVPacket* packet) {
  return media::MockFFmpeg::get()->AVReadFrame(format, packet);
}

int av_seek_frame(AVFormatContext *format, int stream_index, int64_t timestamp,
                  int flags) {
  return media::MockFFmpeg::get()->AVSeekFrame(format, stream_index, timestamp,
                                               flags);
}

void av_init_packet(AVPacket* pkt) {
  return media::MockFFmpeg::get()->AVInitPacket(pkt);
}

int av_new_packet(AVPacket* packet, int size) {
  return media::MockFFmpeg::get()->AVNewPacket(packet, size);
}

void av_free_packet(AVPacket* packet) {
  media::MockFFmpeg::get()->AVFreePacket(packet);
}

void av_free(void* ptr) {
  // Freeing NULL pointers are valid, but they aren't interesting from a mock
  // perspective.
  if (ptr) {
    media::MockFFmpeg::get()->AVFree(ptr);
  }
}

}  // extern "C"

}  // namespace media
