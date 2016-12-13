// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Movie class for WTL forms to call to control the media pipeline.

#ifndef MEDIA_PLAYER_MOVIE_H_
#define MEDIA_PLAYER_MOVIE_H_

#include <tchar.h>

#include "base/scoped_ptr.h"
#include "base/singleton.h"

class WtlVideoRenderer;

namespace media {

class PipelineImpl;

class Movie : public Singleton<Movie> {
 public:
  // Open a movie.
  bool Open(const wchar_t* url, WtlVideoRenderer* video_renderer);

  // Set playback rate.
  void Play(float rate);

  // Set playback rate.
  float GetPlayRate();

  // Get movie duration in seconds.
  float GetDuration();

  // Get current movie position in seconds.
  float GetPosition();

  // Set current movie position in seconds.
  void SetPosition(float position);

  // Set playback pause.
  void SetPause(bool pause);

  // Get playback pause state.
  bool GetPause();

  // Set buffer to render into.
  void SetFrameBuffer(HBITMAP hbmp, HWND hwnd);

  // Close movie.
  void Close();

  // Query if movie is currently open.
  bool IsOpen();

  // Enable/Disable audio.
  void SetAudioEnable(bool enable_audio);

  // Get Enable/Disable audio state.
  bool GetAudioEnable();

  // Enable/Disable draw.
  void SetDrawEnable(bool enable_draw);

  // Get Enable/Disable draw state.
  bool GetDrawEnable();

  // Enable/Disable swscaler.
  void SetSwscalerEnable(bool enable_swscaler);

  // Get Enable/Disable swscaler state.
  bool GetSwscalerEnable();

  // Enable/Disable dump yuv file.
  void SetDumpYuvFileEnable(bool enable_dump_yuv_file);

  // Get Enable/Disable dump yuv file state.
  bool GetDumpYuvFileEnable();

  // Enable/Disable OpenMP.
  void SetOpenMpEnable(bool enable_openmp);

  // Get Enable/Disable OpenMP state.
  bool GetOpenMpEnable();

 private:
  // Only allow Singleton to create and delete Movie.
  friend struct DefaultSingletonTraits<Movie>;
  Movie();
  virtual ~Movie();

  scoped_ptr<PipelineImpl> pipeline_;

  bool enable_audio_;
  bool enable_swscaler_;
  bool enable_draw_;
  bool enable_dump_yuv_file_;
  bool enable_pause_;
  bool enable_openmp_;
  int max_threads_;
  float play_rate_;
  HBITMAP movie_dib_;
  HWND movie_hwnd_;

  DISALLOW_COPY_AND_ASSIGN(Movie);
};

}  // namespace media

#endif  // MEDIA_PLAYER_MOVIE_H_

