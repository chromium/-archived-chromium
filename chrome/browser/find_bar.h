// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is an interface for the platform specific FindBar.  It is responsible
// for drawing the FindBar bar on the platform and is owned by the
// FindBarController.

#ifndef CHROME_BROWSER_FIND_BAR_H_
#define CHROME_BROWSER_FIND_BAR_H_

#include "base/gfx/rect.h"

class FindNotificationDetails;
class WebContents;

class FindBar {
 public:
  virtual ~FindBar() { }

  // Shows the find bar. Any previous search string will again be visible.
  virtual void Show() = 0;

  // Hide the find bar.  If |animate| is true, we try to slide the find bar
  // away.
  virtual void Hide(bool animate) = 0;

  // Restore the selected text in the find box and focus it.
  virtual void SetFocusAndSelection() = 0;

  // Clear the text in the find box.
  virtual void ClearResults(const FindNotificationDetails& results) = 0;

  // Stop the animation.
  virtual void StopAnimation() = 0;

  // Set the text in the find box.
  virtual void SetFindText(const std::wstring& find_text) = 0;

  // Updates the FindBar with the find result details contained within the
  // specified |result|.
  virtual void UpdateUIForFindResult(const FindNotificationDetails& result,
                                     const std::wstring& find_text) = 0;

  // Returns the rectangle representing where to position the find bar. It uses
  // GetDialogBounds and positions itself within that, either to the left (if an
  // InfoBar is present) or to the right (no InfoBar). If
  // |avoid_overlapping_rect| is specified, the return value will be a rectangle
  // located immediately to the left of |avoid_overlapping_rect|, as long as
  // there is enough room for the dialog to draw within the bounds. If not, the
  // dialog position returned will overlap |avoid_overlapping_rect|.
  // Note: |avoid_overlapping_rect| is expected to use coordinates relative to
  // the top of the page area, (it will be converted to coordinates relative to
  // the top of the browser window, when comparing against the dialog
  // coordinates). The returned value is relative to the browser window.
  virtual gfx::Rect GetDialogPosition(gfx::Rect avoid_overlapping_rect) = 0;

  // Moves the dialog window to the provided location, moves it to top in the
  // z-order (HWND_TOP, not HWND_TOPMOST) and shows the window (if hidden).
  // It then calls UpdateWindowEdges to make sure we don't overwrite the Chrome
  // window border. If |no_redraw| is set, the window is getting moved but not
  // sized, and should not be redrawn to reduce update flicker.
  virtual void SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw) = 0;

  virtual bool IsFindBarVisible() = 0;

  // Upon dismissing the window, restore focus to the last focused view which is
  // not FindBarView or any of its children.
  virtual void RestoreSavedFocus() = 0;
};

#endif  // CHROME_BROWSER_FIND_BAR_H_
