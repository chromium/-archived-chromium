// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/widget/accelerator_handler.h"

#include "chrome/views/focus/focus_manager.h"

namespace views {

AcceleratorHandler::AcceleratorHandler() {
}

bool AcceleratorHandler::Dispatch(const MSG& msg) {
  bool process_message = true;
  if ((msg.message == WM_KEYDOWN) || (msg.message == WM_SYSKEYDOWN)) {
    FocusManager* focus_manager = FocusManager::GetFocusManager(msg.hwnd);
    if (focus_manager) {
      // FocusManager::OnKeyDown returns false if this message has been
      // consumed and should not be propogated further
      if (!focus_manager->OnKeyDown(msg.hwnd, msg.message, msg.wParam,
                                    msg.lParam)) {
        process_message = false;
      }
    }
  }
  if (process_message) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return true;
}

}  // namespace views
