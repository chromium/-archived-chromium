// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlapp.h>

#include "chrome/views/menu_button.h"

#include "chrome/common/drag_drop_types.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/button.h"
#include "chrome/views/event.h"
#include "chrome/views/view_menu_delegate.h"
#include "chrome/views/widget/root_view.h"
#include "chrome/views/widget/widget.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

using base::Time;
using base::TimeDelta;

namespace views {

// The amount of time, in milliseconds, we wait before allowing another mouse
// pressed event to show the menu.
static const int64 kMinimumTimeBetweenButtonClicks = 100;

// The down arrow used to differentiate the menu button from normal
// text buttons.
static const SkBitmap* kMenuMarker = NULL;

// How much padding to put on the left and right of the menu marker.
static const int kMenuMarkerPaddingLeft = 3;
static const int kMenuMarkerPaddingRight = -1;

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - constructors, destructors, initialization
//
////////////////////////////////////////////////////////////////////////////////

MenuButton::MenuButton(ButtonListener* listener,
                       const std::wstring& text,
                       ViewMenuDelegate* menu_delegate,
                       bool show_menu_marker)
    : TextButton(listener, text),
      menu_visible_(false),
      menu_closed_time_(),
      menu_delegate_(menu_delegate),
      show_menu_marker_(show_menu_marker) {
  if (kMenuMarker == NULL) {
    kMenuMarker = ResourceBundle::GetSharedInstance()
        .GetBitmapNamed(IDR_MENU_DROPARROW);
  }
  set_alignment(TextButton::ALIGN_LEFT);
}

MenuButton::~MenuButton() {
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - Public APIs
//
////////////////////////////////////////////////////////////////////////////////

gfx::Size MenuButton::GetPreferredSize() {
  gfx::Size prefsize = TextButton::GetPreferredSize();
  if (show_menu_marker_) {
    prefsize.Enlarge(kMenuMarker->width() + kMenuMarkerPaddingLeft +
                         kMenuMarkerPaddingRight,
                     0);
  }
  return prefsize;
}

void MenuButton::Paint(ChromeCanvas* canvas, bool for_drag) {
  TextButton::Paint(canvas, for_drag);

  if (show_menu_marker_) {
    gfx::Insets insets = GetInsets();

    // We can not use the views' mirroring infrastructure for mirroring a
    // MenuButton control (see TextButton::Paint() for a detailed explanation
    // regarding why we can not flip the canvas). Therefore, we need to
    // manually mirror the position of the down arrow.
    gfx::Rect arrow_bounds(width() - insets.right() -
                           kMenuMarker->width() - kMenuMarkerPaddingRight,
                           height() / 2 - kMenuMarker->height() / 2,
                           kMenuMarker->width(),
                           kMenuMarker->height());
    arrow_bounds.set_x(MirroredLeftPointForRect(arrow_bounds));
    canvas->DrawBitmapInt(*kMenuMarker, arrow_bounds.x(), arrow_bounds.y());
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - Events
//
////////////////////////////////////////////////////////////////////////////////

int MenuButton::GetMaximumScreenXCoordinate() {
  Widget* widget = GetWidget();

  if (!widget) {
    NOTREACHED();
    return 0;
  }

  HWND hwnd = widget->GetNativeView();
  CRect t;
  ::GetWindowRect(hwnd, &t);

  gfx::Rect r(t);
  gfx::Rect monitor_rect = win_util::GetMonitorBoundsForRect(r);
  return monitor_rect.x() + monitor_rect.width() - 1;
}

bool MenuButton::Activate() {
  SetState(BS_PUSHED);
  // We need to synchronously paint here because subsequently we enter a
  // menu modal loop which will stop this window from updating and
  // receiving the paint message that should be spawned by SetState until
  // after the menu closes.
  PaintNow();
  if (menu_delegate_) {
    gfx::Rect lb = GetLocalBounds(true);

    // The position of the menu depends on whether or not the locale is
    // right-to-left.
    gfx::Point menu_position(lb.right(), lb.bottom());
    if (UILayoutIsRightToLeft())
      menu_position.set_x(lb.x());

    View::ConvertPointToScreen(this, &menu_position);
    if (UILayoutIsRightToLeft())
      menu_position.Offset(2, -4);
    else
      menu_position.Offset(-2, -4);

    int max_x_coordinate = GetMaximumScreenXCoordinate();
    if (max_x_coordinate && max_x_coordinate <= menu_position.x())
      menu_position.set_x(max_x_coordinate - 1);

    // We're about to show the menu from a mouse press. By showing from the
    // mouse press event we block RootView in mouse dispatching. This also
    // appears to cause RootView to get a mouse pressed BEFORE the mouse
    // release is seen, which means RootView sends us another mouse press no
    // matter where the user pressed. To force RootView to recalculate the
    // mouse target during the mouse press we explicitly set the mouse handler
    // to NULL.
    GetRootView()->SetMouseHandler(NULL);

    menu_visible_ = true;
    menu_delegate_->RunMenu(this, menu_position.ToPOINT(),
                            GetWidget()->GetNativeView());
    menu_visible_ = false;
    menu_closed_time_ = Time::Now();

    // Now that the menu has closed, we need to manually reset state to
    // "normal" since the menu modal loop will have prevented normal
    // mouse move messages from getting to this View. We set "normal"
    // and not "hot" because the likelihood is that the mouse is now
    // somewhere else (user clicked elsewhere on screen to close the menu
    // or selected an item) and we will inevitably refresh the hot state
    // in the event the mouse _is_ over the view.
    SetState(BS_NORMAL);

    // We must return false here so that the RootView does not get stuck
    // sending all mouse pressed events to us instead of the appropriate
    // target.
    return false;
  }
  return true;
}

bool MenuButton::OnMousePressed(const MouseEvent& e) {
  RequestFocus();
  if (state() != BS_DISABLED) {
    // If we're draggable (GetDragOperations returns a non-zero value), then
    // don't pop on press, instead wait for release.
    if (e.IsOnlyLeftMouseButton() && HitTest(e.location()) &&
        GetDragOperations(e.x(), e.y()) == DragDropTypes::DRAG_NONE) {
      TimeDelta delta = Time::Now() - menu_closed_time_;
      int64 delta_in_milliseconds = delta.InMilliseconds();
      if (delta_in_milliseconds > kMinimumTimeBetweenButtonClicks) {
        return Activate();
      }
    }
  }
  return true;
}

void MenuButton::OnMouseReleased(const MouseEvent& e,
                                 bool canceled) {
  if (GetDragOperations(e.x(), e.y()) != DragDropTypes::DRAG_NONE &&
      state() != BS_DISABLED && !canceled && !InDrag() &&
      e.IsOnlyLeftMouseButton() && HitTest(e.location())) {
    Activate();
  } else {
    TextButton::OnMouseReleased(e, canceled);
  }
}

// When the space bar or the enter key is pressed we need to show the menu.
bool MenuButton::OnKeyReleased(const KeyEvent& e) {
  if ((e.GetCharacter() == VK_SPACE) || (e.GetCharacter() == VK_RETURN)) {
    return Activate();
  }
  return true;
}

// The reason we override View::OnMouseExited is because we get this event when
// we display the menu. If we don't override this method then
// BaseButton::OnMouseExited will get the event and will set the button's state
// to BS_NORMAL instead of keeping the state BM_PUSHED. This, in turn, will
// cause the button to appear depressed while the menu is displayed.
void MenuButton::OnMouseExited(const MouseEvent& event) {
  if ((state_ != BS_DISABLED) && (!menu_visible_) && (!InDrag())) {
    SetState(BS_NORMAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// MenuButton - accessibility
//
////////////////////////////////////////////////////////////////////////////////

bool MenuButton::GetAccessibleDefaultAction(std::wstring* action) {
  DCHECK(action);

  action->assign(l10n_util::GetString(IDS_ACCACTION_PRESS));
  return true;
}

bool MenuButton::GetAccessibleRole(VARIANT* role) {
  DCHECK(role);

  role->vt = VT_I4;
  role->lVal = ROLE_SYSTEM_BUTTONDROPDOWN;
  return true;
}

bool MenuButton::GetAccessibleState(VARIANT* state) {
  DCHECK(state);

  state->lVal |= STATE_SYSTEM_HASPOPUP;
  return true;
}

}  // namespace views
