// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stand alone media player application used for testing the media library.

#include <iostream>

#include "base/at_exit.h"
#include "media/base/factory.h"
#include "media/base/pipeline_impl.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/coreavc_decoder.h"
#include "media/filters/ffmpeg_audio.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video.h"
#include "media/filters/file_data_source.h"
#include "media/filters/null_audio_renderer.h"
#include "media/player/sdl_renderer.h"

using namespace media;

#include "media/filters/ffmpeg_common.h"

double GetTime() {
  LARGE_INTEGER perf_time, perf_hz;
  QueryPerformanceCounter(&perf_time);
  QueryPerformanceFrequency(&perf_hz);
  return perf_time.QuadPart * 1000.0 / perf_hz.QuadPart;
}

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;

  if (argc <= 1)
    return 1;
  std::string url(argv[1]);

  av_register_all();
  AVFormatContext* format_context = NULL;
  if (av_open_input_file(&format_context, argv[1], NULL, 0, NULL) < 0) {
    std::cerr << "Could not open " << argv[1] << std::endl;
    return 1;
  }

  if (av_find_stream_info(format_context) < 0) {
    std::cerr << "Could not find stream info for " << argv[1] << std::endl;
    return 1;
  }

  size_t aac_stream = -1;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVCodecContext* codec_context = format_context->streams[i]->codec;
    AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);
    if (avcodec_open(codec_context, codec) < 0) {
      std::cerr << "Could not open codec " << codec->name << std::endl;
      return 1;
    }

    if (codec_context->codec_id == CODEC_ID_AAC) {
      aac_stream = i;
    }
  }

  if (aac_stream < 0) {
    std::cerr << "Could not find AAC stream for " << argv[1] << std::endl;
    return 1;
  }

  AVPacket packet;
  uint8* data = reinterpret_cast<uint8*>(av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE));
  int16* samples = reinterpret_cast<int16*>(data);

  double start = GetTime();
  while (av_read_frame(format_context, &packet) >= 0) {
    if (packet.stream_index != aac_stream) {
      continue;
    }

    AVCodecContext* codec_context = format_context->streams[aac_stream]->codec;
    int size_out = AVCODEC_MAX_AUDIO_FRAME_SIZE;  
    int result = avcodec_decode_audio2(codec_context, samples, &size_out,
                                       packet.data, packet.size);
    if (result < 0) {
      std::cerr << "Could not decode AAC" << std::endl;
      return 1;
    }
  }
  double end = GetTime();

  std::cout << "Done: " << (end - start) << "ms" << std::endl;

#if 0
  media::CoreAVCDecoder::DetermineCapabilities();

  // Create our filter factories.
  scoped_refptr<FilterFactoryCollection> factories =
      new FilterFactoryCollection();
  factories->AddFactory(FileDataSource::CreateFactory());
  factories->AddFactory(FFmpegAudioDecoder::CreateFilterFactory());
  factories->AddFactory(FFmpegDemuxer::CreateFilterFactory());
  //factories->AddFactory(CoreAVCDecoder::CreateFilterFactory());
  factories->AddFactory(FFmpegVideoDecoder::CreateFilterFactory());
  //factories->AddFactory(AudioRendererImpl::CreateFilterFactory());
  factories->AddFactory(NullAudioRenderer::CreateFilterFactory());
  factories->AddFactory(SDLVideoRenderer::CreateFilterFactory());

  // Create and start our pipeline.
  PipelineImpl pipeline;
  pipeline.Start(factories.get(), url, NULL);
  while (true) {
    ::Sleep(100);
    if (pipeline.IsInitialized())
      break;
    if (pipeline.GetError() != PIPELINE_OK)
      return 1;
  }

  // Begin playback.
  pipeline.SetPlaybackRate(1.0f);

  // Check for errors during playback.
  while (true) {
    ::Sleep(100);
    if (pipeline.GetError() != PIPELINE_OK)
      break;
  }

  // Teardown.
  pipeline.Stop();
#endif
  return 0;
}
