// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_FIND_BAR_BRIDGE_H_
#define CHROME_BROWSER_COCOA_FIND_BAR_BRIDGE_H_

#import <Cocoa/Cocoa.h>

#import "base/logging.h"
#import "base/scoped_nsobject.h"
#import "chrome/browser/find_bar.h"

class BrowserWindowCocoa;
class FindBarController;
@class FindBarCocoaController;

// Implementation of FindBar for the Mac.  This class simply passes
// each message along to |cocoa_controller_|.
//
// The initialization here is a bit complicated.  FindBarBridge is
// created by a static method in BrowserWindow.  The FindBarBridge
// constructor creates a FindBarCocoaController, which in turn loads a
// FindBarView from a nib file.  All of this is happening outside of
// the main view hierarchy, so the static method also calls
// BrowserWindowCocoa::AddFindBar() in order to add its FindBarView to
// the cocoa views hierarchy.
//
// Memory ownership is relatively straightfoward.  The FindBarBridge
// object is owned by the Browser.  FindBarCocoaController is retained
// by bother FindBarBridge and BrowserWindowController, since both use it.

class FindBarBridge : public FindBar {
 public:
  FindBarBridge();
  virtual ~FindBarBridge();

  FindBarCocoaController* find_bar_cocoa_controller() {
    return cocoa_controller_.get();
  }

  virtual void SetFindBarController(FindBarController* find_bar_controller) {
    find_bar_controller_ = find_bar_controller;
  }

  virtual FindBarController* GetFindBarController() const {
    DCHECK(find_bar_controller_);
    return find_bar_controller_;
  }

  virtual FindBarTesting* GetFindBarTesting() {
    NOTIMPLEMENTED();
    return NULL;
  }

  // Methods from FindBar.
  virtual void Show();
  virtual void Hide(bool animate);
  virtual void SetFocusAndSelection();
  virtual void ClearResults(const FindNotificationDetails& results);
  virtual void StopAnimation();
  virtual void SetFindText(const string16& find_text);
  virtual void UpdateUIForFindResult(const FindNotificationDetails& result,
                                     const string16& find_text);
  virtual void AudibleAlert();
  virtual gfx::Rect GetDialogPosition(gfx::Rect avoid_overlapping_rect);
  virtual void SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw);
  virtual bool IsFindBarVisible();
  virtual void RestoreSavedFocus();
  virtual void MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                     bool no_redraw);

 private:
  // Pointer to the cocoa controller which manages the cocoa view.  Is
  // never null.
  scoped_nsobject<FindBarCocoaController> cocoa_controller_;

  // Pointer back to the owning controller.
  FindBarController* find_bar_controller_;  // weak, owns us

  DISALLOW_COPY_AND_ASSIGN(FindBarBridge);
};

#endif  // CHROME_BROWSER_COCOA_FIND_BAR_BRIDGE_H_
