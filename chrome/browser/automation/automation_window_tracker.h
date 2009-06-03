// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_WINDOW_TRACKER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_WINDOW_TRACKER_H_

#include "base/gfx/native_widget_types.h"
#include "build/build_config.h"
#include "chrome/browser/automation/automation_resource_tracker.h"
#include "chrome/common/native_window_notification_source.h"

class AutomationWindowTracker
    : public AutomationResourceTracker<gfx::NativeWindow> {
 public:
  AutomationWindowTracker(IPC::Message::Sender* automation)
      : AutomationResourceTracker<gfx::NativeWindow>(automation) { }
  virtual ~AutomationWindowTracker() {
  }

  virtual void AddObserver(gfx::NativeWindow resource) {
    registrar_.Add(this, NotificationType::WINDOW_CLOSED,
                   Source<gfx::NativeWindow>(resource));
  }

  virtual void RemoveObserver(gfx::NativeWindow resource) {
    registrar_.Remove(this, NotificationType::WINDOW_CLOSED,
                      Source<gfx::NativeWindow>(resource));
  }
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_WINDOW_TRACKER_H_
