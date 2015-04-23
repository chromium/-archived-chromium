// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifdef _OPENMP
#include <omp.h>
#endif

#include "base/at_exit.h"
#include "base/string_util.h"
#include "media/base/factory.h"
#include "media/base/pipeline_impl.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/file_data_source.h"
#include "media/filters/null_audio_renderer.h"
#include "media/player/movie.h"
#include "media/player/stdafx.h"
#include "media/player/wtl_renderer.h"
// view.h must come after wtl_renderer.h.
#include "media/player/view.h"

using media::AudioRendererImpl;
using media::FFmpegAudioDecoder;
using media::FFmpegDemuxer;
using media::FFmpegVideoDecoder;
using media::FileDataSource;
using media::FilterFactoryCollection;
using media::PipelineImpl;

namespace media {

Movie::Movie()
    : enable_audio_(true),
      enable_swscaler_(false),
      enable_draw_(true),
      enable_dump_yuv_file_(false),
      enable_pause_(false),
      enable_openmp_(false),
      max_threads_(0),
      play_rate_(1.0f),
      movie_dib_(NULL),
      movie_hwnd_(0) {
}

Movie::~Movie() {
}

bool Movie::IsOpen() {
  return pipeline_.get() != NULL;
}

void Movie::SetFrameBuffer(HBITMAP hbmp, HWND hwnd) {
  movie_dib_ = hbmp;
  movie_hwnd_ = hwnd;
}

bool Movie::Open(const wchar_t* url, WtlVideoRenderer* video_renderer) {
  // Close previous movie.
  if (pipeline_.get()) {
    Close();
  }

  // Create our filter factories.
  scoped_refptr<FilterFactoryCollection> factories =
      new FilterFactoryCollection();
  factories->AddFactory(FileDataSource::CreateFactory());
  factories->AddFactory(FFmpegAudioDecoder::CreateFactory());
  factories->AddFactory(FFmpegDemuxer::CreateFilterFactory());
  factories->AddFactory(FFmpegVideoDecoder::CreateFactory());

  if (enable_audio_) {
    factories->AddFactory(AudioRendererImpl::CreateFilterFactory());
  } else {
    factories->AddFactory(media::NullAudioRenderer::CreateFilterFactory());
  }
  factories->AddFactory(
      new media::InstanceFilterFactory<WtlVideoRenderer>(video_renderer));

  pipeline_.reset(new PipelineImpl());

  // Create and start our pipeline.
  pipeline_->Start(factories.get(), WideToUTF8(std::wstring(url)), NULL);
  while (true) {
    PlatformThread::Sleep(100);
    if (pipeline_->IsInitialized())
      break;
    if (pipeline_->GetError() != media::PIPELINE_OK)
      return false;
  }
  pipeline_->SetPlaybackRate(play_rate_);
  return true;
}

void Movie::Play(float rate) {
  // Begin playback.
  if (pipeline_.get())
    pipeline_->SetPlaybackRate(enable_pause_ ? 0.0f : rate);
  if (rate > 0.0f)
    play_rate_ = rate;
}

// Get playback rate.
float Movie::GetPlayRate() {
  return play_rate_;
}

// Get movie duration in seconds.
float Movie::GetDuration() {
  float duration = 0.f;
  if (pipeline_.get())
    duration = (pipeline_->GetDuration()).InMicroseconds() / 1000000.0f;
  return duration;
}

// Get current movie position in seconds.
float Movie::GetPosition() {
  float position = 0.f;
  if (pipeline_.get())
    position = (pipeline_->GetTime()).InMicroseconds() / 1000000.0f;
  return position;
}

// Set current movie position in seconds.
void Movie::SetPosition(float position) {
  int64 us = static_cast<int64>(position * 1000000);
  base::TimeDelta time = base::TimeDelta::FromMicroseconds(us);
  if (pipeline_.get())
    pipeline_->Seek(time, NULL);
}


// Set playback pause.
void Movie::SetPause(bool pause) {
  enable_pause_ = pause;
  Play(play_rate_);
}

// Get playback pause state.
bool Movie::GetPause() {
  return enable_pause_;
}

void Movie::SetAudioEnable(bool enable_audio) {
  enable_audio_ = enable_audio;
}

bool Movie::GetAudioEnable() {
  return enable_audio_;
}

void Movie::SetDrawEnable(bool enable_draw) {
  enable_draw_ = enable_draw;
}

bool Movie::GetDrawEnable() {
  return enable_draw_;
}

void Movie::SetSwscalerEnable(bool enable_swscaler) {
  enable_swscaler_ = enable_swscaler;
}

bool Movie::GetSwscalerEnable() {
  return enable_swscaler_;
}

void Movie::SetDumpYuvFileEnable(bool enable_dump_yuv_file) {
  enable_dump_yuv_file_ = enable_dump_yuv_file;
}

bool Movie::GetDumpYuvFileEnable() {
  return enable_dump_yuv_file_;
}

// Enable/Disable OpenMP.
void Movie::SetOpenMpEnable(bool enable_openmp) {
#ifdef _OPENMP
  if (!max_threads_)
    max_threads_ = omp_get_max_threads();

  enable_openmp_ = enable_openmp;
  omp_set_num_threads(enable_openmp_ ? max_threads_ : 1);
#endif
}

// Get Enable/Disable OpenMP state.
bool Movie::GetOpenMpEnable() {
  return enable_openmp_;
}

// Teardown.
void Movie::Close() {
  if (pipeline_.get()) {
    pipeline_->Stop();
    pipeline_.reset();
  }
}

}  // namespace media
