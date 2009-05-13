// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlwin.h>

#include "base/logging.h"

// Helper class for moving a window to the foreground.
// Windows XP and later will not allow a window which is in the background to
// move to the foreground, unless requested by the current window in the
// foreground.  For automated testing, we really want some of our windows
// to be capable of moving to the foreground.
//
// This is probably leveraging a windows bug.
class ForegroundHelper : public CWindowImpl<ForegroundHelper> {
 public:
BEGIN_MSG_MAP(ForegroundHelper)
  MESSAGE_HANDLER(WM_HOTKEY, OnHotKey)
END_MSG_MAP()

  // Brings a window into the foreground.
  // Can be called from any window, even if the caller is not the
  // foreground window.
  static HRESULT SetForeground(HWND window) {
    DCHECK(::IsWindow(window));
    ForegroundHelper foreground_helper;
    CHECK(foreground_helper.ForegroundHotKey(window) == S_OK);
    return S_OK;
  }

 private:
  HRESULT ForegroundHotKey(HWND window) {
    // This implementation registers a hot key (F22) and then
    // triggers the hot key.  When receiving the hot key, we'll
    // be in the foreground and allowed to move the target window
    // into the foreground too.

    if(NULL == Create(NULL, NULL, NULL, WS_POPUP))
      return AtlHresultFromLastError();

    static const int hotkey_id = 0x0000baba;

    // Store the target window into our USERDATA for use in our
    // HotKey handler.
    SetWindowLongPtr(GWLP_USERDATA, reinterpret_cast<ULONG_PTR>(window));
    RegisterHotKey(m_hWnd, hotkey_id, 0, VK_F22);

    // If the calling thread is not yet a UI thread, call PeekMessage
    // to ensure creation of its message queue.
    MSG msg = {0};
    PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);

    // Send the Hotkey.
    INPUT hotkey = {0};
    hotkey.type = INPUT_KEYBOARD;
    hotkey.ki.wVk =  VK_F22;
    if (1 != SendInput(1, &hotkey, sizeof(hotkey)))
      return E_FAIL;

    // Loop until we get the key.
    // TODO: It may be possible to get stuck here if the
    // message gets lost?
    while(GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);

      if(WM_HOTKEY == msg.message)
        break;
    }

    UnregisterHotKey(m_hWnd, hotkey_id);
    DestroyWindow();

    return S_OK;
  }

  // Handle the registered Hotkey being pressed.
  LRESULT OnHotKey(UINT /*uMsg*/, WPARAM /*wParam*/,
		   LPARAM /*lParam*/, BOOL& bHandled) {
    HWND window = reinterpret_cast<HWND>(GetWindowLongPtr(GWLP_USERDATA));
    SetForegroundWindow(window);
    return 1;
  }
};
