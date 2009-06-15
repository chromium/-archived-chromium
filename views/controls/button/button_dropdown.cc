// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/button/button_dropdown.h"

#include "app/l10n_util.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "grit/app_strings.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/widget/widget.h"

namespace views {

// How long to wait before showing the menu
static const int kMenuTimerDelay = 500;

////////////////////////////////////////////////////////////////////////////////
//
// ButtonDropDown - constructors, destructors, initialization, cleanup
//
////////////////////////////////////////////////////////////////////////////////

ButtonDropDown::ButtonDropDown(ButtonListener* listener,
                               Menu2Model* model)
    : ImageButton(listener),
      model_(model),
      y_position_on_lbuttondown_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(show_menu_factory_(this)) {
}

ButtonDropDown::~ButtonDropDown() {
}

////////////////////////////////////////////////////////////////////////////////
//
// ButtonDropDown - Events
//
////////////////////////////////////////////////////////////////////////////////

bool ButtonDropDown::OnMousePressed(const MouseEvent& e) {
  if (IsEnabled() && e.IsLeftMouseButton() && HitTest(e.location())) {
    // Store the y pos of the mouse coordinates so we can use them later to
    // determine if the user dragged the mouse down (which should pop up the
    // drag down menu immediately, instead of waiting for the timer)
    y_position_on_lbuttondown_ = e.y();

    // Schedule a task that will show the menu.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        show_menu_factory_.NewRunnableMethod(&ButtonDropDown::ShowDropDownMenu,
                                             GetWidget()->GetNativeView()),
        kMenuTimerDelay);
  }

  return ImageButton::OnMousePressed(e);
}

void ButtonDropDown::OnMouseReleased(const MouseEvent& e, bool canceled) {
  ImageButton::OnMouseReleased(e, canceled);

  if (canceled)
    return;

  if (e.IsLeftMouseButton())
    show_menu_factory_.RevokeAll();

  if (IsEnabled() && e.IsRightMouseButton() && HitTest(e.location())) {
    show_menu_factory_.RevokeAll();
    // Make the button look depressed while the menu is open.
    // NOTE: SetState() schedules a paint, but it won't occur until after the
    //       context menu message loop has terminated, so we PaintNow() to
    //       update the appearance synchronously.
    SetState(BS_PUSHED);
    PaintNow();
    ShowDropDownMenu(GetWidget()->GetNativeView());
  }
}

bool ButtonDropDown::OnMouseDragged(const MouseEvent& e) {
  bool result = ImageButton::OnMouseDragged(e);

  if (!show_menu_factory_.empty()) {
    // If the mouse is dragged to a y position lower than where it was when
    // clicked then we should not wait for the menu to appear but show
    // it immediately.
    if (e.y() > y_position_on_lbuttondown_ + GetHorizontalDragThreshold()) {
      show_menu_factory_.RevokeAll();
      ShowDropDownMenu(GetWidget()->GetNativeView());
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
//
// ButtonDropDown - Menu functions
//
////////////////////////////////////////////////////////////////////////////////

void ButtonDropDown::ShowContextMenu(int x, int y, bool is_mouse_gesture) {
  show_menu_factory_.RevokeAll();
  // Make the button look depressed while the menu is open.
  // NOTE: SetState() schedules a paint, but it won't occur until after the
  //       context menu message loop has terminated, so we PaintNow() to
  //       update the appearance synchronously.
  SetState(BS_PUSHED);
  PaintNow();
  ShowDropDownMenu(GetWidget()->GetNativeView());
  SetState(BS_HOT);
}

void ButtonDropDown::ShowDropDownMenu(gfx::NativeView window) {
  if (model_) {
    gfx::Rect lb = GetLocalBounds(true);

    // Both the menu position and the menu anchor type change if the UI layout
    // is right-to-left.
    gfx::Point menu_position(lb.origin());
    menu_position.Offset(0, lb.height() - 1);
    if (UILayoutIsRightToLeft())
      menu_position.Offset(lb.width() - 1, 0);

    View::ConvertPointToScreen(this, &menu_position);

#if defined(OS_WIN)
    int left_bound = GetSystemMetrics(SM_XVIRTUALSCREEN);
#else
    int left_bound = 0;
    NOTIMPLEMENTED();
#endif
    if (menu_position.x() < left_bound)
      menu_position.set_x(left_bound);

    menu_.reset(new Menu2(model_));
    Menu2::Alignment align = Menu2::ALIGN_TOPLEFT;
    if (UILayoutIsRightToLeft())
      align = Menu2::ALIGN_TOPLEFT;
    menu_->RunMenuAt(menu_position, align);

    // Need to explicitly clear mouse handler so that events get sent
    // properly after the menu finishes running. If we don't do this, then
    // the first click to other parts of the UI is eaten.
    SetMouseHandler(NULL);
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// ButtonDropDown - Accessibility
//
////////////////////////////////////////////////////////////////////////////////

bool ButtonDropDown::GetAccessibleDefaultAction(std::wstring* action) {
  DCHECK(action);

  action->assign(l10n_util::GetString(IDS_APP_ACCACTION_PRESS));
  return true;
}

bool ButtonDropDown::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_BUTTONDROPDOWN;
  return true;
}

bool ButtonDropDown::GetAccessibleState(AccessibilityTypes::State* state) {
  DCHECK(state);

  *state = AccessibilityTypes::STATE_HASPOPUP;
  return true;
}

}  // namespace views
