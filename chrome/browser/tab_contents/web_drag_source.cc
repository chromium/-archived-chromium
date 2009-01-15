// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>

#include "chrome/browser/tab_contents/web_drag_source.h"

#include "chrome/browser/render_view_host.h"

namespace {

static void GetCursorPositions(HWND hwnd, CPoint* client, CPoint* screen) {
  GetCursorPos(screen);
  *client = *screen;
  ScreenToClient(hwnd, client);
}

}  // namespace
///////////////////////////////////////////////////////////////////////////////
// WebDragSource, public:

WebDragSource::WebDragSource(HWND source_hwnd,
                             RenderViewHost* render_view_host)
    : BaseDragSource(),
      source_hwnd_(source_hwnd),
      render_view_host_(render_view_host) {
}

void WebDragSource::OnDragSourceDrop() {
  CPoint client;
  CPoint screen;
  GetCursorPositions(source_hwnd_, &client, &screen);
  render_view_host_->DragSourceEndedAt(client.x, client.y, screen.x, screen.y);
}

void WebDragSource::OnDragSourceMove() {
  CPoint client;
  CPoint screen;
  GetCursorPositions(source_hwnd_, &client, &screen);
  render_view_host_->DragSourceMovedTo(client.x, client.y, screen.x, screen.y);
}

