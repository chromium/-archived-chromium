// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_ACTIVE_WINDOW_WATCHER_H_
#define CHROME_BROWSER_GTK_ACTIVE_WINDOW_WATCHER_H_

#include "base/basictypes.h"

typedef void* gpointer;
typedef void GdkXEvent;
typedef union _GdkEvent GdkEvent;

// This is a helper class that is used to keep track of which window the window
// manager thinks is active.
class ActiveWindowWatcher {
 public:
  ActiveWindowWatcher();

 private:
  void Init();

  // Sends a notification out through the NotificationService that the active
  // window has changed.
  void NotifyActiveWindowChanged();

  // Callback for PropertyChange XEvents.
  static GdkFilterReturn OnWindowXEvent(GdkXEvent* xevent,
                                        GdkEvent* event,
                                        gpointer window_watcher);

  DISALLOW_COPY_AND_ASSIGN(ActiveWindowWatcher);
};

#endif  // CHROME_BROWSER_GTK_ACTIVE_WINDOW_WATCHER_H_
