// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>
#endif

#include "chrome/browser/tab_contents/web_drag_source.h"

#include "chrome/browser/renderer_host/render_view_host.h"

namespace {

static void GetCursorPositions(gfx::NativeWindow wnd, gfx::Point* client,
                               gfx::Point* screen) {
#if defined(OS_WIN)
  POINT cursor_pos;
  GetCursorPos(&cursor_pos);
  screen->SetPoint(cursor_pos.x, cursor_pos.y);
  ScreenToClient(wnd, &cursor_pos);
  client->SetPoint(cursor_pos.x, cursor_pos.y);
#else
  // TODO(port): Get the cursor positions.
  NOTIMPLEMENTED();
#endif
}

}  // namespace
///////////////////////////////////////////////////////////////////////////////
// WebDragSource, public:

WebDragSource::WebDragSource(gfx::NativeWindow source_wnd,
                             RenderViewHost* render_view_host)
    : BaseDragSource(),
      source_wnd_(source_wnd),
      render_view_host_(render_view_host) {
}

void WebDragSource::OnDragSourceCancel() {
  gfx::Point client;
  gfx::Point screen;
  GetCursorPositions(source_wnd_, &client, &screen);
  render_view_host_->DragSourceEndedAt(client.x(), client.y(),
                                       screen.x(), screen.y());
}

void WebDragSource::OnDragSourceDrop() {
  gfx::Point client;
  gfx::Point screen;
  GetCursorPositions(source_wnd_, &client, &screen);
  render_view_host_->DragSourceEndedAt(client.x(), client.y(),
                                       screen.x(), screen.y());
}

void WebDragSource::OnDragSourceMove() {
  gfx::Point client;
  gfx::Point screen;
  GetCursorPositions(source_wnd_, &client, &screen);
  render_view_host_->DragSourceMovedTo(client.x(), client.y(),
                                       screen.x(), screen.y());
}
