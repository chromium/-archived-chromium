// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/player/stdafx.h"
#include "media/player/view.h"

WtlVideoRenderer::WtlVideoRenderer(WtlVideoWindow* window)
    : window_(window) {
}

bool WtlVideoRenderer::OnInitialize(size_t width, size_t height) {
  window_->SetSize(width, height);
  return true;
}

void WtlVideoRenderer::OnPaintNeeded() {
  window_->Invalidate();
}

void WtlVideoRenderer::GetCurrentFrame(
    scoped_refptr<media::VideoFrame>* frame_out) {
  return media::VideoRendererBase::GetCurrentFrame(frame_out);
}
