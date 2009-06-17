// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_COMMON_H_
#define MEDIA_FILTERS_FFMPEG_COMMON_H_

// Used for FFmpeg error codes.
#include <cerrno>

#include "base/compiler_specific.h"
#include "base/singleton.h"

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

// FFmpegLock is used to serialize calls to avcodec_open(), avcodec_close(),
// and av_find_stream_info() for an entire process because for whatever reason
// it does Very Bad Things to other FFmpeg instances.
//
// TODO(scherkus): track down and upstream a fix to FFmpeg, if possible.
class FFmpegLock : public Singleton<FFmpegLock> {
 public:
  Lock& lock();

 private:
  // Only allow Singleton to create and delete FFmpegLock.
  friend struct DefaultSingletonTraits<FFmpegLock>;
  FFmpegLock();
  virtual ~FFmpegLock();

  Lock lock_;
  DISALLOW_COPY_AND_ASSIGN(FFmpegLock);
};


// Wraps FFmpeg's av_free() in a class that can be passed as a template argument
// to scoped_ptr_malloc.
class ScopedPtrAVFree {
 public:
  inline void operator()(void* x) const {
    av_free(x);
  }
};


// FFmpeg MIME types.
namespace mime_type {

extern const char kFFmpegAudio[];
extern const char kFFmpegVideo[];

}  // namespace mime_type

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_COMMON_H_
