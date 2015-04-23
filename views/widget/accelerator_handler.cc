// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/accelerator_handler.h"

#include "views/focus/focus_manager.h"

namespace views {

AcceleratorHandler::AcceleratorHandler() {
}

bool AcceleratorHandler::Dispatch(const MSG& msg) {
  bool process_message = true;

  if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST) {
    FocusManager* focus_manager =
        FocusManager::GetFocusManagerForNativeView(msg.hwnd);
    if (focus_manager) {
      // FocusManager::OnKeyDown and OnKeyUp return false if this message has
      // been consumed and should not be propagated further.
      switch (msg.message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
          process_message = focus_manager->OnKeyDown(msg.hwnd, msg.message,
              msg.wParam, msg.lParam);
          break;

        case WM_KEYUP:
        case WM_SYSKEYUP:
          process_message = focus_manager->OnKeyUp(msg.hwnd, msg.message,
              msg.wParam, msg.lParam);
          break;
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
