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
