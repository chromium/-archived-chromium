// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_COMMON_H_
#define MEDIA_FILTERS_FFMPEG_COMMON_H_

// Used for FFmpeg error codes.
#include <cerrno>

#include "base/compiler_specific.h"

// Include FFmpeg header files.
extern "C" {
// Temporarily disable possible loss of data warning.
// TODO(scherkus): fix and upstream the compiler warnings.
MSVC_PUSH_DISABLE_WARNING(4244);
#include "third_party/ffmpeg/include/libavcodec/avcodec.h"
#include "third_party/ffmpeg/include/libavformat/avformat.h"
MSVC_POP_WARNING();
}  // extern "C"

namespace media {

// MediaFormat key identifying the CodecID.
extern const char kFFmpegCodecID[];

// FFmpeg MIME types.
namespace mime_type {

extern const char kFFmpegAudio[];
extern const char kFFmpegVideo[];

}  // namespace mime_type

namespace interface_id {

extern const char kFFmpegDemuxerStream[];

}  // namespace interface_id


}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_COMMON_H_
