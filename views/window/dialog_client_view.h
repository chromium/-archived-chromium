// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_
#define VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_

#include "app/gfx/font.h"
#include "views/focus/focus_manager.h"
#include "views/controls/button/button.h"
#include "views/window/client_view.h"

namespace views {

class DialogDelegate;
class NativeButton;
class Window;

///////////////////////////////////////////////////////////////////////////////
// DialogClientView
//
//  This ClientView subclass provides the content of a typical dialog box,
//  including a strip of buttons at the bottom right of the window, default
//  accelerator handlers for accept and cancel, and the ability for the
//  embedded contents view to provide extra UI to be shown in the row of
//  buttons.
//
class DialogClientView : public ClientView,
                         public ButtonListener,
                         public FocusChangeListener {
 public:
  DialogClientView(Window* window, View* contents_view);
  virtual ~DialogClientView();

  // Adds the dialog buttons required by the supplied WindowDelegate to the
  // view.
  void ShowDialogButtons();

  // Updates the enabled state and label of the buttons required by the
  // supplied WindowDelegate
  void UpdateDialogButtons();

  // Accept the changes made in the window that contains this ClientView.
  void AcceptWindow();

  // Cancel the changes made in the window that contains this ClientView.
  void CancelWindow();

  // Accessors in case the user wishes to adjust these buttons.
  NativeButton* ok_button() const { return ok_button_; }
  NativeButton* cancel_button() const { return cancel_button_; }

  // Overridden from ClientView:
  virtual bool CanClose() const;
  virtual void WindowClosing();
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual DialogClientView* AsDialogClientView() { return this; }

  // FocusChangeListener implementation:
  virtual void FocusWillChange(View* focused_before, View* focused_now);

 protected:
  // View overrides:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void PaintChildren(gfx::Canvas* canvas);
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual gfx::Size GetPreferredSize();
  virtual bool AcceleratorPressed(const Accelerator& accelerator);

  // ButtonListener implementation:
  virtual void ButtonPressed(Button* sender);

 private:
  // Paint the size box in the bottom right corner of the window if it is
  // resizable.
  void PaintSizeBox(gfx::Canvas* canvas);

  // Returns the width of the specified dialog button using the correct font.
  int GetButtonWidth(int button) const;
  int GetButtonsHeight() const;

  // Position and size various sub-views.
  void LayoutDialogButtons();
  void LayoutContentsView();

  // Makes the specified button the default button.
  void SetDefaultButton(NativeButton* button);

  bool has_dialog_buttons() const { return ok_button_ || cancel_button_; }

  // Create and add the extra view, if supplied by the delegate.
  void CreateExtraView();

  // Returns the DialogDelegate for the window.
  DialogDelegate* GetDialogDelegate() const;

  // Closes the window.
  void Close();

  // The dialog buttons.
  NativeButton* ok_button_;
  NativeButton* cancel_button_;

  // The button that is currently the default button if any.
  NativeButton* default_button_;

  // The button-level extra view, NULL unless the dialog delegate supplies one.
  View* extra_view_;

  // The layout rect of the size box, when visible.
  gfx::Rect size_box_bounds_;

  // True if the window was Accepted by the user using the OK button.
  bool accepted_;

  // Static resource initialization
  static void InitClass();
  static gfx::Font* dialog_button_font_;

  DISALLOW_COPY_AND_ASSIGN(DialogClientView);
};

}  // namespace views

#endif  // #ifndef VIEWS_WINDOW_DIALOG_CLIENT_VIEW_H_
