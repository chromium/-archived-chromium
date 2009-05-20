// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include <string>

#include <dlfcn.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "media/filters/ffmpeg_common.h"

// We create stub references to dynamically loaded functions in ffmpeg
// for ease of linking.
//
// TODO(ajwong): We need to find a more maintainable way to have this work.
// Also, this code should really be in the ffmpeg wrapper, and not here
// in the media level.  The concept of "weak symbols" looks like it might
// be promising, but I don't quite understand it yet.
extern "C" {

int (*av_get_bits_per_sample_format_ptr)(enum SampleFormat sample_fmt);
int av_get_bits_per_sample_format(enum SampleFormat sample_fmt) {
  return av_get_bits_per_sample_format_ptr(sample_fmt);
}

void (*avcodec_init_ptr)(void) = NULL;
void avcodec_init(void) {
  avcodec_init_ptr();
}

AVCodec* (*avcodec_find_decoder_ptr)(enum CodecID id) = NULL;
AVCodec* avcodec_find_decoder(enum CodecID id) {
  return avcodec_find_decoder_ptr(id);
}

int (*avcodec_thread_init_ptr)(AVCodecContext* s, int thread_count) = NULL;
int avcodec_thread_init(AVCodecContext* s, int thread_count) {
  return avcodec_thread_init_ptr(s, thread_count);
}

int (*avcodec_open_ptr)(AVCodecContext* avctx, AVCodec* codec) = NULL;
int avcodec_open(AVCodecContext* avctx, AVCodec* codec) {
  return avcodec_open_ptr(avctx, codec);
}

AVFrame* (*avcodec_alloc_frame_ptr)(void) = NULL;
AVFrame* avcodec_alloc_frame(void) {
  return avcodec_alloc_frame_ptr();
}

int (*avcodec_decode_audio2_ptr)(AVCodecContext* avctx, int16_t* samples,
                                 int* frame_size_ptr, const uint8_t* buf,
                                 int buf_size) = NULL;
int avcodec_decode_audio2(AVCodecContext* avctx, int16_t* samples,
                          int* frame_size_ptr,
                          const uint8_t* buf, int buf_size) {

  return avcodec_decode_audio2_ptr(avctx, samples, frame_size_ptr, buf,
                                   buf_size);
}


int (*avcodec_decode_video_ptr)(AVCodecContext* avctx, AVFrame* picture,
                                int* got_picture_ptr, const uint8_t* buf,
                                int buf_size) = NULL;
int avcodec_decode_video(AVCodecContext* avctx, AVFrame* picture,
                         int* got_picture_ptr, const uint8_t* buf,
                         int buf_size) {
  return avcodec_decode_video_ptr(avctx, picture, got_picture_ptr, buf,
                                  buf_size);
}


void (*av_register_all_ptr)(void);
void av_register_all(void) {
  av_register_all_ptr();
}

int (*av_open_input_file_ptr)(AVFormatContext** ic_ptr, const char* filename,
                              AVInputFormat* fmt, int buf_size,
                              AVFormatParameters* ap) = NULL;
int av_open_input_file(AVFormatContext** ic_ptr, const char* filename,
                       AVInputFormat* fmt, int buf_size,
                       AVFormatParameters* ap) {
  return av_open_input_file_ptr(ic_ptr, filename, fmt, buf_size, ap);
}

int (*av_find_stream_info_ptr)(AVFormatContext* ic) = NULL;
int av_find_stream_info(AVFormatContext* ic) {
  return av_find_stream_info_ptr(ic);
}

int (*av_read_frame_ptr)(AVFormatContext* s, AVPacket* pkt) = NULL;
int av_read_frame(AVFormatContext* s, AVPacket* pkt) {
  return av_read_frame_ptr(s, pkt);
}

int (*av_seek_frame_ptr)(AVFormatContext* s, int stream_index,
                         int64_t timestamp, int flags) = NULL;
int av_seek_frame(AVFormatContext* s, int stream_index,
                  int64_t timestamp, int flags) {
  return av_seek_frame_ptr(s, stream_index, timestamp, flags);
}

int (*av_register_protocol_ptr)(URLProtocol* protocol) = NULL;
int av_register_protocol(URLProtocol* protocol) {
  return av_register_protocol_ptr(protocol);
}


void* (*av_malloc_ptr)(unsigned int size) = NULL;
void* av_malloc(unsigned int size) {
  return av_malloc_ptr(size);
}

void (*av_free_ptr)(void* ptr) = NULL;
void av_free(void* ptr) {
  return av_free_ptr(ptr);
}

}  // extern "C"


