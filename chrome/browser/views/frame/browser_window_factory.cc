// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h" // TODO(beng): remove once done.
#include "chrome/browser/browser_window.h"
#include "chrome/browser/frame_util.h"
#include "chrome/browser/views/frame/aero_glass_frame.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/browser_view2.h"
#include "chrome/browser/views/frame/opaque_frame.h"
#include "chrome/common/win_util.h"

///////////////////////////////////////////////////////////////////////////////
// BrowserWindow, public:

// static
BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser,
                                                  const gfx::Rect& bounds,
                                                  int show_command) {
  // TODO(beng): fix this hack.
  //             To get us off the ground and allow us to incrementally migrate
  //             BrowserWindow functionality from XP/VistaFrame to BrowserView,
  //             all objects need to implement the BrowserWindow interface.
  //             However BrowserView is the one that Browser has a ref to, and
  //             calls that BrowserView can't perform directly are passed on to
  //             its frame. Eventually this will be better, I promise.
  if (g_browser_process->IsUsingNewFrames()) {
    BrowserView2* browser_view = new BrowserView2(browser);
    BrowserFrame::CreateForBrowserView(BrowserFrame::GetActiveFrameType(),
                                       browser_view, bounds, show_command);
    return browser_view;
  }
  BrowserWindow* window = FrameUtil::CreateBrowserWindow(bounds, browser);
  return window->GetBrowserView();
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
                                                 BrowserView2* browser_view,
                                                 const gfx::Rect& bounds,
                                                 int show_command) {
  if (type == FRAMETYPE_OPAQUE) {
    OpaqueFrame* frame = new OpaqueFrame(browser_view);
    frame->Init(NULL, bounds);
    return frame;
  } else if (type == FRAMETYPE_AERO_GLASS) {
    AeroGlassFrame* frame = new AeroGlassFrame(browser_view);
    frame->Init(bounds);
    return frame;
  }
  NOTREACHED() << "Unsupported frame type";
  return NULL;
}

