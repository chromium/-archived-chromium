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
