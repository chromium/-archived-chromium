// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_FFMPEG_H_
#define MEDIA_BASE_MOCK_FFMPEG_H_

#include "testing/gmock/include/gmock/gmock.h"

struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
enum CodecID;

namespace media {

class MockFFmpeg {
 public:
  MOCK_METHOD1(AVCodecFindDecoder, AVCodec*(enum CodecID id));
  MOCK_METHOD2(AVCodecOpen, int(AVCodecContext* avctx, AVCodec* codec));
  MOCK_METHOD2(AVCodecThreadInit, int(AVCodecContext* avctx, int threads));

  // Setter/getter for the global instance of MockFFmpeg.
  static void set(MockFFmpeg* instance);
  static MockFFmpeg* get();

 private:
  static MockFFmpeg* instance_;
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FFMPEG_H_
