// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "chrome/common/gfx/chrome_font.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/l10n_util_win.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/win_util.h"
#include "chrome/views/root_view.h"
#include "chrome/views/tooltip_manager.h"
#include "chrome/views/view.h"
#include "chrome/views/widget.h"

namespace views {

//static
int TooltipManager::tooltip_height_ = 0;

// Default timeout for the tooltip displayed using keyboard.
// Timeout is mentioned in milliseconds.
static const int kDefaultTimeout = 4000;

// Maximum number of lines we allow in the tooltip.
static const int kMaxLines = 6;

// Maximum number of characters we allow in a tooltip.
static const int kMaxTooltipLength = 1024;

// Breaks |text| along line boundaries, placing each line of text into lines.
static void SplitTooltipString(const std::wstring& text,
                               std::vector<std::wstring>* lines) {
  size_t index = 0;
  size_t next_index;
  while ((next_index = text.find(TooltipManager::GetLineSeparator(), index))
         != std::wstring::npos && lines->size() < kMaxLines) {
    lines->push_back(text.substr(index, next_index - index));
    index = next_index + TooltipManager::GetLineSeparator().size();
  }
  if (next_index != text.size() && lines->size() < kMaxLines)
    lines->push_back(text.substr(index, text.size() - index));
}

// static
int TooltipManager::GetTooltipHeight() {
  DCHECK(tooltip_height_ > 0);
  return tooltip_height_;
}

static ChromeFont DetermineDefaultFont() {
  HWND window = CreateWindowEx(
      WS_EX_TRANSPARENT | l10n_util::GetExtendedTooltipStyles(),
      TOOLTIPS_CLASS, NULL, 0 , 0, 0, 0, 0, NULL, NULL, NULL, NULL);
  HFONT hfont = reinterpret_cast<HFONT>(SendMessage(window, WM_GETFONT, 0, 0));
  ChromeFont font = hfont ? ChromeFont::CreateFont(hfont) : ChromeFont();
  DestroyWindow(window);
  return font;
}

// static
ChromeFont TooltipManager::GetDefaultFont() {
  static ChromeFont* font = NULL;
  if (!font)
    font = new ChromeFont(DetermineDefaultFont());
  return *font;
}

// static
const std::wstring& TooltipManager::GetLineSeparator() {
  static const std::wstring* separator = NULL;
  if (!separator)
    separator = new std::wstring(L"\r\n");
  return *separator;
}

TooltipManager::TooltipManager(Widget* widget, HWND parent)
    : widget_(widget),
      parent_(parent),
      last_mouse_x_(-1),
      last_mouse_y_(-1),
      tooltip_showing_(false),
      last_tooltip_view_(NULL),
      last_view_out_of_sync_(false),
      tooltip_width_(0),
      keyboard_tooltip_hwnd_(NULL),
#pragma warning(suppress: 4355)
      keyboard_tooltip_factory_(this) {
  DCHECK(widget && parent);
  Init();
}

TooltipManager::~TooltipManager() {
  if (tooltip_hwnd_)
    DestroyWindow(tooltip_hwnd_);
  if (keyboard_tooltip_hwnd_)
    DestroyWindow(keyboard_tooltip_hwnd_);
}

void TooltipManager::Init() {
  // Create the tooltip control.
  tooltip_hwnd_ = CreateWindowEx(
      WS_EX_TRANSPARENT | l10n_util::GetExtendedTooltipStyles(),
      TOOLTIPS_CLASS, NULL, TTS_NOPREFIX, 0, 0, 0, 0,
      parent_, NULL, NULL, NULL);

  // This effectively turns off clipping of tooltips. We need this otherwise
  // multi-line text (\r\n) won't work right. The size doesn't really matter
  // (just as long as its bigger than the monitor's width) as we clip to the
  // screen size before rendering.
  SendMessage(tooltip_hwnd_, TTM_SETMAXTIPWIDTH, 0,
              std::numeric_limits<short>::max());

  // Add one tool that is used for all tooltips.
  toolinfo_.cbSize = sizeof(toolinfo_);
  toolinfo_.uFlags = TTF_TRANSPARENT | TTF_IDISHWND;
  toolinfo_.hwnd = parent_;
  toolinfo_.uId = reinterpret_cast<UINT_PTR>(parent_);
  // Setting this tells windows to call parent_ back (using a WM_NOTIFY
  // message) for the actual tooltip contents.
  toolinfo_.lpszText = LPSTR_TEXTCALLBACK;
  SetRectEmpty(&toolinfo_.rect);
  SendMessage(tooltip_hwnd_, TTM_ADDTOOL, 0, (LPARAM)&toolinfo_);
}

void TooltipManager::UpdateTooltip() {
  // Set last_view_out_of_sync_ to indicate the view is currently out of sync.
  // This doesn't update the view under the mouse immediately as it may cause
  // timing problems.
  last_view_out_of_sync_ = true;
  last_tooltip_view_ = NULL;
  // Hide the tooltip.
  SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
}

void TooltipManager::TooltipTextChanged(View* view) {
  if (view == last_tooltip_view_)
    UpdateTooltip(last_mouse_x_, last_mouse_y_);
}

LRESULT TooltipManager::OnNotify(int w_param, NMHDR* l_param, bool* handled) {
  *handled = false;
  if (l_param->hwndFrom == tooltip_hwnd_ && keyboard_tooltip_hwnd_ == NULL) {
    switch (l_param->code) {
      case TTN_GETDISPINFO: {
        if (last_view_out_of_sync_) {
          // View under the mouse is out of sync, determine it now.
          RootView* root_view = widget_->GetRootView();
          last_tooltip_view_ = root_view->GetViewForPoint(
              gfx::Point(last_mouse_x_, last_mouse_y_));
          last_view_out_of_sync_ = false;
        }
        // Tooltip control is asking for the tooltip to display.
        NMTTDISPINFOW* tooltip_info =
            reinterpret_cast<NMTTDISPINFOW*>(l_param);
        // Initialize the string, if we have a valid tooltip the string will
        // get reset below.
        tooltip_info->szText[0] = TEXT('\0');
        tooltip_text_.clear();
        tooltip_info->lpszText = NULL;
        clipped_text_.clear();
        if (last_tooltip_view_ != NULL) {
          tooltip_text_.clear();
          // Mouse is over a View, ask the View for it's tooltip.
          gfx::Point view_loc(last_mouse_x_, last_mouse_y_);
          View::ConvertPointToView(widget_->GetRootView(),
                                   last_tooltip_view_, &view_loc);
          if (last_tooltip_view_->GetTooltipText(view_loc.x(), view_loc.y(),
                                                 &tooltip_text_) &&
              !tooltip_text_.empty()) {
            // View has a valid tip, copy it into TOOLTIPINFO.
            clipped_text_ = tooltip_text_;
            TrimTooltipToFit(&clipped_text_, &tooltip_width_, &line_count_,
                             last_mouse_x_, last_mouse_y_, tooltip_hwnd_);
            // Adjust the clipped tooltip text for locale direction.
            l10n_util::AdjustStringForLocaleDirection(clipped_text_,
                                                      &clipped_text_);
            tooltip_info->lpszText = const_cast<WCHAR*>(clipped_text_.c_str());
          } else {
            tooltip_text_.clear();
          }
        }
        *handled = true;
        return 0;
      }
      case TTN_POP:
        tooltip_showing_ = false;
        *handled = true;
        return 0;
      case TTN_SHOW: {
        *handled = true;
        tooltip_showing_ = true;
        // The tooltip is about to show, allow the view to position it
        gfx::Point text_origin;
        if (tooltip_height_ == 0)
          tooltip_height_ = CalcTooltipHeight();
        gfx::Point view_loc(last_mouse_x_, last_mouse_y_);
        View::ConvertPointToView(widget_->GetRootView(),
                                 last_tooltip_view_, &view_loc);
        if (last_tooltip_view_->GetTooltipTextOrigin(
              view_loc.x(), view_loc.y(), &text_origin) &&
            SetTooltipPosition(text_origin.x(), text_origin.y())) {
          // Return true, otherwise the rectangle we specified is ignored.
          return TRUE;
        }
        return 0;
      }
      default:
        // Fall through.
        break;
    }
  }
  return 0;
}

bool TooltipManager::SetTooltipPosition(int text_x, int text_y) {
  // NOTE: this really only tests that the y location fits on screen, but that
  // is good enough for our usage.

  // Calculate the bounds the tooltip will get.
  gfx::Point view_loc;
  View::ConvertPointToScreen(last_tooltip_view_, &view_loc);
  RECT bounds = { view_loc.x() + text_x,
                  view_loc.y() + text_y,
                  view_loc.x() + text_x + tooltip_width_,
                  view_loc.y() + line_count_ * GetTooltipHeight() };
  SendMessage(tooltip_hwnd_, TTM_ADJUSTRECT, TRUE, (LPARAM)&bounds);

  // Make sure the rectangle completely fits on the current monitor. If it
  // doesn't, return false so that windows positions the tooltip at the
  // default location.
  gfx::Rect monitor_bounds =
      win_util::GetMonitorBoundsForRect(gfx::Rect(bounds.left,bounds.right,
                                                  0, 0));
  if (!monitor_bounds.Contains(gfx::Rect(bounds))) {
    return false;
  }

  ::SetWindowPos(tooltip_hwnd_, NULL, bounds.left, bounds.top, 0, 0,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
  return true;
}

int TooltipManager::CalcTooltipHeight() {
  // Ask the tooltip for it's font.
  int height;
  HFONT hfont = reinterpret_cast<HFONT>(
      SendMessage(tooltip_hwnd_, WM_GETFONT, 0, 0));
  if (hfont != NULL) {
    HDC dc = GetDC(tooltip_hwnd_);
    HFONT previous_font = static_cast<HFONT>(SelectObject(dc, hfont));
    int last_map_mode = SetMapMode(dc, MM_TEXT);
    TEXTMETRIC font_metrics;
    GetTextMetrics(dc, &font_metrics);
    height = font_metrics.tmHeight;
    // To avoid the DC referencing font_handle_, select the previous font.
    SelectObject(dc, previous_font);
    SetMapMode(dc, last_map_mode);
    ReleaseDC(NULL, dc);
  } else {
    // Tooltip is using the system font. Use ChromeFont, which should pick
    // up the system font.
    height = ChromeFont().height();
  }
  // Get the margins from the tooltip
  RECT tooltip_margin;
  SendMessage(tooltip_hwnd_, TTM_GETMARGIN, 0, (LPARAM)&tooltip_margin);
  return height + tooltip_margin.top + tooltip_margin.bottom;
}

void TooltipManager::TrimTooltipToFit(std::wstring* text,
                                      int* max_width,
                                      int* line_count,
                                      int position_x,
                                      int position_y,
                                      HWND window) {
  *max_width = 0;
  *line_count = 0;

  // Clamp the tooltip length to kMaxTooltipLength so that we don't
  // accidentally DOS the user with a mega tooltip (since Windows doesn't seem
  // to do this itself).
  if (text->length() > kMaxTooltipLength)
    *text = text->substr(0, kMaxTooltipLength);

  // Determine the available width for the tooltip.
  gfx::Point screen_loc(position_x, position_y);
  View::ConvertPointToScreen(widget_->GetRootView(), &screen_loc);
  gfx::Rect monitor_bounds =
      win_util::GetMonitorBoundsForRect(gfx::Rect(screen_loc.x(), screen_loc.y(),
                                                  0, 0));
  RECT tooltip_margin;
  SendMessage(window, TTM_GETMARGIN, 0, (LPARAM)&tooltip_margin);
  const int available_width = monitor_bounds.width() - tooltip_margin.left -
      tooltip_margin.right;
  if (available_width <= 0)
    return;

  // Split the string.
  std::vector<std::wstring> lines;
  SplitTooltipString(*text, &lines);
  *line_count = static_cast<int>(lines.size());

  // Format each line to fit.
  ChromeFont font = GetDefaultFont();
  std::wstring result;
  for (std::vector<std::wstring>::iterator i = lines.begin(); i != lines.end();
       ++i) {
    std::wstring elided_text = gfx::ElideText(*i, font, available_width);
    *max_width = std::max(*max_width, font.GetStringWidth(elided_text));
    if (i == lines.begin() && i + 1 == lines.end()) {
      *text = elided_text;
      return;
    }
    if (!result.empty())
      result.append(GetLineSeparator());
    result.append(elided_text);
  }
  *text = result;
}

void TooltipManager::UpdateTooltip(int x, int y) {
  RootView* root_view = widget_->GetRootView();
  View* view = root_view->GetViewForPoint(gfx::Point(x, y));
  if (view != last_tooltip_view_) {
    // NOTE: This *must* be sent regardless of the visibility of the tooltip.
    // It triggers Windows to ask for the tooltip again.
    SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
    last_tooltip_view_ = view;
  } else if (last_tooltip_view_ != NULL) {
    // Tooltip is showing, and mouse is over the same view. See if the tooltip
    // text has changed.
    gfx::Point view_point(x, y);
    View::ConvertPointToView(root_view, last_tooltip_view_, &view_point);
    std::wstring new_tooltip_text;
    if (last_tooltip_view_->GetTooltipText(view_point.x(), view_point.y(),
                                           &new_tooltip_text) &&
        new_tooltip_text != tooltip_text_) {
      // The text has changed, hide the popup.
      SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
      if (!new_tooltip_text.empty() && tooltip_showing_) {
        // New text is valid, show the popup.
        SendMessage(tooltip_hwnd_, TTM_POPUP, 0, 0);
      }
    }
  }
}

void TooltipManager::OnMouse(UINT u_msg, WPARAM w_param, LPARAM l_param) {
  int x = GET_X_LPARAM(l_param);
  int y = GET_Y_LPARAM(l_param);

  if (u_msg >= WM_NCMOUSEMOVE && u_msg <= WM_NCXBUTTONDBLCLK) {
    // NC message coordinates are in screen coordinates.
    gfx::Rect frame_bounds;
    widget_->GetBounds(&frame_bounds, true);
    x -= frame_bounds.x();
    y -= frame_bounds.y();
  }

  if (u_msg != WM_MOUSEMOVE || last_mouse_x_ != x || last_mouse_y_ != y) {
    last_mouse_x_ = x;
    last_mouse_y_ = y;
    HideKeyboardTooltip();
    UpdateTooltip(x, y);
  }
  // Forward the message onto the tooltip.
  MSG msg;
  msg.hwnd = parent_;
  msg.message = u_msg;
  msg.wParam = w_param;
  msg.lParam = l_param;
  SendMessage(tooltip_hwnd_, TTM_RELAYEVENT, 0, (LPARAM)&msg);
}

void TooltipManager::ShowKeyboardTooltip(View* focused_view) {
  if (tooltip_showing_) {
    SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
    tooltip_text_.clear();
  }
  HideKeyboardTooltip();
  std::wstring tooltip_text;
  if (!focused_view->GetTooltipText(0, 0, &tooltip_text))
    return;
  gfx::Rect focused_bounds = focused_view->bounds();
  gfx::Point screen_point;
  focused_view->ConvertPointToScreen(focused_view, &screen_point);
  gfx::Point relative_point_coordinates;
  focused_view->ConvertPointToWidget(focused_view, &relative_point_coordinates);
  keyboard_tooltip_hwnd_ = CreateWindowEx(
      WS_EX_TRANSPARENT | l10n_util::GetExtendedTooltipStyles(),
      TOOLTIPS_CLASS, NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
  SendMessage(keyboard_tooltip_hwnd_, TTM_SETMAXTIPWIDTH, 0,
              std::numeric_limits<short>::max());
  int tooltip_width;
  int line_count;
  TrimTooltipToFit(&tooltip_text, &tooltip_width, &line_count,
                   relative_point_coordinates.x(),
                   relative_point_coordinates.y(), keyboard_tooltip_hwnd_);
  TOOLINFO keyboard_toolinfo;
  memset(&keyboard_toolinfo, 0, sizeof(keyboard_toolinfo));
  keyboard_toolinfo.cbSize = sizeof(keyboard_toolinfo);
  keyboard_toolinfo.hwnd = parent_;
  keyboard_toolinfo.uFlags = TTF_TRACK | TTF_TRANSPARENT | TTF_IDISHWND ;
  keyboard_toolinfo.lpszText = const_cast<WCHAR*>(tooltip_text.c_str());
  SendMessage(keyboard_tooltip_hwnd_, TTM_ADDTOOL, 0,
              reinterpret_cast<LPARAM>(&keyboard_toolinfo));
  SendMessage(keyboard_tooltip_hwnd_, TTM_TRACKACTIVATE,  TRUE,
              reinterpret_cast<LPARAM>(&keyboard_toolinfo));
  if (!tooltip_height_)
    tooltip_height_ = CalcTooltipHeight();
  RECT rect_bounds = {screen_point.x(),
                      screen_point.y() + focused_bounds.height(),
                      screen_point.x() + tooltip_width,
                      screen_point.y() + focused_bounds.height() +
                      line_count * tooltip_height_ };
  gfx::Rect monitor_bounds =
      win_util::GetMonitorBoundsForRect(gfx::Rect(rect_bounds));
  rect_bounds = gfx::Rect(rect_bounds).AdjustToFit(monitor_bounds).ToRECT();
  ::SetWindowPos(keyboard_tooltip_hwnd_, NULL, rect_bounds.left,
                 rect_bounds.top, 0, 0,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      keyboard_tooltip_factory_.NewRunnableMethod(
      &TooltipManager::DestroyKeyboardTooltipWindow, keyboard_tooltip_hwnd_),
      kDefaultTimeout);
}

void TooltipManager::HideKeyboardTooltip() {
  if (keyboard_tooltip_hwnd_ != NULL) {
    SendMessage(keyboard_tooltip_hwnd_, WM_CLOSE, 0, 0);
    keyboard_tooltip_hwnd_ = NULL;
  }
}

void TooltipManager::DestroyKeyboardTooltipWindow(HWND window_to_destroy) {
  if (keyboard_tooltip_hwnd_ == window_to_destroy)
    HideKeyboardTooltip();
}

}  // namespace views
