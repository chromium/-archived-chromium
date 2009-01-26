// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/media/audio_renderer_impl.h"
#include "chrome/renderer/media/data_source_impl.h"
#include "chrome/renderer/media/video_renderer_impl.h"
#include "chrome/renderer/webmediaplayer_delegate_impl.h"

WebMediaPlayerDelegateImpl::WebMediaPlayerDelegateImpl()
    : bytes_loaded_(0),
      current_time_(0.0f),
      data_rate_(0),
      duration_(0.0f),
      height_(0),
      network_state_(webkit_glue::WebMediaPlayer::EMPTY),
      paused_(true),
      playback_rate_(0.0f),
      ready_state_(webkit_glue::WebMediaPlayer::DATA_UNAVAILABLE),
      seeking_(false),
      total_bytes_(0),
      video_(false),
      visible_(false),
      volume_(0.0f),
      web_media_player_(NULL),
      width_(0) {
  // TODO(hclam): initialize the media player here

  // TODO(scherkus): remove these when actually in use -- right now I declare
  // these to force compiler/linker errors to reveal themselves.
  scoped_refptr<AudioRendererImpl> audio_renderer;
  audio_renderer = new AudioRendererImpl();
  scoped_refptr<DataSourceImpl> data_source;
  data_source = new DataSourceImpl();
  scoped_refptr<VideoRendererImpl> video_renderer;
  video_renderer = new VideoRendererImpl();
}

WebMediaPlayerDelegateImpl::~WebMediaPlayerDelegateImpl() {
  // TODO(hclam): do the following things here:
  // 1. Destroy the internal media player
  // 2. Destroy the associated WebMediaPlayer
}

void WebMediaPlayerDelegateImpl::Initialize(
    webkit_glue::WebMediaPlayer* media_player) {
  web_media_player_ = media_player;
}

void WebMediaPlayerDelegateImpl::Load(const GURL& url) {
}

void WebMediaPlayerDelegateImpl::CancelLoad() {
  // TODO(hclam): delegate to google's media player
}

void WebMediaPlayerDelegateImpl::Play() {
  // TODO(hclam): delegate to google's media player
}

void WebMediaPlayerDelegateImpl::Pause() {
  // TODO(hclam): delegate to google's media player
}

void WebMediaPlayerDelegateImpl::Stop() {
  // TODO(hclam): delegate to google's media player
}

void WebMediaPlayerDelegateImpl::Seek(float time) {
  // TODO(hclam): delegate to google's media player
}

void WebMediaPlayerDelegateImpl::SetEndTime(float time) {
  // TODO(hclam): delegate to google's media player
}
 
void WebMediaPlayerDelegateImpl::SetPlaybackRate(float rate) {
  // TODO(hclam): delegate to google's media player
}

void WebMediaPlayerDelegateImpl::SetVolume(float volume) {
  // TODO(hclam): delegate to google's media player
}
 
void WebMediaPlayerDelegateImpl::SetVisible(bool visible) {
  // TODO(hclam): delegate to google's media player
}
 
bool WebMediaPlayerDelegateImpl::IsTotalBytesKnown() {
  // TODO(hclam): delegate to google's media player
  return false;
}

float WebMediaPlayerDelegateImpl::GetMaxTimeBuffered() const {
  // TODO(hclam): delegate to google's media player
  return 0.0f;
}

float WebMediaPlayerDelegateImpl::GetMaxTimeSeekable() const {
  // TODO(hclam): delegate to google's media player
  return 0.0f;
}

void WebMediaPlayerDelegateImpl::SetRect(const gfx::Rect& rect) {
  // TODO(hclam): Figure out what to do here..
}

void WebMediaPlayerDelegateImpl::Paint(skia::PlatformCanvas *canvas,
                                       const gfx::Rect& rect) {
  // TODO(hclam): grab a frame from the internal player and draw it.
}

void WebMediaPlayerDelegateImpl::WillSendRequest(WebRequest& request,
                                                 const WebResponse& response) {
  // TODO(hclam): do we need to change the request?
}

void WebMediaPlayerDelegateImpl::DidReceiveResponse(
    const WebResponse& response) {
  // TODO(hclam): tell the video piepline to prepare for arriving bytes.
}

void WebMediaPlayerDelegateImpl::DidReceiveData(const char* buf, size_t size) {
  // TODO(hclam): direct the data to video pipeline's data source
}

void WebMediaPlayerDelegateImpl::DidFinishLoading() {
  // TODO(hclam): do appropriate actions related to load. We should wait
  // for video pipeline to be initialized and fire a LOADED event.
}

void WebMediaPlayerDelegateImpl::DidFail(const WebError& error) {
  // Simply fires a LOAD_FAILED event.
  // TODO(hclam): will also need to fire a MediaError event.
}
