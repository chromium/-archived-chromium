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

#ifndef CHROME_BROWSER_VIEWS_STAR_TOGGLE_H__
#define CHROME_BROWSER_VIEWS_STAR_TOGGLE_H__

#include "chrome/views/view.h"
#include "chrome/views/event.h"

////////////////////////////////////////////////////////////////////////////////
//
// A view subclass to implement the star button. The star button notifies its
// Delegate when the state changes.
//
////////////////////////////////////////////////////////////////////////////////
class StarToggle : public ChromeViews::View {
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
  void GetPreferredSize(CSize* out);
  virtual bool OnMousePressed(const ChromeViews::MouseEvent& e);
  virtual bool OnMouseDragged(const ChromeViews::MouseEvent& event);
  virtual void OnMouseReleased(const ChromeViews::MouseEvent& e, bool canceled);
  bool OnKeyPressed(const ChromeViews::KeyEvent& e);

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

#endif  // CHROME_BROWSER_VIEWS_STAR_TOGGLE_H__
