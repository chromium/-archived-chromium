// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_REPEAT_CONTROLLER_H_
#define CHROME_VIEWS_REPEAT_CONTROLLER_H_

#include "base/scoped_ptr.h"
#include "base/timer.h"

namespace views {

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
class RepeatController {
 public:
  typedef Callback0::Type RepeatCallback;

  // The RepeatController takes ownership of this callback object.
  explicit RepeatController(RepeatCallback* callback);
  virtual ~RepeatController();

  // Start repeating.
  void Start();

  // Stop repeating.
  void Stop();

 private:
  RepeatController();

  // Called when the timer expires.
  void Run();

  // The current timer.
  base::OneShotTimer<RepeatController> timer_;

  scoped_ptr<RepeatCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(RepeatController);
};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_REPEAT_CONTROLLER_H_
