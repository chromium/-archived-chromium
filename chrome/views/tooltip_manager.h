// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_VIEWS_TOOLTIP_MANAGER_H__
#define CHROME_VIEWS_TOOLTIP_MANAGER_H__

#include <windows.h>
#include <string>
#include "base/basictypes.h"

class ChromeFont;

namespace ChromeViews {

class View;
class ViewContainer;

// TooltipManager takes care of the wiring to support tooltips for Views.
// This class is intended to be used by ViewContainers. To use this, you must
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
// See XPFrame for an example of this in action.
class TooltipManager {
 public:
  // Returns the height of tooltips. This should only be invoked from within
  // GetTooltipTextOrigin.
  static int GetTooltipHeight();

  // Returns the default font used by tooltips.
  static ChromeFont GetDefaultFont();

  // Returns the separator for lines of text in a tooltip.
  static const std::wstring& GetLineSeparator();

  // Creates a TooltipManager for the specified ViewContainer and parent window.
  TooltipManager(ViewContainer* container, HWND parent);
  virtual ~TooltipManager();

  // Notification that the view hierarchy has changed in some way.
  void UpdateTooltip();

  // Invoked when the tooltip text changes for the specified views.
  void TooltipTextChanged(View* view);

  // Invoked when toolbar icon gets focus.
  void ShowKeyboardTooltip(View* view);

  // Invoked when toolbar loses focus.
  void HideKeyboardTooltip();

  // Message handlers. These forward to the tooltip control.
  virtual void OnMouse(UINT u_msg, WPARAM w_param, LPARAM l_param);
  LRESULT OnNotify(int w_param, NMHDR* l_param, bool* handled);

 protected:
  virtual void Init();

  // Updates the tooltip for the specified location.
  void UpdateTooltip(int x, int y);

  // Parent window the tooltip is added to.
  HWND parent_;

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

  // Hosting view container.
  ViewContainer* view_container_;

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

  // Height for a tooltip; lazily calculated.
  static int tooltip_height_;

  // control window for tooltip displayed using keyboard.
  HWND keyboard_tooltip_hwnd_;

  // Used to register DestroyTooltipWindow function with postdelayedtask
  // function.
  ScopedRunnableMethodFactory<TooltipManager> keyboard_tooltip_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(TooltipManager);
};

} // namespace ChromeViews

#endif // CHROME_VIEWS_TOOLTIP_MANAGER_H__
