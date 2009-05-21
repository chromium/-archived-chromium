// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_WINDOW_TRACKER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_WINDOW_TRACKER_H_

#include "base/gfx/native_widget_types.h"
#include "build/build_config.h"
#include "chrome/browser/automation/automation_resource_tracker.h"

#if defined(OS_WIN)
// Since HWNDs aren't pointers, we can't have NativeWindow
// be directly a pointer and so must explicitly declare the Source types
// for it.
#include "chrome/common/hwnd_notification_source.h"
#elif defined(OS_LINUX) || defined(OS_MACOSX)
// But on Linux and Mac, it is a pointer so this definition suffices.
template<>
class Source<gfx::NativeWindow> : public NotificationSource {
 public:
  explicit Source(gfx::NativeWindow win) : NotificationSource(win) {}

  explicit Source(const NotificationSource& other)
      : NotificationSource(other) {}

  gfx::NativeWindow operator->() const { return ptr(); }
  gfx::NativeWindow ptr() const { return static_cast<gfx::NativeWindow>(ptr_); }
};
#endif

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
