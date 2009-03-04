// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_STAR_TOGGLE_H_
#define CHROME_BROWSER_VIEWS_STAR_TOGGLE_H_

#include "chrome/views/view.h"
#include "chrome/views/event.h"

class SkBitmap;

////////////////////////////////////////////////////////////////////////////////
//
// A view subclass to implement the star button. The star button notifies its
// Delegate when the state changes.
//
////////////////////////////////////////////////////////////////////////////////
class StarToggle : public views::View {
 public:
  class Delegate {
   public:
    // Called when the star is toggled.
    virtual void StarStateChanged(bool state) = 0;
  };

  explicit StarToggle(Delegate* delegate);
  virtual ~StarToggle();

  // Set whether the star is checked.
  void SetState(bool s);
  bool GetState() const;

  // If true (the default) the state is immediately changed on a mouse release.
  // If false, on mouse release the delegate is notified, but the state is not
  // changed.
  void set_change_state_immediately(bool value) {
    change_state_immediately_ = value;
  }

  // Check/uncheck the star.
  void SwitchState();

  // Overriden from view.
  void Paint(ChromeCanvas* canvas);
  gfx::Size GetPreferredSize();
  virtual bool OnMousePressed(const views::MouseEvent& e);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& e, bool canceled);
  bool OnKeyPressed(const views::KeyEvent& e);

 private:
  // The state.
  bool state_;

  // Our bitmap.
  SkBitmap* state_off_;
  SkBitmap* state_on_;

  // Parent to be notified.
  Delegate* delegate_;

  // See note in setter.
  bool change_state_immediately_;

  DISALLOW_EVIL_CONSTRUCTORS(StarToggle);
};

#endif  // CHROME_BROWSER_VIEWS_STAR_TOGGLE_H_
