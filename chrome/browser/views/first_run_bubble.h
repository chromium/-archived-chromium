// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FIRST_RUN_BUBBLE_H__
#define CHROME_BROWSER_VIEWS_FIRST_RUN_BUBBLE_H__

#include "base/task.h"
#include "chrome/browser/views/info_bubble.h"

class Profile;

class FirstRunBubble : public InfoBubble,
                       public InfoBubbleDelegate {
 public:
  static FirstRunBubble* Show(Profile* profile, HWND parent_hwnd,
                              const gfx::Rect& position_relative_to);

  FirstRunBubble()
      : enable_window_method_factory_(this),
        has_been_activated_(false) {
  }

  virtual ~FirstRunBubble() {
    // We should have called RevokeAll on the method factory already.
    DCHECK(enable_window_method_factory_.empty());
    enable_window_method_factory_.RevokeAll();
  }

  // Overridden from InfoBubble:
  virtual void OnActivate(UINT action, BOOL minimized, HWND window);

  // InfoBubbleDelegate.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape() { return true; }

 private:
  // Re-enable the parent window.
  void EnableParent();

  // Whether we have already been activated.
  bool has_been_activated_;

  ScopedRunnableMethodFactory<FirstRunBubble> enable_window_method_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(FirstRunBubble);
};

#endif  // CHROME_BROWSER_VIEWS_FIRST_RUN_BUBBLE_H__
