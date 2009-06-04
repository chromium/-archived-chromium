// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_GO_BUTTON_H__
#define CHROME_BROWSER_VIEWS_GO_BUTTON_H__

#include "base/basictypes.h"
#include "base/task.h"
#include "views/controls/button/image_button.h"

class Browser;
class LocationBarView;

////////////////////////////////////////////////////////////////////////////////
//
// GoButton
//
// The go button attached to the toolbar. It shows different tooltips
// according to the content of the location bar and changes to a stop
// button when a page load is in progress. Trickiness comes from the
// desire to have the 'stop' button not change back to 'go' if the user's
// mouse is hovering over it (to prevent mis-clicks).
//
////////////////////////////////////////////////////////////////////////////////

class GoButton : public views::ToggleImageButton,
                 public views::ButtonListener {
 public:
  enum Mode { MODE_GO = 0, MODE_STOP };

  GoButton(LocationBarView* location_bar, Browser* Browser);
  virtual ~GoButton();

  // Ask for a specified button state.  If |force| is true this will be applied
  // immediately.
  void ChangeMode(Mode mode, bool force);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* button);

  // Overridden from views::View:
  virtual void OnMouseExited(const views::MouseEvent& e);
  virtual bool GetTooltipText(int x, int y, std::wstring* tooltip);

 private:
  void OnButtonTimer();

  int button_delay_;
  ScopedRunnableMethodFactory<GoButton> stop_timer_;

  LocationBarView* location_bar_;
  Browser* browser_;

  // The mode we should be in
  Mode intended_mode_;

  // The currently-visible mode - this may different from the intended mode
  Mode visible_mode_;

  DISALLOW_COPY_AND_ASSIGN(GoButton);
};

#endif  // CHROME_BROWSER_VIEWS_GO_BUTTON_H__
