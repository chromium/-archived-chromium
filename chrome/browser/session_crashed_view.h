// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSION_CRASHED_VIEW_H__
#define CHROME_BROWSER_SESSION_CRASHED_VIEW_H__

#include "chrome/browser/views/info_bar_confirm_view.h"

class Profile;

// SessionCrashedView is used on startup when the last session didn't exit
// cleanly. The user is given the option of restoring the last session.
class SessionCrashedView : public InfoBarConfirmView {
 public:
  explicit SessionCrashedView(Profile* profile);
  virtual ~SessionCrashedView();

  virtual void SessionCrashedView::OKButtonPressed();

 private:
  void Init();

  Profile* profile_;

  DISALLOW_EVIL_CONSTRUCTORS(SessionCrashedView);
};

#endif  // CHROME_BROWSER_SESSION_CRASHED_VIEW_H__

