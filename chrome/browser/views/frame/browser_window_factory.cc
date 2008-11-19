// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h" // TODO(beng): remove once done.
#include "chrome/browser/browser_window.h"
#include "chrome/browser/views/frame/aero_glass_frame.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/opaque_frame.h"
#include "chrome/common/win_util.h"

///////////////////////////////////////////////////////////////////////////////
// BrowserWindow, public:

// static
BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser) {
  BrowserView* browser_view = new BrowserView(browser);
  BrowserFrame::CreateForBrowserView(BrowserFrame::GetActiveFrameType(),
                                     browser_view);
  return browser_view;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, public:

// static
BrowserFrame::FrameType BrowserFrame::GetActiveFrameType() {
  return win_util::ShouldUseVistaFrame() ? BrowserFrame::FRAMETYPE_AERO_GLASS
                                         : BrowserFrame::FRAMETYPE_OPAQUE;
}

// static
BrowserFrame* BrowserFrame::CreateForBrowserView(BrowserFrame::FrameType type,
                                                 BrowserView* browser_view) {
  if (type == FRAMETYPE_OPAQUE) {
    OpaqueFrame* frame = new OpaqueFrame(browser_view);
    frame->Init();
    return frame;
  } else if (type == FRAMETYPE_AERO_GLASS) {
    AeroGlassFrame* frame = new AeroGlassFrame(browser_view);
    frame->Init();
    return frame;
  }
  NOTREACHED() << "Unsupported frame type";
  return NULL;
}

