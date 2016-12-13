// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Standalone benchmarking application based on FFmpeg.  This tool is used to
// measure decoding performance between different FFmpeg compile and run-time
// options.  We also use this tool to measure performance regressions when
// testing newer builds of FFmpeg from trunk.
//
// This tool requires FFMPeg DLL's built with --enable-protocol=file.

#include <iomanip>
#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/base/media.h"
#include "media/filters/ffmpeg_common.h"

namespace switches {
const wchar_t kStream[]                 = L"stream";
const wchar_t kVideoThreads[]           = L"video-threads";
const wchar_t kFast2[]                  = L"fast2";
const wchar_t kSkip[]                   = L"skip";
const wchar_t kFlush[]                  = L"flush";
}  // namespace switches

int main(int argc, const char** argv) {
  base::AtExitManager exit_manager;

  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  std::vector<std::wstring> filenames(cmd_line->GetLooseValues());
  if (filenames.empty()) {
    std::cerr << "Usage: media_bench [OPTIONS] FILE\n"
              << "  --stream=[audio|video]          "
              << "Benchmark either the audio or video stream\n"
              << "  --video-threads=N               "
              << "Decode video using N threads\n"
              << "  --fast2                         "
              << "Enable fast2 flag\n"
              << "  --flush                         "
              << "Flush last frame\n"
              << "  --skip=[1|2|3]                  "
              << "1=loop nonref, 2=loop, 3= frame nonref\n" << std::endl;
    return 1;
  }

  // Initialize our media library (try loading DLLs, etc.) before continuing.
  // We use an empty file path as the parameter to force searching of the
  // default locations for necessary DLLs and DSOs.
  if (media::InitializeMediaLibrary(FilePath()) == false) {
    std::cerr << "Unable to initialize the media library.";
    return 1;
  }

  // Retrieve command line options.
  std::string path(WideToUTF8(filenames[0]));
  CodecType target_codec = CODEC_TYPE_UNKNOWN;
  int video_threads = 0;

  // Determine whether to benchmark audio or video decoding.
  std::wstring stream(cmd_line->GetSwitchValue(switches::kStream));
  if (!stream.empty()) {
    if (stream.compare(L"audio") == 0) {
      target_codec = CODEC_TYPE_AUDIO;
    } else if (stream.compare(L"video") == 0) {
      target_codec = CODEC_TYPE_VIDEO;
    } else {
      std::cerr << "Unknown --stream option " << stream << std::endl;
      return 1;
    }
  }

  // Determine number of threads to use for video decoding (optional).
  std::wstring threads(cmd_line->GetSwitchValue(switches::kVideoThreads));
  if (!threads.empty() &&
      !StringToInt(WideToUTF16Hack(threads), &video_threads)) {
    video_threads = 0;
  }

  bool fast2 = false;
  if (cmd_line->HasSwitch(switches::kFast2)) {
    fast2 = true;
  }

  bool flush = false;
  if (cmd_line->HasSwitch(switches::kFlush)) {
    flush = true;
  }

  int skip = 0;
  if (cmd_line->HasSwitch(switches::kSkip)) {
    std::wstring skip_opt(cmd_line->GetSwitchValue(switches::kSkip));
    if (!StringToInt(WideToUTF16Hack(skip_opt), &skip)) {
      skip = 0;
    }
  }

  // Register FFmpeg and attempt to open file.
  avcodec_init();
  av_register_all();
  AVFormatContext* format_context = NULL;
  if (av_open_input_file(&format_context, path.c_str(), NULL, 0, NULL) < 0) {
    std::cerr << "Could not open " << path << std::endl;
    return 1;
  }

  // Parse a little bit of the stream to fill out the format context.
  if (av_find_stream_info(format_context) < 0) {
    std::cerr << "Could not find stream info for " << path << std::endl;
    return 1;
  }

  // Find our target stream.
  int target_stream = -1;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVCodecContext* codec_context = format_context->streams[i]->codec;
    AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

    // See if we found our target codec.
    if (codec_context->codec_type == target_codec && target_stream < 0) {
      std::cout << "* ";
      target_stream = i;
    } else {
      std::cout << "  ";
    }

    if (codec_context->codec_type == CODEC_TYPE_UNKNOWN) {
      std::cout << "Stream #" << i << ": Unknown" << std::endl;
    } else {
      // Print out stream information
      std::cout << "Stream #" << i << ": " << codec->name << " ("
                << codec->long_name << ")" << std::endl;
    }
  }

  // Only continue if we found our target stream.
  if (target_stream < 0) {
    return 1;
  }

  // Prepare FFmpeg structures.
  AVPacket packet;
  AVCodecContext* codec_context = format_context->streams[target_stream]->codec;
  AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

  if (skip == 1) {
    codec_context->skip_loop_filter = AVDISCARD_NONREF;
  } else if (skip == 2) {
    codec_context->skip_loop_filter = AVDISCARD_ALL;
  } else if (skip == 3) {
    codec_context->skip_loop_filter = AVDISCARD_ALL;
    codec_context->skip_frame = AVDISCARD_NONREF;
  }
  if (fast2) {
    codec_context->flags2 |= CODEC_FLAG2_FAST;
  }

  // Initialize threaded decode.
  if (target_codec == CODEC_TYPE_VIDEO && video_threads > 0) {
    if (avcodec_thread_init(codec_context, video_threads) < 0) {
      std::cerr << "WARNING: Could not initialize threading!\n"
                << "Did you build with pthread/w32thread support?" << std::endl;
    }
  }

  // Initialize our codec.
  if (avcodec_open(codec_context, codec) < 0) {
    std::cerr << "Could not open codec " << codec_context->codec->name
              << std::endl;
    return 1;
  }

  // Buffer used for audio decoding.
  int16* samples =
      reinterpret_cast<int16*>(av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE));

  // Buffer used for video decoding.
  AVFrame* frame = avcodec_alloc_frame();
  if (!frame) {
    std::cerr << "Could not allocate an AVFrame" << std::endl;
    return 1;
  }

  // Stats collector.
  std::vector<double> decode_times;
  decode_times.reserve(4096);
  // Parse through the entire stream until we hit EOF.
  base::TimeTicks start = base::TimeTicks::HighResNow();
  size_t frames = 0;
  int read_result = 0;
  do {
    read_result = av_read_frame(format_context, &packet);

    if (read_result < 0) {
      if (flush) {
        packet.stream_index = target_stream;
        packet.size = 0;
      } else {
        break;
      }
    }

    // Only decode packets from our target stream.
    if (packet.stream_index == target_stream) {
      int result = -1;
      base::TimeTicks decode_start = base::TimeTicks::HighResNow();
      if (target_codec == CODEC_TYPE_AUDIO) {
        int size_out = AVCODEC_MAX_AUDIO_FRAME_SIZE;
        result = avcodec_decode_audio3(codec_context, samples, &size_out,
                                       &packet);
        if (size_out) {
          ++frames;
          read_result = 0;  // Force continuation.
        }
      } else if (target_codec == CODEC_TYPE_VIDEO) {
        int got_picture = 0;
        result = avcodec_decode_video2(codec_context, frame, &got_picture,
                                       &packet);
        if (got_picture) {
          ++frames;
          read_result = 0;  // Force continuation.
        }
      } else {
        NOTREACHED();
      }
      base::TimeDelta delta = base::TimeTicks::HighResNow() - decode_start;

      decode_times.push_back(delta.InMillisecondsF());

      // Make sure our decoding went OK.
      if (result < 0) {
        std::cerr << "Error while decoding" << std::endl;
        return 1;
      }
    }
    // Free our packet.
    av_free_packet(&packet);
  } while (read_result >= 0);
  base::TimeDelta total = base::TimeTicks::HighResNow() - start;

  // Calculate the sum of times.  Note that some of these may be zero.
  double sum = 0;
  for (size_t i = 0; i < decode_times.size(); ++i) {
    sum += decode_times[i];
  }

  // Print our results.
  std::cout.setf(std::ios::fixed);
  std::cout.precision(3);
  std::cout << std::endl;
  std::cout << "     Frames:" << std::setw(10) << frames
            << std::endl;
  std::cout << "      Total:" << std::setw(10) << total.InMillisecondsF()
            << " ms" << std::endl;
  std::cout << "  Summation:" << std::setw(10) << sum
            << " ms" << std::endl;

  if (frames > 0u) {
    // Calculate the average time per frame.
    double average = sum / frames;

    // Calculate the sum of the squared differences.
    // Standard deviation will only be accurate if no threads are used.
    // TODO(fbarchard): Rethink standard deviation calculation.
    double squared_sum = 0;
    for (size_t i = 0; i < frames; ++i) {
      double difference = decode_times[i] - average;
      squared_sum += difference * difference;
    }

    // Calculate the standard deviation (jitter).
    double stddev = sqrt(squared_sum / frames);

    std::cout << "    Average:" << std::setw(10) << average
              << " ms" << std::endl;
    std::cout << "     StdDev:" << std::setw(10) << stddev
              << " ms" << std::endl;
  }
  return 0;
}
