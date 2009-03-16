// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_FIND_BAR_GTK_H_
#define CHROME_BROWSER_GTK_FIND_BAR_GTK_H_

#include <gtk/gtk.h>

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/find_bar.h"
#include "chrome/common/owned_widget_gtk.h"

class FindBarController;
class TabContentsContainerGtk;
class WebContents;

// Currently this class contains both a model and a view.  We may want to
// eventually pull out the model specific bits and share with Windows.
class FindBarGtk : public FindBar {
 public:
  FindBarGtk();
  virtual ~FindBarGtk();

  void set_find_bar_controller(FindBarController* find_bar_controller) {
    find_bar_controller_ = find_bar_controller;
  }

  // Callback when the text in the find box changes.
  void ContentsChanged();

  // Callback when Escape is pressed.
  void EscapePressed();

  GtkWidget* widget() const { return container_.get(); }

  // Methods from FindBar.
  virtual void Show();
  virtual void Hide(bool animate);
  virtual void SetFocusAndSelection();
  virtual void ClearResults(const FindNotificationDetails& results);
  virtual void StopAnimation();
  virtual void SetFindText(const std::wstring& find_text);
  virtual void UpdateUIForFindResult(const FindNotificationDetails& result,
                                     const std::wstring& find_text);
  virtual gfx::Rect GetDialogPosition(gfx::Rect avoid_overlapping_rect);
  virtual void SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw);
  virtual bool IsFindBarVisible();
  virtual void RestoreSavedFocus();

 private:
  // GtkHBox containing the find bar widgets.
  OwnedWidgetGtk container_;

  // The widget where text is entered.
  GtkWidget* find_text_;

  // Pointer back to the owning controller.
  FindBarController* find_bar_controller_;

  DISALLOW_COPY_AND_ASSIGN(FindBarGtk);
};

#endif  // CHROME_BROWSER_GTK_FIND_BAR_GTK_H_
