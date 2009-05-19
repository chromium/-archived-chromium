// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/views/frame/browser_frame_gtk.h"
#include "chrome/browser/views/frame/browser_view.h"

// static (Factory method.)
BrowserFrame* BrowserFrame::Create(BrowserView* browser_view,
                                   Profile* profile) {
  BrowserFrameGtk* frame = new BrowserFrameGtk(browser_view, profile);
  frame->Init();
  return frame;
}

BrowserFrameGtk::BrowserFrameGtk(BrowserView* browser_view, Profile* profile)
    : WindowGtk(browser_view),
      browser_view_(browser_view),
      profile_(profile) {
  browser_view_->set_frame(this);
  GetNonClientView()->SetFrameView(CreateFrameViewForWindow());
  // Don't focus anything on creation, selecting a tab will set the focus.
}

BrowserFrameGtk::~BrowserFrameGtk() {
}

void BrowserFrameGtk::Init() {
  WindowGtk::Init(gfx::Rect());
}

views::Window* BrowserFrameGtk::GetWindow() {
  return this;
}

void BrowserFrameGtk::TabStripCreated(TabStrip* tabstrip) {
  NOTIMPLEMENTED();
}

int BrowserFrameGtk::GetMinimizeButtonOffset() const {
  NOTIMPLEMENTED();
  return 0;
}

gfx::Rect BrowserFrameGtk::GetBoundsForTabStrip(TabStrip* tabstrip) const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void BrowserFrameGtk::UpdateThrobber(bool running) {
  NOTIMPLEMENTED();
}

ThemeProvider* BrowserFrameGtk::GetThemeProviderForFrame() const {
  return profile_->GetThemeProvider();
}
