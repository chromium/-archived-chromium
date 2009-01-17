// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_GLUE_WEBMEDIAPLAYER_DELEGATE_H_
#define WEBKIT_GLUE_WEBMEDIAPLAYER_DELEGATE_H_

#include "base/gfx/platform_canvas.h"
#include "webkit/glue/weberror.h"
#include "webkit/glue/webmediaplayer.h"
#include "webkit/glue/webresponse.h"
#include "webkit/glue/weburlrequest.h"

class GURL;

namespace gfx {
class Rect;
}

namespace webkit_glue {

class WebMediaPlayerDelegate {
 public:
  WebMediaPlayerDelegate() {}
  virtual ~WebMediaPlayerDelegate() {}

  virtual void Initialize(WebMediaPlayer *web_media_player) = 0;

  virtual void Load(const GURL& url) = 0;
  virtual void CancelLoad() = 0;

  // Playback controls.
  virtual void Play() = 0;
  virtual void Pause() = 0;
  virtual void Stop() = 0;
  virtual void Seek(float time) = 0;
  virtual void SetEndTime(float time) = 0;
  virtual void SetPlaybackRate(float rate) = 0;
  virtual void SetVolume(float volume) = 0;
  virtual void SetVisible(bool visible) = 0;
  virtual bool IsTotalBytesKnown() = 0;
  virtual float GetMaxTimeBuffered() const = 0;
  virtual float GetMaxTimeSeekable() const = 0;

  // Methods for painting.
  virtual void SetRect(const gfx::Rect& rect) = 0;

  // TODO(hclam): Using paint at the moment, maybe we just need to return a 
  //              SkiaBitmap?
  virtual void Paint(skia::PlatformCanvas *canvas, const gfx::Rect& rect) = 0;

  // True if a video is loaded.
  virtual bool IsVideo() const = 0;

  // Dimension of the video.
  virtual size_t GetWidth() const = 0;
  virtual size_t GetHeight() const = 0;

  // Getters fo playback state.
  virtual bool IsPaused() const = 0;
  virtual bool IsSeeking() const = 0;
  virtual float GetDuration() const = 0;
  virtual float GetCurrentTime() const = 0;
  virtual float GetPlayBackRate() const = 0;
  virtual float GetVolume() const = 0;

  // Get rate of loading the resource.
  virtual int32 GetDataRate() const = 0;

  // Internal states of loading and network.
  virtual WebMediaPlayer::NetworkState GetNetworkState() const = 0;
  virtual WebMediaPlayer::ReadyState GetReadyState() const = 0;

  virtual int64 GetBytesLoaded() const = 0;
  virtual int64 GetTotalBytes() const = 0;

  // Data handlers called from WebMediaPlayer
  virtual void WillSendRequest(WebRequest& request,
                               const WebResponse& response) = 0;
  virtual void DidReceiveData(const char* buf, size_t size) = 0;
  virtual void DidReceiveResponse(const WebResponse& response) = 0;
  virtual void DidFinishLoading() = 0;
  virtual void DidFail(const WebError& error) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerDelegate);
};

}  // namespace webkit_glue

#endif // ifndef WEBKIT_GLUE_WEBMEDIAPLAYER_DELEGATE_H_
