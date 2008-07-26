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

// Throbbers display an animation, usually used as a status indicator.

#ifndef CHROME_VIEWS_THROBBER_H__
#define CHROME_VIEWS_THROBBER_H__

#include "base/basictypes.h"
#include "base/task.h"
#include "chrome/views/view.h"

class SkBitmap;
class Timer;

namespace ChromeViews {

class Throbber : public ChromeViews::View,
                 public Task {
 public:
  // |frame_time_ms| is the amount of time that should elapse between frames
  //                 (in milliseconds)
  // If |paint_while_stopped| is false, this view will be invisible when not
  // running.
  Throbber(int frame_time_ms, bool paint_while_stopped);
  virtual ~Throbber();

  // Start and stop the throbber animation
  virtual void Start();
  virtual void Stop();

  // overridden from View
  virtual void GetPreferredSize(CSize *out);
  virtual void Paint(ChromeCanvas* canvas);

  // implemented from Task
  virtual void Run();

 protected:
  // Specifies whether the throbber is currently animating or not
  bool running_;

 private:
  bool paint_while_stopped_;
  int frame_count_;
  int last_frame_drawn_;
  DWORD start_time_;
  DWORD last_time_recorded_;
  SkBitmap* frames_;
  int frame_time_ms_;
  Timer* timer_;

  DISALLOW_EVIL_CONSTRUCTORS(Throbber);
};

// A SmoothedThrobber is a throbber that is representing potentially short
// and nonoverlapping bursts of work.  SmoothedThrobber ignores small
// pauses in the work stops and starts, and only starts its throbber after
// a small amount of work time has passed.
class SmoothedThrobber : public Throbber {
 public:
  SmoothedThrobber(int frame_delay_ms);

  virtual void Start();
  virtual void Stop();

 private:
  // Called when the startup-delay timer fires
  // This function starts the actual throbbing.
  void StartDelayOver();

  // Called when the shutdown-delay timer fires.
  // This function stops the actual throbbing.
  void StopDelayOver();

  // Method factory for delaying throbber startup.
  ScopedRunnableMethodFactory<SmoothedThrobber> start_delay_factory_;
  // Method factory for delaying throbber shutdown.
  ScopedRunnableMethodFactory<SmoothedThrobber> end_delay_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(SmoothedThrobber);
};

// A CheckmarkThrobber is a special variant of throbber that has three states:
//   1. not yet completed (which paints nothing)
//   2. working (which paints the throbber animation)
//   3. completed (which paints a checkmark)
//
class CheckmarkThrobber : public Throbber {
 public:
  CheckmarkThrobber();

  // If checked is true, the throbber stops spinning and displays a checkmark.
  // If checked is false, the throbber stops spinning and displays nothing.
  void SetChecked(bool checked);

  // Overridden from ChromeViews::Throbber:
  virtual void Paint(ChromeCanvas* canvas);

 private:
  static const int kFrameTimeMs = 30;

  static void InitClass();

  // Whether or not we should display a checkmark.
  bool checked_;

  // The checkmark image.
  static SkBitmap* checkmark_;

  DISALLOW_EVIL_CONSTRUCTORS(CheckmarkThrobber);
};

}  // namespace ChromeViews

#endif  // CHROME_VIEWS_THROBBER_H__
