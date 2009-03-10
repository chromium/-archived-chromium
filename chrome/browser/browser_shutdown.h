// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SHUTDOWN_H__
#define CHROME_BROWSER_BROWSER_SHUTDOWN_H__

class PrefService;

namespace browser_shutdown {

// Should Shutdown() delete the ResourceBundle? This is normally true, but set
// to false for in process unit tests.
extern bool delete_resources_on_shutdown;

enum ShutdownType {
  // an uninitialized value
  NOT_VALID = 0,
  // the last browser window was closed
  WINDOW_CLOSE,
  // user clicked on the Exit menu item
  BROWSER_EXIT,
  // windows is logging off or shutting down
  END_SESSION
};

void RegisterPrefs(PrefService* local_state);

// Called when the browser starts shutting down so that we can measure shutdown
// time.
void OnShutdownStarting(ShutdownType type);

// Invoked in two ways:
// . When the last browser has been deleted and the message loop has finished
//   running.
// . When ChromeFrame::EndSession is invoked and we need to do cleanup.
//   NOTE: in this case the message loop is still running, but will die soon
//         after this returns.
void Shutdown();

// Called at startup to create a histogram from our previous shutdown time.
void ReadLastShutdownInfo();

}  // namespace browser_shutdown

#endif  // CHROME_BROWSER_BROWSER_SHUTDOWN_H__
