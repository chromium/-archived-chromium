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

  // Enable/Disable audio.
  void SetAudioEnable(bool enable_audio);

  // Get Enable/Disable audio state.
  bool GetAudioEnable();

  // Set buffer to render into.
  void SetFrameBuffer(HBITMAP hbmp, HWND hwnd);

  // Close movie.
  void Close();

  // Query if movie is currently open.
  bool IsOpen();

 private:
  // Only allow Singleton to create and delete Movie.
  friend struct DefaultSingletonTraits<Movie>;
  Movie();
  virtual ~Movie();

  scoped_ptr<PipelineImpl> pipeline_;
  HBITMAP movie_dib_;
  HWND movie_hwnd_;
  bool enable_audio_;
  float play_rate_;

  DISALLOW_COPY_AND_ASSIGN(Movie);
};

}  // namespace media

#endif  // MEDIA_PLAYER_MOVIE_H_

