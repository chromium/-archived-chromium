// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "config.h"

#include "FrameView.h"
#include "MediaPlayerPrivateChromium.h"
#undef LOG

#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webmediaplayer_impl.h"

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
  if (media_player_private_->frameView()->frame()) {
    return WebFrameImpl::FromFrame(
        media_player_private_->frameView()->frame());
  } else {
    return NULL;
  }
}

void WebMediaPlayerImpl::NotifynetworkStateChange() {
  media_player_private_->networkStateChanged();
}

void WebMediaPlayerImpl::NotifyReadyStateChange() {
  media_player_private_->readyStateChanged();
}

void WebMediaPlayerImpl::NotifyTimeChange() {
  media_player_private_->timeChanged();
}

void WebMediaPlayerImpl::NotifyVolumeChange() {
  media_player_private_->volumeChanged();
}

void WebMediaPlayerImpl::Repaint() {
  media_player_private_->repaint();
}

}  // namespace webkit_glue

#endif
