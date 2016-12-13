// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_HIT_TEST_H_
#define VIEWS_WINDOW_HIT_TEST_H_

// Defines the same symbolic names used by the WM_NCHITTEST Notification under
// win32 (the integer values are not guaranteed to be equivalent). We do this
// because we have a whole bunch of code that deals with window resizing and
// such that requires these values.
enum HitTestCompat {
  HTBORDER = 1,
  HTBOTTOM,
  HTBOTTOMLEFT,
  HTBOTTOMRIGHT,
  HTCAPTION,
  HTCLIENT,
  HTCLOSE,
  HTERROR,
  HTGROWBOX,
  HTHELP,
  HTHSCROLL,
  HTLEFT,
  HTMENU,
  HTMAXBUTTON,
  HTMINBUTTON,
  HTNOWHERE,
  HTREDUCE,
  HTRIGHT,
  HTSIZE,
  HTSYSMENU,
  HTTOP,
  HTTOPLEFT,
  HTTOPRIGHT,
  HTTRANSPARENT,
  HTVSCROLL,
  HTZOOM
};

#endif  // VIEWS_WINDOW_HIT_TEST_H_
