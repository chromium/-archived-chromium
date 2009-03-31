// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_GLUE_WEBMEDIAPLAYER_H_
#define WEBKIT_GLUE_WEBMEDIAPLAYER_H_

#include "base/basictypes.h"

class WebFrame;

namespace webkit_glue {

class WebMediaPlayerDelegate;

class WebMediaPlayer {
public:
  enum NetworkState {
    EMPTY,
    IDLE,
    LOADING,
    LOADED,
    FORMAT_ERROR,
    NETWORK_ERROR,
    DECODE_ERROR,
  };

  enum ReadyState {
    HAVE_NOTHING,
    HAVE_METADATA,
    HAVE_CURRENT_DATA,
    HAVE_FUTURE_DATA,
    HAVE_ENOUGH_DATA,
  };

  WebMediaPlayer() {}
  virtual ~WebMediaPlayer() {}

  virtual void Initialize(WebMediaPlayerDelegate* delegate) = 0;

  // Get the web frame associated with the media player
  virtual WebFrame* GetWebFrame() = 0;

  // Notify the media player about network state change.
  virtual void NotifyNetworkStateChange() = 0;

  // Notify the media player about ready state change.
  virtual void NotifyReadyStateChange() = 0;

  // Notify the media player about time change.
  virtual void NotifyTimeChange() = 0;

  // Notify the media player about volume change.
  virtual void NotifyVolumeChange() = 0;

  // Notify the media player size of video frame changed.
  virtual void NotifySizeChanged() = 0;

  // Notify the media player playback rate has changed.
  virtual void NotifyRateChanged() = 0;

  // Notify the media player duration of the media file has changed.
  virtual void NotifyDurationChanged() = 0;

  // Tell the media player to repaint itself.
  virtual void Repaint() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayer);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBMEDIAPLAYER_H_
