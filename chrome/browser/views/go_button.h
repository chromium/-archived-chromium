// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_GO_BUTTON_H__
#define CHROME_BROWSER_VIEWS_GO_BUTTON_H__

#include "chrome/views/button.h"
#include "base/task.h"

class CommandUpdater;
class LocationBarView;

////////////////////////////////////////////////////////////////////////////////
//
// GoButton
//
// The go button attached to the toolbar. It shows different tooltips
// according to the content of the location bar and changes to a stop
// button when a page load is in progress. Trickiness comes from the
// desire to have the 'stop' button not change back to 'go' if the user's
// mouse is hovering over it (to prevent misclicks).
//
////////////////////////////////////////////////////////////////////////////////

class GoButton : public views::ToggleButton {
 public:
  // TODO(beng): get rid of the command updater param and instead have a
  //             delegate.
  GoButton(LocationBarView* location_bar, CommandUpdater* command_updater);
  virtual ~GoButton();

  typedef enum Mode { MODE_GO = 0, MODE_STOP };

  virtual void NotifyClick(int mouse_event_flags);
  virtual void OnMouseExited(const views::MouseEvent& e);

  // Force the button state
  void ChangeMode(Mode mode);

  // Ask for a specified button state. This is commonly called by the Browser
  // when page load state changes.
  void ScheduleChangeMode(Mode mode);

  virtual bool GetTooltipText(int x, int y, std::wstring* tooltip);

 private:
  void OnButtonTimer();

  int button_delay_;
  ScopedRunnableMethodFactory<GoButton> stop_timer_;

  LocationBarView* location_bar_;
  CommandUpdater* command_updater_;
  ButtonListener* listener_;

  // The mode we should be in
  Mode intended_mode_;

  // The currently-visible mode - this may different from the intended mode
  Mode visible_mode_;

  DISALLOW_EVIL_CONSTRUCTORS(GoButton);
};

#endif  // CHROME_BROWSER_VIEWS_GO_BUTTON_H__
