// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/event_utils.h"

#include "views/event.h"

using views::Event;

namespace event_utils {

WindowOpenDisposition DispositionFromEventFlags(int event_flags) {
  if (((event_flags & Event::EF_MIDDLE_BUTTON_DOWN) ==
          Event::EF_MIDDLE_BUTTON_DOWN) ||
      ((event_flags & Event::EF_CONTROL_DOWN) ==
          Event::EF_CONTROL_DOWN)) {
    return ((event_flags & Event::EF_SHIFT_DOWN) ==  Event::EF_SHIFT_DOWN) ?
        NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
  }

  if ((event_flags & Event::EF_SHIFT_DOWN) == Event::EF_SHIFT_DOWN)
    return NEW_WINDOW;
  return false /*event.IsAltDown()*/ ? SAVE_TO_DISK : CURRENT_TAB;
}

bool IsPossibleDispositionEvent(const views::MouseEvent& event) {
  return event.IsLeftMouseButton() || event.IsMiddleMouseButton();
}

}
