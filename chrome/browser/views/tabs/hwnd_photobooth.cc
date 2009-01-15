// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/point.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tabs/hwnd_photobooth.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/views/widget_win.h"
#include "skia/include/SkBitmap.h"

namespace {

static BOOL CALLBACK MonitorEnumProc(HMONITOR monitor, HDC monitor_dc,
                                     RECT* monitor_rect, LPARAM data) {
  gfx::Point* point = reinterpret_cast<gfx::Point*>(data);
  if (monitor_rect->right > point->x() && monitor_rect->bottom > point->y()) {
    point->set_x(monitor_rect->right);
    point->set_y(monitor_rect->bottom);
  }
  return TRUE;
}

gfx::Point GetCaptureWindowPosition() {
  // Since the capture window must be visible to be painted, it must be opened
  // off screen to avoid flashing. But if it is opened completely off-screen
  // (e.g. at 0xFFFFx0xFFFF) then on Windows Vista it will not paint even if it
  // _is_ visible. So we need to find the right/bottommost monitor, and
  // position it so that 1x1 pixel is on-screen on that monitor which is enough
  // to convince Vista to paint it. Don't ask why this is so - this appears to
  // be a regression over XP.
  gfx::Point point(0, 0);
  EnumDisplayMonitors(NULL, NULL, &MonitorEnumProc,
                      reinterpret_cast<LPARAM>(&point));
  return gfx::Point(point.x() - 1, point.y() - 1);
}

}

///////////////////////////////////////////////////////////////////////////////
// HWNDPhotobooth, public:

HWNDPhotobooth::HWNDPhotobooth(HWND initial_hwnd)
    : capture_window_(NULL),
      current_hwnd_(initial_hwnd) {
  DCHECK(IsWindow(current_hwnd_));
  CreateCaptureWindow(initial_hwnd);
}

HWNDPhotobooth::~HWNDPhotobooth() {
  // Detach the attached HWND. The creator of the photo-booth is responsible
  // for destroying it.
  ReplaceHWND(NULL);
  capture_window_->Close();
}

void HWNDPhotobooth::ReplaceHWND(HWND new_hwnd) {
  if (IsWindow(current_hwnd_) &&
      GetParent(current_hwnd_) == capture_window_->GetHWND()) {
    // We need to hide the window too, so it doesn't show up in the TaskBar or
    // be parented to the desktop.
    ShowWindow(current_hwnd_, SW_HIDE);
    SetParent(current_hwnd_, NULL);
  }
  current_hwnd_ = new_hwnd;

  if (IsWindow(new_hwnd)) {
    // Insert the TabContents into the capture window.
    SetParent(current_hwnd_, capture_window_->GetHWND());

    // Show the window (it may not be visible). This is the only safe way of
    // doing this. ShowWindow does not work.
    SetWindowPos(current_hwnd_, NULL, 0, 0, 0, 0,
                 SWP_DEFERERASE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                     SWP_NOOWNERZORDER | SWP_NOSENDCHANGING | SWP_NOZORDER |
                     SWP_SHOWWINDOW | SWP_NOSIZE);
  }
}

void HWNDPhotobooth::PaintScreenshotIntoCanvas(
    ChromeCanvas* canvas,
    const gfx::Rect& target_bounds) {
  // Our contained window may have been re-parented. Make sure it belongs to
  // us until someone calls ReplaceHWND(NULL).
  if (IsWindow(current_hwnd_) &&
      GetParent(current_hwnd_) != capture_window_->GetHWND()) {
    ReplaceHWND(current_hwnd_);
  }

  // We compel the contained HWND to paint now, synchronously. We do this to
  // populate the device context with valid and current data.
  RedrawWindow(current_hwnd_, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);

  // Transfer the contents of the layered capture window to the screen-shot
  // canvas' DIB.
  HDC target_dc = canvas->beginPlatformPaint();
  HDC source_dc = GetDC(current_hwnd_);
  RECT window_rect = {0};
  GetWindowRect(current_hwnd_, &window_rect);
  BitBlt(target_dc, target_bounds.x(), target_bounds.y(),
         target_bounds.width(), target_bounds.height(), source_dc, 0, 0,
         SRCCOPY);
  // Windows screws up the alpha channel on all text it draws, and so we need
  // to call makeOpaque _after_ the blit to correct for this.
  canvas->getTopPlatformDevice().makeOpaque(target_bounds.x(),
                                            target_bounds.y(),
                                            target_bounds.width(),
                                            target_bounds.height());
  ReleaseDC(current_hwnd_, source_dc);
  canvas->endPlatformPaint();
}

///////////////////////////////////////////////////////////////////////////////
// HWNDPhotobooth, private:

void HWNDPhotobooth::CreateCaptureWindow(HWND initial_hwnd) {
  // Snapshotting a HWND is tricky - if the HWND is clipped (e.g. positioned
  // partially off-screen) then just blitting from the HWND' DC to the capture
  // bitmap would be incorrect, since the capture bitmap would show only the
  // visible area of the HWND.
  //
  // The approach turns out to be to create a second layered window in
  // hyperspace the to act as a "photo booth." The window is created with the
  // size of the unclipped HWND, and we attach the HWND as a child, refresh the
  // HWND' by calling |Paint| on it, and then blitting from the HWND's DC to
  // the capture bitmap. This results in the entire unclipped HWND display
  // bitmap being captured.
  //
  // The capture window must be layered so that Windows generates a backing
  // store for it, so that blitting from a child window's DC produces data. If
  // the window is not layered, because it is off-screen Windows does not
  // retain its contents and blitting results in blank data. The capture window
  // is a "basic" (1 level of alpha) layered window because that is the mode
  // that supports having child windows (variable alpha layered windows do not
  // support child HWNDs).
  //
  // This function sets up the off-screen capture window, and attaches the
  // associated HWND to it. Note that the details are important here, see below
  // for further comments.
  //
  CRect contents_rect;
  GetClientRect(initial_hwnd, &contents_rect);
  gfx::Point window_position = GetCaptureWindowPosition();
  gfx::Rect capture_bounds(window_position.x(), window_position.y(),
                           contents_rect.Width(), contents_rect.Height());
  capture_window_ = new views::WidgetWin;
  capture_window_->set_window_style(WS_POPUP);
  // WS_EX_TOOLWINDOW ensures the capture window doesn't produce a Taskbar
  // button.
  capture_window_->set_window_ex_style(WS_EX_LAYERED | WS_EX_TOOLWINDOW);
  capture_window_->Init(NULL, capture_bounds, false);
  // If the capture window isn't visible, blitting from the TabContents'
  // HWND's DC to the capture bitmap produces blankness.
  capture_window_->ShowWindow(SW_SHOWNOACTIVATE);
  SetLayeredWindowAttributes(
      capture_window_->GetHWND(), RGB(0xFF, 0xFF, 0xFF), 0xFF, LWA_ALPHA);

  ReplaceHWND(initial_hwnd);
}
