// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/button_dropdown.h"

#include "base/message_loop.h"
#include "chrome/browser/back_forward_menu_model.h"
#include "chrome/common/l10n_util.h"
#include "chrome/views/view_menu_delegate.h"
#include "chrome/views/widget/widget.h"
#include "grit/generated_resources.h"

namespace views {

// How long to wait before showing the menu
static const int kMenuTimerDelay = 500;

////////////////////////////////////////////////////////////////////////////////
//
// ButtonDropDown - constructors, destructors, initialization, cleanup
//
////////////////////////////////////////////////////////////////////////////////

ButtonDropDown::ButtonDropDown(ButtonListener* listener,
                               Menu::Delegate* menu_delegate)
    : ImageButton(listener),
      menu_delegate_(menu_delegate),
      y_position_on_lbuttondown_(0),
      show_menu_factory_(this) {
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
    // SM_CYDRAG is a pixel value for minimum dragging distance before operation
    // counts as a drag, and not just as a click and accidental move of a mouse.
    // See http://msdn2.microsoft.com/en-us/library/ms724385.aspx for details.
    int dragging_threshold = GetSystemMetrics(SM_CYDRAG);

    // If the mouse is dragged to a y position lower than where it was when
    // clicked then we should not wait for the menu to appear but show
    // it immediately.
    if (e.y() > y_position_on_lbuttondown_ + dragging_threshold) {
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

void ButtonDropDown::ShowDropDownMenu(HWND window) {
  if (menu_delegate_) {
    gfx::Rect lb = GetLocalBounds(true);

    // Both the menu position and the menu anchor type change if the UI layout
    // is right-to-left.
    gfx::Point menu_position(lb.origin());
    menu_position.Offset(0, lb.height() - 1);
    if (UILayoutIsRightToLeft())
      menu_position.Offset(lb.width() - 1, 0);

    Menu::AnchorPoint anchor = Menu::TOPLEFT;
    if (UILayoutIsRightToLeft())
      anchor = Menu::TOPRIGHT;

    View::ConvertPointToScreen(this, &menu_position);

    int left_bound = GetSystemMetrics(SM_XVIRTUALSCREEN);
    if (menu_position.x() < left_bound)
      menu_position.set_x(left_bound);

    Menu menu(menu_delegate_, anchor, window);

    // ID's for AppendMenu is 1-based because RunMenu will ignore the user
    // selection if id=0 is selected (0 = NO-OP) so we add 1 here and subtract 1
    // in the handlers above to get the actual index
    int item_count = menu_delegate_->GetItemCount();
    for (int i = 0; i < item_count; i++) {
      if (menu_delegate_->IsItemSeparator(i + 1)) {
        menu.AppendSeparator();
      } else {
        if (menu_delegate_->HasIcon(i + 1))
          menu.AppendMenuItemWithIcon(i + 1, L"", SkBitmap());
        else
          menu.AppendMenuItem(i+1, L"", Menu::NORMAL);
      }
    }

    menu.RunMenuAt(menu_position.x(), menu_position.y());

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

  action->assign(l10n_util::GetString(IDS_ACCACTION_PRESS));
  return true;
}

bool ButtonDropDown::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_BUTTONDROPDOWN;
  return true;
}

bool ButtonDropDown::GetAccessibleState(VARIANT* state) {
  DCHECK(state);

  state->lVal |= STATE_SYSTEM_HASPOPUP;
  return true;
}

}  // namespace views
