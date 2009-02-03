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
    LOAD_FAILED,
    LOADING,
    LOADED_META_DATA,
    LOADED_FIRST_FRAME,
    LOADED
  };

  enum ReadyState {
    DATA_UNAVAILABLE,
    CAN_SHOW_CURRENT_FRAME,
    CAN_PLAY,
    CAN_PLAY_THROUGH
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

  // Tell the media player to repaint itself.
  virtual void Repaint() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayer);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBMEDIAPLAYER_H_
