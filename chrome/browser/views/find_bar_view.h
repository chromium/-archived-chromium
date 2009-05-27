// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FIND_BAR_VIEW_H_
#define CHROME_BROWSER_VIEWS_FIND_BAR_VIEW_H_

#include "base/gfx/size.h"
#include "base/string16.h"
#include "chrome/browser/find_notification_details.h"
#include "views/controls/button/button.h"
#include "views/controls/textfield/textfield.h"

class FindBarWin;

namespace views {
class ImageButton;
class Label;
class MouseEvent;
class View;
}

////////////////////////////////////////////////////////////////////////////////
//
// The FindInPageView is responsible for drawing the UI controls of the
// FindInPage window, the find text box, the 'Find' button and the 'Close'
// button. It communicates the user search words to the FindBarWin.
//
////////////////////////////////////////////////////////////////////////////////
class FindBarView : public views::View,
                    public views::ButtonListener,
                    public views::Textfield::Controller {
 public:
  // A tag denoting which button the user pressed.
  enum ButtonTag {
    FIND_PREVIOUS_TAG = 0,  // The Find Previous button.
    FIND_NEXT_TAG,          // The Find Next button.
    CLOSE_TAG,              // The Close button (the 'X').
  };

  explicit FindBarView(FindBarWin* container);
  virtual ~FindBarView();

  // Sets the text displayed in the text box.
  void SetFindText(const string16& find_text);

  // Updates the label inside the Find text box that shows the ordinal of the
  // active item and how many matches were found.
  void UpdateForResult(const FindNotificationDetails& result,
                       const string16& find_text);

  // Claims focus for the text field and selects its contents.
  void SetFocusAndSelection();

  // Updates the view to let it know where the controller is clipping the
  // Find window (while animating the opening or closing of the window).
  void animation_offset(int offset) { animation_offset_ = offset; }

  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender);

  // Overridden from views::Textfield::Controller:
  virtual void ContentsChanged(views::Textfield* sender,
                               const std::wstring& new_contents);
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& key);

 private:
  // Resets the background for the match count label.
  void ResetMatchCountBackground();

  // We use a hidden view to grab mouse clicks and bring focus to the find
  // text box. This is because although the find text box may look like it
  // extends all the way to the find button, it only goes as far as to the
  // match_count label. The user, however, expects being able to click anywhere
  // inside what looks like the find text box (including on or around the
  // match_count label) and have focus brought to the find box.
  class FocusForwarderView : public views::View {
   public:
    explicit FocusForwarderView(
        views::Textfield* view_to_focus_on_mousedown)
      : view_to_focus_on_mousedown_(view_to_focus_on_mousedown) {}

   private:
    virtual bool OnMousePressed(const views::MouseEvent& event);

    views::Textfield* view_to_focus_on_mousedown_;

    DISALLOW_COPY_AND_ASSIGN(FocusForwarderView);
  };

  // Manages the OS-specific view for the find bar and acts as an intermediary
  // between us and the TabContentsView.
  FindBarWin* container_;

  // The controls in the window.
  views::Textfield* find_text_;
  views::Label* match_count_text_;
  FocusForwarderView* focus_forwarder_view_;
  views::ImageButton* find_previous_button_;
  views::ImageButton* find_next_button_;
  views::ImageButton* close_button_;

  // While animating, the controller clips the window and draws only the bottom
  // part of it. The view needs to know the pixel offset at which we are drawing
  // the window so that we can draw the curved edges that attach to the toolbar
  // in the right location.
  int animation_offset_;

  DISALLOW_COPY_AND_ASSIGN(FindBarView);
};

#endif  // CHROME_BROWSER_VIEWS_FIND_BAR_VIEW_H_
