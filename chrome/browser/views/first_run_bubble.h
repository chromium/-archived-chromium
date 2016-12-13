// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FIRST_RUN_BUBBLE_H_
#define CHROME_BROWSER_VIEWS_FIRST_RUN_BUBBLE_H_

#include "base/compiler_specific.h"
#include "base/task.h"
#include "chrome/browser/views/info_bubble.h"

class FirstRunBubbleViewBase;
class Profile;

class FirstRunBubble : public InfoBubble,
                       public InfoBubbleDelegate {
 public:
  static FirstRunBubble* Show(Profile* profile, views::Window* window,
                              const gfx::Rect& position_relative_to,
                              bool use_OEM_bubble);

  FirstRunBubble()
      : has_been_activated_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(enable_window_method_factory_(this)),
        view_(NULL) {
  }

  virtual ~FirstRunBubble() {
    // We should have called RevokeAll on the method factory already.
    DCHECK(enable_window_method_factory_.empty());
    enable_window_method_factory_.RevokeAll();
  }

  void set_view(FirstRunBubbleViewBase* view) { view_ = view; }

#if defined(OS_WIN)
  // Overridden from InfoBubble:
  virtual void OnActivate(UINT action, BOOL minimized, HWND window);
#endif

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

  // The view inside the FirstRunBubble.
  FirstRunBubbleViewBase* view_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubble);
};

#endif  // CHROME_BROWSER_VIEWS_FIRST_RUN_BUBBLE_H_