namespace media {

namespace {

enum FFmpegDSOKeys {
  FILE_LIBAVCODEC,       // full path to libavcodec media decoding library.
  FILE_LIBAVFORMAT,      // full path to libavformat media parsing library.
  FILE_LIBAVUTIL,        // full path to libavutil media utility library.
};

// Retrieves the DLLName for the given key.
std::string GetDSOName(FFmpegDSOKeys dso_key) {
  // TODO(ajwong): Do we want to lock to a specific ffmpeg version?
  // TODO(port): These library names are incorrect for mac.  We need .dynlib
  // suffixes.
  switch (dso_key) {
    case FILE_LIBAVCODEC:
      return FILE_PATH_LITERAL("libavcodec.so.52");
    case FILE_LIBAVFORMAT:
      return FILE_PATH_LITERAL("libavformat.so.52");
    case FILE_LIBAVUTIL:
      return FILE_PATH_LITERAL("libavutil.so.50");
    default:
      LOG(DFATAL) << "Invalid DSO key requested: " << dso_key;
      return FILE_PATH_LITERAL("");
  }
}

}  // namespace

// Attempts to initialize the media library (loading DLLs, DSOs, etc.).
// Returns true if everything was successfully initialized, false otherwise.
bool InitializeMediaLibrary(const FilePath& module_dir) {
  // TODO(ajwong): We need error resolution.
  FFmpegDSOKeys path_keys[] = {
    FILE_LIBAVCODEC,
    FILE_LIBAVFORMAT,
    FILE_LIBAVUTIL
  };
  void* libs[arraysize(path_keys)] = {};
  for (size_t i = 0; i < arraysize(path_keys); ++i) {
    FilePath path = module_dir.Append(GetDSOName(path_keys[i]));
    libs[i] = dlopen(path.value().c_str(), RTLD_LAZY);
    if (!libs[i])
      break;
  }

  // Check that we loaded all libraries successfully.  We only need to check the
  // last array element because the loop above breaks on any failure.
  if (libs[arraysize(libs)-1] == NULL) {
    // Free any loaded libraries if we weren't successful.
    for (size_t i = 0; i < arraysize(libs) && libs[i] != NULL; ++i) {
      dlclose(libs[i]);
      libs[i] = NULL;  // Just to be safe.
    }
    return false;
  }

  // TODO(ajwong): Extract this to somewhere saner, and hopefully
  // autogenerate the bindings from the .def files.  Having all this
  // code here is incredibly ugly.
  av_get_bits_per_sample_format_ptr =
      reinterpret_cast<int (*)(enum SampleFormat)>(
          dlsym(libs[FILE_LIBAVCODEC], "av_get_bits_per_sample_format"));
  avcodec_init_ptr =
      reinterpret_cast<void(*)(void)>(
          dlsym(libs[FILE_LIBAVCODEC], "avcodec_init"));
  avcodec_find_decoder_ptr =
      reinterpret_cast<AVCodec* (*)(enum CodecID)>(
          dlsym(libs[FILE_LIBAVCODEC], "avcodec_find_decoder"));
  avcodec_thread_init_ptr =
      reinterpret_cast<int (*)(AVCodecContext*, int)>(
          dlsym(libs[FILE_LIBAVCODEC], "avcodec_thread_init"));
  avcodec_open_ptr =
      reinterpret_cast<int (*)(AVCodecContext*, AVCodec*)>(
          dlsym(libs[FILE_LIBAVCODEC], "avcodec_open"));
  avcodec_alloc_frame_ptr =
      reinterpret_cast<AVFrame* (*)(void)>(
          dlsym(libs[FILE_LIBAVCODEC], "avcodec_alloc_frame"));
  avcodec_decode_audio2_ptr =
      reinterpret_cast<int (*)(AVCodecContext*, int16_t*, int*,
                               const uint8_t*, int)>(
          dlsym(libs[FILE_LIBAVCODEC], "avcodec_decode_audio2"));
  avcodec_decode_video_ptr =
      reinterpret_cast<int (*)(AVCodecContext*, AVFrame*, int*,
                               const uint8_t*, int)>(
          dlsym(libs[FILE_LIBAVCODEC], "avcodec_decode_video"));

  av_register_all_ptr =
      reinterpret_cast<void(*)(void)>(
          dlsym(libs[FILE_LIBAVFORMAT], "av_register_all"));
  av_open_input_file_ptr =
      reinterpret_cast<int (*)(AVFormatContext**, const char*,
                               AVInputFormat*, int,
                               AVFormatParameters*)>(
          dlsym(libs[FILE_LIBAVFORMAT], "av_open_input_file"));
  av_find_stream_info_ptr =
      reinterpret_cast<int (*)(AVFormatContext*)>(
          dlsym(libs[FILE_LIBAVFORMAT], "av_find_stream_info"));
  av_read_frame_ptr =
      reinterpret_cast<int (*)(AVFormatContext*, AVPacket*)>(
          dlsym(libs[FILE_LIBAVFORMAT], "av_read_frame"));
  av_seek_frame_ptr =
      reinterpret_cast<int (*)(AVFormatContext*, int, int64_t, int)>(
          dlsym(libs[FILE_LIBAVFORMAT], "av_seek_frame"));
  av_register_protocol_ptr =
      reinterpret_cast<int (*)(URLProtocol*)>(
          dlsym(libs[FILE_LIBAVFORMAT], "av_register_protocol"));

  av_malloc_ptr =
      reinterpret_cast<void* (*)(unsigned int)>(
          dlsym(libs[FILE_LIBAVUTIL], "av_malloc"));
  av_free_ptr =
      reinterpret_cast<void (*)(void*)>(
          dlsym(libs[FILE_LIBAVUTIL], "av_free"));

  // Check that all the symbols were loaded correctly before returning true.
  if (av_get_bits_per_sample_format_ptr &&
      avcodec_init_ptr &&
      avcodec_find_decoder_ptr &&
      avcodec_thread_init_ptr &&
      avcodec_open_ptr &&
      avcodec_alloc_frame_ptr &&
      avcodec_decode_audio2_ptr &&
      avcodec_decode_video_ptr &&

      av_register_all_ptr &&
      av_open_input_file_ptr &&
      av_find_stream_info_ptr &&
      av_read_frame_ptr &&
      av_seek_frame_ptr &&
      av_register_protocol_ptr &&

      av_malloc_ptr &&
      av_free_ptr) {
    return true;
  }

  return false;
}

}  // namespace media
