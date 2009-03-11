// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_MEMORY_WATCHER_HOTKEY_H_
#define TOOLS_MEMORY_WATCHER_HOTKEY_H_

#include <atlbase.h>
#include <atlwin.h>
#include "base/logging.h"

// HotKey handler.
// Programs wishing to register a hotkey can use this.
class HotKeyHandler : public CWindowImpl<HotKeyHandler> {
 public:
  HotKeyHandler(UINT modifiers, UINT vk)
    : m_modifiers(modifiers),
      m_vkey(vk) {
    Start();
  }
  ~HotKeyHandler() { Stop(); }

BEGIN_MSG_MAP(HotKeyHandler)
  MESSAGE_HANDLER(WM_HOTKEY, OnHotKey)
END_MSG_MAP()

 private:
  static const int hotkey_id = 0x0000baba;

  bool Start() {
    if (NULL == Create(NULL, NULL, NULL, WS_POPUP))
      return false;
    return RegisterHotKey(m_hWnd, hotkey_id, m_modifiers, m_vkey) == TRUE;
  }

  void Stop() {
    UnregisterHotKey(m_hWnd, hotkey_id);
    DestroyWindow();
  }

  // Handle the registered Hotkey being pressed.
  virtual LRESULT OnHotKey(UINT /*uMsg*/, WPARAM /*wParam*/,
                           LPARAM /*lParam*/, BOOL& bHandled) = 0;

  UINT m_modifiers;
  UINT m_vkey;
};

#endif  // TOOLS_MEMORY_WATCHER_HOTKEY_H_
