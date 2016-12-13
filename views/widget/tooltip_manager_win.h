// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_TOOLTIP_MANAGER_WIN_H_
#define VIEWS_WIDGET_TOOLTIP_MANAGER_WIN_H_

#include <windows.h>
#include <commctrl.h>
#include <string>

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/task.h"
#include "views/widget/tooltip_manager.h"

namespace gfx {
class Font;
}

namespace views {

class View;
class Widget;

// TooltipManager implementation for Windows.
//
// This class is intended to be used by WidgetWin. To use this, you must
// do the following:
// Add the following to your MSG_MAP:
//
//   MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
//   MESSAGE_RANGE_HANDLER(WM_NCMOUSEMOVE, WM_NCMOUSEMOVE, OnMouseRange)
//   MSG_WM_NOTIFY(OnNotify)
//
// With the following implementations:
//   LRESULT XXX::OnMouseRange(UINT u_msg, WPARAM w_param, LPARAM l_param,
//                             BOOL& handled) {
//     tooltip_manager_->OnMouse(u_msg, w_param, l_param);
//     handled = FALSE;
//     return 0;
//   }
//
//   LRESULT XXX::OnNotify(int w_param, NMHDR* l_param) {
//     bool handled;
//     LRESULT result = tooltip_manager_->OnNotify(w_param, l_param, &handled);
//     SetMsgHandled(handled);
//     return result;
//   }
//
// And of course you'll need to create the TooltipManager!
//
// Lastly, you'll need to override GetTooltipManager.
//
// See WidgetWin for an example of this in action.
class TooltipManagerWin : public TooltipManager {
 public:
  // Creates a TooltipManager for the specified Widget and parent window.
  explicit TooltipManagerWin(Widget* widget);
  virtual ~TooltipManagerWin();

  // Notification that the view hierarchy has changed in some way.
  virtual void UpdateTooltip();

  // Invoked when the tooltip text changes for the specified views.
  virtual void TooltipTextChanged(View* view);

  // Invoked when toolbar icon gets focus.
  virtual void ShowKeyboardTooltip(View* view);

  // Invoked when toolbar loses focus.
  virtual void HideKeyboardTooltip();

  // Message handlers. These forward to the tooltip control.
  virtual void OnMouse(UINT u_msg, WPARAM w_param, LPARAM l_param);
  LRESULT OnNotify(int w_param, NMHDR* l_param, bool* handled);
  // Not used directly by TooltipManager, but provided for AeroTooltipManager.
  virtual void OnMouseLeave() {}

 protected:
  virtual void Init();

  // Returns the Widget we're showing tooltips for.
  gfx::NativeView GetParent();

  // Updates the tooltip for the specified location.
  void UpdateTooltip(int x, int y);

  // Tooltip control window.
  HWND tooltip_hwnd_;

  // Tooltip information.
  TOOLINFO toolinfo_;

  // Last location of the mouse. This is in the coordinates of the rootview.
  int last_mouse_x_;
  int last_mouse_y_;

  // Whether or not the tooltip is showing.
  bool tooltip_showing_;

 private:
  // Sets the tooltip position based on the x/y position of the text. If the
  // tooltip fits, true is returned.
  bool SetTooltipPosition(int text_x, int text_y);

  // Calculates the preferred height for tooltips. This always returns a
  // positive value.
  int CalcTooltipHeight();

  // Trims the tooltip to fit, setting text to the clipped result, width to the
  // width (in pixels) of the clipped text and line_count to the number of lines
  // of text in the tooltip.
  void TrimTooltipToFit(std::wstring* text,
                        int* width,
                        int* line_count,
                        int position_x,
                        int position_y,
                        HWND window);

  // Invoked when the timer elapses and tooltip has to be destroyed.
  void DestroyKeyboardTooltipWindow(HWND window_to_destroy);

  // Hosting Widget.
  Widget* widget_;

  // The View the mouse is under. This is null if the mouse isn't under a
  // View.
  View* last_tooltip_view_;

  // Whether or not the view under the mouse needs to be refreshed. If this
  // is true, when the tooltip is asked for the view under the mouse is
  // refreshed.
  bool last_view_out_of_sync_;

  // Text for tooltip from the view.
  std::wstring tooltip_text_;

  // The clipped tooltip.
  std::wstring clipped_text_;

  // Number of lines in the tooltip.
  int line_count_;

  // Width of the last tooltip.
  int tooltip_width_;

  // control window for tooltip displayed using keyboard.
  HWND keyboard_tooltip_hwnd_;

  // Used to register DestroyTooltipWindow function with PostDelayedTask
  // function.
  ScopedRunnableMethodFactory<TooltipManagerWin> keyboard_tooltip_factory_;

  DISALLOW_COPY_AND_ASSIGN(TooltipManagerWin);
};

}  // namespace views

#endif // VIEWS_WIDGET_TOOLTIP_MANAGER_WIN_H_
