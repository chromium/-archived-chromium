// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/media/video_renderer_impl.h"

VideoRendererImpl::VideoRendererImpl(WebMediaPlayerDelegateImpl* delegate)
    : delegate_(delegate) {
}

VideoRendererImpl::~VideoRendererImpl() {
}

void VideoRendererImpl::Stop() {
  // TODO(scherkus): implement Stop.
  NOTIMPLEMENTED();
}

bool VideoRendererImpl::Initialize(media::VideoDecoder* decoder) {
  // TODO(scherkus): implement Initialize.
  NOTIMPLEMENTED();
  return false;
}

bool VideoRendererImpl::IsMediaFormatSupported(
    const media::MediaFormat* format) {
  // TODO(hclam): check the format correctly.
  return true;
}

void VideoRendererImpl::Paint(skia::PlatformCanvas *canvas,
                              const gfx::Rect& rect) {
  // TODO(hclam): add stuff here.
}

void VideoRendererImpl::SetRect(const gfx::Rect& rect) {
  // TODO(hclam): add stuff here.
}
