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
