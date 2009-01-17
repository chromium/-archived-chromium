// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// Delegate calls from WebCore::MediaPlayerPrivate to google's video player.
// It is a friend of VideoStackMediaPlayer, which is the actual media player
// VideoStackMediaPlayer will use WebMediaPlayer to create a resource loader
// and bridges the WebCore::ResourceHandle and media::DataSource, so that
// VideoStackMediaPlayer would have no knowledge of WebCore::ResourceHandle.

#ifndef CHROME_RENDERER_WEBMEDIAPLAYER_DELEGATE_IMPL_H_
#define CHROME_RENDERER_WEBMEDIAPLAYER_DELEGATE_IMPL_H_

#include "webkit/glue/webmediaplayer_delegate.h"

namespace media {
class VideoStackMediaPlayer;
}

class WebMediaPlayerDelegateImpl : public webkit_glue::WebMediaPlayerDelegate {
 public:
  WebMediaPlayerDelegateImpl();
  virtual ~WebMediaPlayerDelegateImpl();

  virtual void Initialize(webkit_glue::WebMediaPlayer* media_player);

  virtual void Load(const GURL& url);
  virtual void CancelLoad();

  // Playback controls.
  virtual void Play();
  virtual void Pause();
  virtual void Stop();
  virtual void Seek(float time);
  virtual void SetEndTime(float time);
  virtual void SetPlaybackRate(float rate);
  virtual void SetVolume(float volume);
  virtual void SetVisible(bool visible);
  virtual bool IsTotalBytesKnown();

  // Methods for painting.
  virtual void SetRect(const gfx::Rect& rect);

  virtual void Paint(skia::PlatformCanvas *canvas, const gfx::Rect& rect);

  // True if a video is loaded.
  virtual bool IsVideo() const { return video_; }

  // Dimension of the video.
  virtual size_t GetWidth() const { return width_; }
  virtual size_t GetHeight() const { return height_; }

  // Getters fo playback state.
  virtual bool IsPaused() const { return paused_; }
  virtual bool IsSeeking() const { return seeking_; }
  virtual float GetDuration() const { return duration_; }
  virtual float GetCurrentTime() const { return current_time_; }
  virtual float GetPlayBackRate() const { return playback_rate_; }
  virtual float GetVolume() const { return volume_; }
  virtual float GetMaxTimeBuffered() const;
  virtual float GetMaxTimeSeekable() const;

  // Get rate of loading the resource.
  virtual int32 GetDataRate() const { return data_rate_; }

  // Internal states of loading and network.
  virtual webkit_glue::WebMediaPlayer::NetworkState GetNetworkState() const {
    return network_state_;
  }
  virtual webkit_glue::WebMediaPlayer::ReadyState GetReadyState() const {
    return ready_state_;
  }

  virtual int64 GetBytesLoaded() const { return bytes_loaded_; }
  virtual int64 GetTotalBytes() const { return total_bytes_; }

  // Data handlers.
  virtual void WillSendRequest(WebRequest& request,
               const WebResponse& response);
  virtual void DidReceiveResponse(const WebResponse& response);
  virtual void DidReceiveData(const char* buf, size_t size);
  virtual void DidFinishLoading();
  virtual void DidFail(const WebError& error);

  // Inline getters.
  webkit_glue::WebMediaPlayer* web_media_player() { return web_media_player_; }

 private:
  int64 bytes_loaded_;
  float current_time_;
  int32 data_rate_;
  float duration_;
  size_t height_;
  webkit_glue::WebMediaPlayer::NetworkState network_state_;
  bool paused_;
  float playback_rate_;
  webkit_glue::WebMediaPlayer::ReadyState ready_state_;
  bool seeking_;
  int64 total_bytes_;
  bool video_;
  media::VideoStackMediaPlayer* video_stack_media_player_;
  bool visible_;
  float volume_;
  webkit_glue::WebMediaPlayer* web_media_player_;
  size_t width_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerDelegateImpl);
};

#endif  // ifndef CHROME_RENDERER_WEBMEDIAPLAYER_DELEGATE_IMPL_H_
