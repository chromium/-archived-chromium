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

#ifndef CHROME_BROWSER_VIEW_TAB_ICON_VIEW_H__
#define CHROME_BROWSER_VIEW_TAB_ICON_VIEW_H__

#include "chrome/views/view.h"

class TabContents;

////////////////////////////////////////////////////////////////////////////////
//
// A view to display a tab fav icon or a throbber.
//
////////////////////////////////////////////////////////////////////////////////
class TabIconView : public ChromeViews::View {
 public:
  class TabContentsProvider {
   public:
    // Should return the current tab contents this TabIconView object is
    // representing.
    virtual TabContents* GetCurrentTabContents() = 0;

    // Returns the favicon to display in the icon view
    virtual SkBitmap GetFavIcon() = 0;
  };

  static void InitializeIfNeeded();

  explicit TabIconView(TabContentsProvider* provider);
  virtual ~TabIconView();

  // Invoke whenever the tab state changes or the throbber should update.
  void Update();

  // Set the throbber to the light style (for use on dark backgrounds).
  void set_is_light(bool is_light) { is_light_ = is_light; }

  // Overriden from View
  virtual void Paint(ChromeCanvas* canvas);
  virtual void GetPreferredSize(CSize* out);

 private:
  void PaintThrobber(ChromeCanvas* canvas);
  void PaintFavIcon(ChromeCanvas* canvas, const SkBitmap& bitmap);

  // Our provider of current tab contents.
  TabContentsProvider* provider_;

  // Whether the throbber is running.
  bool throbber_running_;

  // Whether we should display our light or dark style.
  bool is_light_;

  // Current frame of the throbber being painted. This is only used if
  // throbber_running_ is true.
  int throbber_frame_;

  DISALLOW_EVIL_CONSTRUCTORS(TabIconView);
};

#endif  // CHROME_BROWSER_VIEW_TAB_ICON_VIEW_H__
