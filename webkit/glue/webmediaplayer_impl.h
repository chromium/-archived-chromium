// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// Wrapper over WebCore::MediaPlayerPrivate. It also would handle resource 
// loading for the internal media player.

#ifndef WEBKIT_GLUE_WEBMEDIAPLAYER_IMPL_H_
#define WEBKIT_GLUE_WEBMEDIAPLAYER_IMPL_H_

#include "ResourceHandleClient.h"

#include "webkit/glue/webmediaplayer.h"

#if ENABLE(VIDEO)

namespace WebCore {
class MediaPlayerPrivate;
class ResourceHandle;
}

namespace webkit_glue {

class WebMediaPlayerDelegate;

class WebMediaPlayerImpl : public WebMediaPlayer,
                           WebCore::ResourceHandleClient {
public:
  WebMediaPlayerImpl(WebCore::MediaPlayerPrivate* media_player_private);

  virtual ~WebMediaPlayerImpl();

  virtual void Initialize(WebMediaPlayerDelegate* delegate);

  // Get the web frame associated with the media player
  virtual WebFrame* GetWebFrame();
  
  // Notify the media player about network state change.
  virtual void NotifynetworkStateChange();

  // Notify the media player about ready state change.
  virtual void NotifyReadyStateChange();

  // Notify the media player about time change.
  virtual void NotifyTimeChange();

  // Notify the media player about volume change.
  virtual void NotifyVolumeChange();

  // Tell the media player to repaint itself.
  virtual void Repaint();

  // Load a media resource.
  virtual void LoadMediaResource(const GURL& url);

  // Cancel loading the media resource.
  virtual void CancelLoad();

  // ResourceHandleClient methods
  void willSendRequest(WebCore::ResourceHandle* handle,
                       WebCore::ResourceRequest& request,
                       const WebCore::ResourceResponse&);
  void didReceiveResponse(WebCore::ResourceHandle* handle,
                          const WebCore::ResourceResponse& response);
  void didReceiveData(WebCore::ResourceHandle* handle, const char *buffer,
                      int length, int);
  void didFinishLoading(WebCore::ResourceHandle* handle);
  void didFail(WebCore::ResourceHandle* handle, const WebCore::ResourceError&);

private:
  WebCore::MediaPlayerPrivate* media_player_private_;
  WebMediaPlayerDelegate* delegate_;
  RefPtr<WebCore::ResourceHandle> resource_handle_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImpl);
};

}  // namespace webkit_glue

#endif // ENABLE(VIDEO)

#endif // ifndef WEBKIT_GLUE_WEBMEDIAPLAYER_H_
