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

#ifndef CHROME_BROWSER_VIEWS_GO_BUTTON_H__
#define CHROME_BROWSER_VIEWS_GO_BUTTON_H__

#include "chrome/views/button.h"
#include "chrome/browser/controller.h"
#include "base/task.h"

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

class GoButton : public ChromeViews::ToggleButton {
 public:
  GoButton(LocationBarView* location_bar, CommandController* controller);
  virtual ~GoButton();

  typedef enum Mode { MODE_GO = 0, MODE_STOP };

  virtual void NotifyClick(int mouse_event_flags);
  virtual void OnMouseExited(const ChromeViews::MouseEvent& e);

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
  CommandController* controller_;
  ButtonListener* listener_;

  // The mode we should be in
  Mode intended_mode_;

  // The currently-visible mode - this may different from the intended mode
  Mode visible_mode_;

  DISALLOW_EVIL_CONSTRUCTORS(GoButton);
};

#endif  // CHROME_BROWSER_VIEWS_GO_BUTTON_H__
