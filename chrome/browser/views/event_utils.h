// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_EVENT_UTILS_H__
#define CHROME_BROWSER_VIEWS_EVENT_UTILS_H__

#include "webkit/glue/window_open_disposition.h"

namespace views {
class MouseEvent;
}

namespace event_utils {

// Translates event flags into what kind of disposition they represents.
// For example, a middle click would mean to open a background tab.
// event_flags are the flags as understood by views::MouseEvent.
WindowOpenDisposition DispositionFromEventFlags(int event_flags);

// Returns true if the specified mouse event may have a
// WindowOptionDisposition.
bool IsPossibleDispositionEvent(const views::MouseEvent& event);

}

#endif  // CHROME_BROWSER_VIEWS_EVENT_UTILS_H__
