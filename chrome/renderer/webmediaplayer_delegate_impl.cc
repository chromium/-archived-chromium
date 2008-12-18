// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

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
  // TODO(hclam): delegate to google's media player
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
