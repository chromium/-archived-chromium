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

#ifndef CHROME_BROWSER_VIEWS_SAD_TAB_H__
#define CHROME_BROWSER_VIEWS_SAD_TAB_H__

#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/view.h"

///////////////////////////////////////////////////////////////////////////////
//
// SadTabView
//
//  A ChromeViews::View subclass used to render the presentation of the crashed
//  "sad tab" in the browser window when a renderer is destroyed unnaturally.
//
//  Note that since this view is not (currently) part of a ViewContainer or
//  RootView hierarchy, it cannot respond to events or contain controls that
//  do, right now it is used simply to render. Adding an extra ViewContainer to
//  WebContents seemed like a lot of complexity. Ideally, perhaps WebContents'
//  view portion would itself become a ViewContainer in the future, then event
//  processing will work.
//
///////////////////////////////////////////////////////////////////////////////
class SadTabView : public ChromeViews::View {
 public:
  SadTabView();
  virtual ~SadTabView() {}

  // Overridden from ChromeViews::View:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void Layout();
  virtual void DidChangeBounds(const CRect&, const CRect&);

 private:
  static void InitClass();

  // Assorted resources for display.
  static SkBitmap* sad_tab_bitmap_;
  static ChromeFont title_font_;
  static ChromeFont message_font_;
  static std::wstring title_;
  static std::wstring message_;
  static int title_width_;

  // Regions within the display for different components, populated by
  // Layout().
  gfx::Rect icon_bounds_;
  gfx::Rect title_bounds_;
  gfx::Rect message_bounds_;

  DISALLOW_EVIL_CONSTRUCTORS(SadTabView);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_SAD_TAB_H__
