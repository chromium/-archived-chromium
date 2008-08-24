// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_REPEAT_CONTROLLER_H__
#define CHROME_VIEWS_REPEAT_CONTROLLER_H__

#include "base/message_loop.h"
#include "base/task.h"

namespace ChromeViews {

///////////////////////////////////////////////////////////////////////////////
//
// RepeatController
//
//  An object that handles auto-repeating UI actions. There is a longer initial
//  delay after which point repeats become constant. Users provide a callback
//  that is notified when each repeat occurs so that they can perform the
//  associated action.
//
///////////////////////////////////////////////////////////////////////////////
class RepeatController : public Task {
 public:
  typedef Callback0::Type RepeatCallback;

  // The RepeatController takes ownership of this callback object.
  explicit RepeatController(RepeatCallback* callback);
  virtual ~RepeatController();

  // Start repeating.
  void Start();

  // Stop repeating.
  void Stop();

  // Task implementation:
  void Run();

 private:
  RepeatController();

  // Stop and delete the timer.
  void DestroyTimer();

  // The current timer.
  Timer* timer_;

  scoped_ptr<RepeatCallback> callback_;

  DISALLOW_EVIL_CONSTRUCTORS(RepeatController);
};

}

#endif  // #ifndef CHROME_VIEWS_REPEAT_CONTROLLER_H__

