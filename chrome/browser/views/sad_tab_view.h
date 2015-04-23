// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_SAD_TAB_VIEW_H_
#define CHROME_BROWSER_VIEWS_SAD_TAB_VIEW_H_

#include "app/gfx/font.h"
#include "base/basictypes.h"
#include "views/view.h"

class SkBitmap;

///////////////////////////////////////////////////////////////////////////////
//
// SadTabView
//
//  A views::View subclass used to render the presentation of the crashed
//  "sad tab" in the browser window when a renderer is destroyed unnaturally.
//
//  Note that since this view is not (currently) part of a Container or
//  RootView hierarchy, it cannot respond to events or contain controls that
//  do, right now it is used simply to render. Adding an extra Container to
//  TabContents seemed like a lot of complexity. Ideally, perhaps TabContents'
//  view portion would itself become a Container in the future, then event
//  processing will work.
//
///////////////////////////////////////////////////////////////////////////////
class SadTabView : public views::View {
 public:
  SadTabView();
  virtual ~SadTabView() {}

  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();

 private:
  static void InitClass();

  // Assorted resources for display.
  static SkBitmap* sad_tab_bitmap_;
  static gfx::Font* title_font_;
  static gfx::Font* message_font_;
  static std::wstring title_;
  static std::wstring message_;
  static int title_width_;

  // Regions within the display for different components, populated by
  // Layout().
  gfx::Rect icon_bounds_;
  gfx::Rect title_bounds_;
  gfx::Rect message_bounds_;

  DISALLOW_COPY_AND_ASSIGN(SadTabView);
};

#endif  // CHROME_BROWSER_VIEWS_SAD_TAB_VIEW_H__
