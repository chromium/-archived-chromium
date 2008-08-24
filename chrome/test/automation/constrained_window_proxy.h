// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_CONSTRAINED_WINDOW_PROXY_H__
#define CHROME_TEST_AUTOMATION_CONSTRAINED_WINDOW_PROXY_H__

#include <string>

#include "chrome/test/automation/automation_handle_tracker.h"

namespace gfx {
class Rect;
}

class ConstrainedWindowProxy : public AutomationResourceProxy {
public:
  ConstrainedWindowProxy(AutomationMessageSender* sender,
                         AutomationHandleTracker* tracker,
                         int handle)
      : AutomationResourceProxy(tracker, sender, handle) {}

  virtual ~ConstrainedWindowProxy() {}

  bool GetTitle(std::wstring* title) const;
  bool GetBoundsWithTimeout(gfx::Rect* bounds,
                            uint32 timeout_ms,
                            bool* is_timeout);

private:
  DISALLOW_EVIL_CONSTRUCTORS(ConstrainedWindowProxy);
};

#endif  // CHROME_TEST_AUTOMATION_CONSTRAINED_WINDOW_PROXY_H__
