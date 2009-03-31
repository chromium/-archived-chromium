// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// Wrapper over WebCore::MediaPlayerPrivate. It also would handle resource
// loading for the internal media player.

#ifndef WEBKIT_GLUE_WEBMEDIAPLAYER_IMPL_H_
#define WEBKIT_GLUE_WEBMEDIAPLAYER_IMPL_H_

#include "webkit/glue/webmediaplayer.h"

#if ENABLE(VIDEO)

namespace WebCore {
class MediaPlayerPrivate;
class ResourceHandle;
}

namespace webkit_glue {

class WebMediaPlayerDelegate;

class WebMediaPlayerImpl : public WebMediaPlayer {
 public:
  explicit WebMediaPlayerImpl(
      WebCore::MediaPlayerPrivate* media_player_private);

  virtual ~WebMediaPlayerImpl();

  virtual void Initialize(WebMediaPlayerDelegate* delegate);

  // Get the web frame associated with the media player
  virtual WebFrame* GetWebFrame();

  // Notify the media player about network state change.
  virtual void NotifyNetworkStateChange();

  // Notify the media player about ready state change.
  virtual void NotifyReadyStateChange();

  // Notify the media player about time change.
  virtual void NotifyTimeChange();

  // Notify the media player about volume change.
  virtual void NotifyVolumeChange();

  // Notify the media player size of video frame changed.
  virtual void NotifySizeChanged();

  // Notify the media player playback rate has changed.
  virtual void NotifyRateChanged();

  // Notify the media player duration of the media file has changed.
  virtual void NotifyDurationChanged();

  // Tell the media player to repaint itself.
  virtual void Repaint();

 private:
  WebCore::MediaPlayerPrivate* media_player_private_;
  WebMediaPlayerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace webkit_glue

#endif  // ENABLE(VIDEO)

#endif  // WEBKIT_GLUE_WEBMEDIAPLAYER_H_
