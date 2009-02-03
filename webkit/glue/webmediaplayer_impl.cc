// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "config.h"

#include "FrameView.h"
#include "MediaPlayerPrivateChromium.h"
#include "PlatformString.h"
#include "ResourceRequest.h"
#include "ResourceHandle.h"
#undef LOG

#include "googleurl/src/gurl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/weberror_impl.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webmediaplayer_delegate.h"
#include "webkit/glue/webmediaplayer_impl.h"
#include "webkit/glue/webresponse_impl.h"
#include "webkit/glue/weburlrequest_impl.h"

#if ENABLE(VIDEO)

namespace webkit_glue {

WebMediaPlayerImpl::WebMediaPlayerImpl(
    WebCore::MediaPlayerPrivate* media_player_private)
    : media_player_private_(media_player_private) {
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
}

void WebMediaPlayerImpl::Initialize(WebMediaPlayerDelegate* delegate){
  delegate_ = delegate;
}

WebFrame* WebMediaPlayerImpl::GetWebFrame() {
  if (media_player_private_ && media_player_private_->frameView()->frame()) {
    return WebFrameImpl::FromFrame(
        media_player_private_->frameView()->frame());
  } else {
    return NULL;
  }
}

void WebMediaPlayerImpl::NotifyNetworkStateChange() {
  if (media_player_private_)
    media_player_private_->networkStateChanged();
}

void WebMediaPlayerImpl::NotifyReadyStateChange() {
  if (media_player_private_)
    media_player_private_->readyStateChanged();
}

void WebMediaPlayerImpl::NotifyTimeChange() {
  if (media_player_private_)
    media_player_private_->timeChanged();
}

void WebMediaPlayerImpl::NotifyVolumeChange() {
  if (media_player_private_)
    media_player_private_->volumeChanged();
}

void WebMediaPlayerImpl::Repaint() {
  if (media_player_private_)
    media_player_private_->repaint();
}

}  // namespace webkit_glue

#endif  // ENABLE(VIDEO)
