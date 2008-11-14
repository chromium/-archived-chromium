// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The SuspendController keeps track of actions related to the computer going
// into power savings mode. Its purpose right now is to close all network
// requests and prevent creation of new requests until the computer resumes.

#ifndef CHROME_BROWSER_SUSPEND_CONTROLLER_H__
#define CHROME_BROWSER_SUSPEND_CONTROLLER_H__

#include "base/basictypes.h"
#include "chrome/browser/profile.h"

class Profile;

// The browser process owns the only instance of this class.
class SuspendController : public base::RefCountedThreadSafe<SuspendController> {
 public:
  SuspendController() {}
  ~SuspendController() {}

  // Called when the system is going to be suspended.
  static void OnSuspend(Profile* profile);

  // Called when the system has been resumed.
  static void OnResume(Profile* profile);

 private:
  // Run on the io_thread.
  void StopRequests(Profile* profile);
  void AllowNewRequests(Profile* profile);

  static bool is_suspended_;

  DISALLOW_EVIL_CONSTRUCTORS(SuspendController);
};

#endif  // CHROME_BROWSER_SUSPEND_CONTROLLER_H__

