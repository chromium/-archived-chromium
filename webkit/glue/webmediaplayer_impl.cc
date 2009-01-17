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

void WebMediaPlayerImpl::LoadMediaResource(const GURL& url) {
  // Make sure we cancel the previous load request before starting a new one.
  CancelLoad();

  WebCore::Frame* frame = static_cast<WebFrameImpl*>(GetWebFrame())->frame();
  WebCore::ResourceRequest request(webkit_glue::GURLToKURL(url),
                                   WebCore::String(""),
                                   WebCore::UseProtocolCachePolicy);
  request.setTargetType(WebCore::ResourceRequest::TargetIsMedia);
  request.setFrame(frame);

  resource_handle_ = WebCore::ResourceHandle::create(request,
                                                     this,
                                                     frame,
                                                     false,
                                                     true);
}

void WebMediaPlayerImpl::CancelLoad() {
  // Delegate the cancel call to ResourceHandle, this should be enough to
  // stop any further callbacks from ResouceHandle.
  if (resource_handle_) {
    resource_handle_->cancel();
    resource_handle_ = NULL;
  }
}

void WebMediaPlayerImpl::willSendRequest(
    WebCore::ResourceHandle* handle,
    WebCore::ResourceRequest& request,
    const WebCore::ResourceResponse& response) {
  if (delegate_) {
    delegate_->WillSendRequest(WebRequestImpl(request),
                               WebResponseImpl(response));
  }
}

void WebMediaPlayerImpl::didReceiveResponse(
    WebCore::ResourceHandle* handle,
    const WebCore::ResourceResponse& response) {
  if (delegate_) {
    delegate_->DidReceiveResponse(WebResponseImpl(response));
  }
}

void WebMediaPlayerImpl::didReceiveData(WebCore::ResourceHandle* handle,
                                        const char *buffer,
                                        int length,
                                        int bytes_received) {
  if (delegate_) {
    delegate_->DidReceiveData(buffer, length);
  }
}

void WebMediaPlayerImpl::didFinishLoading(WebCore::ResourceHandle* handle) {
  if (delegate_) {
    delegate_->DidFinishLoading();
  }
}

void WebMediaPlayerImpl::didFail(WebCore::ResourceHandle* handle,
                                 const WebCore::ResourceError& error) {
  if (delegate_) {
    delegate_->DidFail(WebErrorImpl(error));
  }
}

}  // namespace webkit_glue

#endif
