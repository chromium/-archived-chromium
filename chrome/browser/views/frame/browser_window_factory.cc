// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/frame_util.h"
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
  CommandLine parsed_command_line;
  if (parsed_command_line.HasSwitch(L"magic_browzR")) {
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
    NOTREACHED() << "Aero/Glass not supported yet by magic_browzR switch";
  }
  NOTREACHED() << "Unsupported frame type";
  return NULL;
}
