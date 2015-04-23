// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/drag_delegate.h"

#include <atltypes.h>

#include "webkit/api/public/WebPoint.h"
#include "webkit/glue/webview.h"

using WebKit::WebPoint;

namespace {

void GetCursorPositions(HWND hwnd, CPoint* client, CPoint* screen) {
  // GetCursorPos will fail if the input desktop isn't the current desktop.
  // See http://b/1173534. (0,0) is wrong, but better than uninitialized.
  if (!GetCursorPos(screen))
    screen->SetPoint(0, 0);

  *client = *screen;
  ScreenToClient(hwnd, client);
}

}  // anonymous namespace

void TestDragDelegate::OnDragSourceCancel() {
  OnDragSourceDrop();
}

void TestDragDelegate::OnDragSourceDrop() {
  CPoint client;
  CPoint screen;
  GetCursorPositions(source_hwnd_, &client, &screen);
  webview_->DragSourceEndedAt(WebPoint(client.x, client.y),
                              WebPoint(screen.x, screen.y));
}

void TestDragDelegate::OnDragSourceMove() {
  CPoint client;
  CPoint screen;
  GetCursorPositions(source_hwnd_, &client, &screen);
  webview_->DragSourceMovedTo(WebPoint(client.x, client.y),
                              WebPoint(screen.x, screen.y));
}
