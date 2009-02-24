// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <atlbase.h>
#include <atlapp.h>  // for GET_X/Y_LPARAM
#include <commctrl.h>
#include <shlobj.h>

#include "chrome/views/aero_tooltip_manager.h"

#include "base/message_loop.h"
#include "chrome/common/l10n_util_win.h"

namespace views {

///////////////////////////////////////////////////////////////////////////////
// AeroTooltipManager, public:

AeroTooltipManager::AeroTooltipManager(Widget* widget, HWND parent)
    : TooltipManager(widget, parent),
      initial_delay_(0) {
}

AeroTooltipManager::~AeroTooltipManager() {
  if (initial_timer_)
    initial_timer_->Disown();
}

void AeroTooltipManager::OnMouse(UINT u_msg, WPARAM w_param, LPARAM l_param) {
  if (initial_timer_)
    initial_timer_->Disown();

  if (u_msg == WM_MOUSEMOVE || u_msg == WM_NCMOUSEMOVE) {
    int x = GET_X_LPARAM(l_param);
    int y = GET_Y_LPARAM(l_param);
    if (last_mouse_x_ != x || last_mouse_y_ != y) {
      last_mouse_x_ = x;
      last_mouse_y_ = y;
      HideKeyboardTooltip();
      UpdateTooltip(x, y);
    }

    // Delay opening of the tooltip just in case the user moves their
    // mouse to another control. We defer this from Init because we get
    // zero if we query it too soon.
    if (!initial_delay_) {
      initial_delay_ = static_cast<int>(
          ::SendMessage(tooltip_hwnd_, TTM_GETDELAYTIME, TTDT_INITIAL, 0));
    }
    initial_timer_ = new InitialTimer(this, initial_delay_);
  } else {
    // Hide the tooltip and cancel any timers.
    ::SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
    ::SendMessage(tooltip_hwnd_, TTM_TRACKACTIVATE, false, (LPARAM)&toolinfo_);
    return;
  }
}

void AeroTooltipManager::OnMouseLeave() {
  last_mouse_x_ = last_mouse_y_ = -1;
  UpdateTooltip();
}

///////////////////////////////////////////////////////////////////////////////
// AeroTooltipManager, private:

void AeroTooltipManager::Init() {
  // Create the tooltip control.
  tooltip_hwnd_ = CreateWindowEx(
      WS_EX_TRANSPARENT | l10n_util::GetExtendedTooltipStyles(),
      TOOLTIPS_CLASS, NULL, TTS_NOPREFIX, 0, 0, 0, 0,
      parent_, NULL, NULL, NULL);
  // Add one tool that is used for all tooltips.
  toolinfo_.cbSize = sizeof(toolinfo_);

  // We use tracking tooltips on Vista to allow us to manually control the
  // visibility of the tooltip.
  toolinfo_.uFlags = TTF_TRANSPARENT | TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE;
  toolinfo_.hwnd = parent_;
  toolinfo_.uId = (UINT_PTR)parent_;

  // Setting this tells windows to call parent_ back (using a WM_NOTIFY
  // message) for the actual tooltip contents.
  toolinfo_.lpszText = LPSTR_TEXTCALLBACK;
  SetRectEmpty(&toolinfo_.rect);
  ::SendMessage(tooltip_hwnd_, TTM_ADDTOOL, 0, (LPARAM)&toolinfo_);
}

void AeroTooltipManager::OnTimer() {
  initial_timer_ = NULL;

  POINT pt;
  pt.x = last_mouse_x_;
  pt.y = last_mouse_y_;
  ::ClientToScreen(parent_, &pt);

  // Set the position and visibility.
  if (!tooltip_showing_) {
    ::SendMessage(tooltip_hwnd_, TTM_POPUP, 0, 0);
    ::SendMessage(tooltip_hwnd_, TTM_TRACKPOSITION, 0, MAKELPARAM(pt.x, pt.y));
    ::SendMessage(tooltip_hwnd_, TTM_TRACKACTIVATE, true, (LPARAM)&toolinfo_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// AeroTooltipManager::InitialTimer

AeroTooltipManager::InitialTimer::InitialTimer(AeroTooltipManager* manager,
                                               int time) : manager_(manager) {
  MessageLoop::current()->PostDelayedTask(FROM_HERE, NewRunnableMethod(
      this, &InitialTimer::Execute), time);
}

void AeroTooltipManager::InitialTimer::Disown() {
  manager_ = NULL;
}

void AeroTooltipManager::InitialTimer::Execute() {
  if (manager_)
    manager_->OnTimer();
}

}  // namespace views

