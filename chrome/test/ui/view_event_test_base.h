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

#ifndef CHROME_TEST_UI_VIEW_EVENT_TEST_BASE_H_
#define CHROME_TEST_UI_VIEW_EVENT_TEST_BASE_H_

#include "base/thread.h"
#include "chrome/views/window_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

class Task;

// Base class for Views based tests that dispatch events.
//
// As views based event test involves waiting for events to be processed,
// writing a views based test is slightly different than that of writing
// other unit tests. In particular when the test fails or is done you need
// to stop the message loop. This can be done by way of invoking the Done
// method.
//
// Any delayed callbacks should be done by way of CreateEventTask.
// CreateEventTask checks to see if ASSERT_XXX has been invoked after invoking
// the task. If there was a failure Done is invoked and the test stops.
//
// ViewEventTestBase creates a Window with the View returned from
// CreateContentsView. The preferred size for the view can be customized by
// overriding GetPreferredSize. If you do not override GetPreferredSize the
// preferred size of the view returned from CreateContentsView is used.
//
// Subclasses of ViewEventTestBase must implement two methods:
// . DoTestOnMessageLoop: invoked when the message loop is running. Run your
//   test here, invoke Done when done.
// . CreateContentsView: returns the view to place in the window.
//
// Once you have created a ViewEventTestBase use the macro VIEW_TEST to define
// the fixture.
//
// I encountered weird timing problems in initiating dragging and drop that
// necessitated ugly hacks. In particular when the hook installed by
// ui_controls received the mouse event and posted a task that task was not
// processed. To work around this use the following pattern when initiating
// dnd:
//   // Schedule the mouse move at a location slightly different from where
//   // you really want to move to.
//   ui_controls::SendMouseMoveNotifyWhenDone(loc.x + 10, loc.y,
//       NewRunnableMethod(this, YYY));
//   // Then use this to schedule another mouse move.
//   ScheduleMouseMoveInBackground(loc.x, loc.y);

class ViewEventTestBase : public ChromeViews::WindowDelegate,
                          public testing::Test {
 public:
  // Invoke when done either because of failure or success. Quits the message
  // loop.
  static void Done();

  ViewEventTestBase();

  // Creates a window.
  virtual void SetUp();

  // Destroys the window.
  virtual void TearDown();

  virtual bool CanResize() const {
    return true;
  }

  // WindowDelegate method. Calls into CreateContentsView to get the actual
  // view.
  virtual ChromeViews::View* GetContentsView();

  // Overriden to do nothing so that this class can be used in runnable tasks.
  void AddRef() {}
  void Release() {}

 protected:
  // Returns the view that is added to the window. 
  virtual ChromeViews::View* CreateContentsView() = 0;

  // Called once the message loop is running.
  virtual void DoTestOnMessageLoop() = 0;

  // Invoke from test main. Shows the window, starts the message loop and
  // schedules a task that invokes DoTestOnMessageLoop.
  void StartMessageLoopAndRunTest();

  // Returns an empty Size. Subclasses that want a preferred size other than
  // that of the View returned by CreateContentsView should override this
  // appropriately.
  virtual gfx::Size GetPreferredSize() { return gfx::Size(); }

  // Creates a task that calls the specified method back. The specified
  // method is called in such a way that if there are any test failures
  // Done is invoked.
  template <class T, class Method>
  Task* CreateEventTask(T* target, Method method) {
    return NewRunnableMethod(this, &ViewEventTestBase::RunTestMethod,
                             NewRunnableMethod(target, method));
  }

  // Spawns a new thread posts a MouseMove in the background.
  void ScheduleMouseMoveInBackground(int x, int y);

  ChromeViews::Window* window_;

 private:
  // Stops the thread started by ScheduleMouseMoveInBackground.
  void StopBackgroundThread();

  // Callback from CreateEventTask. Stops the background thread, runs the
  // supplied task and if there are failures invokes Done.
  void RunTestMethod(Task* task);

  // The content of the Window.
  ChromeViews::View* content_view_;

  // Thread for posting background MouseMoves.
  scoped_ptr<Thread> dnd_thread_;

  DISALLOW_COPY_AND_ASSIGN(ViewEventTestBase);
};

// Convenience macro for defining a ViewEventTestBase. See class description 
// of ViewEventTestBase for details.
//
// NOTE: These tests are disabled until we get a buildbot that is always logged
// in and can run them.
#define VIEW_TEST(test_class, name) \
  TEST_F(test_class, DISABLED_name) {\
    StartMessageLoopAndRunTest();\
  }

#endif  // CHROME_TEST_UI_VIEW_EVENT_TEST_BASE_H_
