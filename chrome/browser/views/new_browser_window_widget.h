// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_NEW_BROWSER_WINDOW_WIDGET_H_
#define CHROME_BROWSER_VIEWS_NEW_BROWSER_WINDOW_WIDGET_H_

#include "views/controls/button/button.h"

class Profile;

namespace views {
class Widget;
}

// NewBrowserWindowWidget creates a window containing a single button that
// when clicked creates a new Browser. NewBrowserWindowWidget shows the window
// from its constructor, and closes it from the destructor.
class NewBrowserWindowWidget : public views::ButtonListener {
 public:
  explicit NewBrowserWindowWidget(Profile* profile);
  ~NewBrowserWindowWidget();

  // ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender);

 private:
  // The profile the browser is created with.
  Profile* profile_;

  // The widget containing the button.
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(NewBrowserWindowWidget);
};

#endif  // CHROME_BROWSER_VIEWS_NEW_BROWSER_WINDOW_WIDGET_H_
