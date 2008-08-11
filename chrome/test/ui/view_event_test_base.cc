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

#include "chrome/test/ui/view_event_test_base.h"

#include "base/message_loop.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/views/window.h"

namespace {

// View subclass that allows you to specify the preferred size.
class TestView : public ChromeViews::View {
 public:
  TestView() {}

  void set_preferred_size(const gfx::Size& size) { preferred_size_ = size; }
  void GetPreferredSize(CSize* out) {
    if (!preferred_size_.IsEmpty())
      *out = preferred_size_.ToSIZE();
    else
      View::GetPreferredSize(out);
  }

 private:
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(TestView);
};

// Delay in background thread before posting mouse move.
const int kMouseMoveDelayMS = 200;

}  // namespace

// static
void ViewEventTestBase::Done() {
  MessageLoop::current()->Quit();
  MessageLoop::current()->Quit();
}

ViewEventTestBase::ViewEventTestBase() : window_(NULL), content_view_(NULL) { }

void ViewEventTestBase::SetUp() {
  OleInitialize(NULL);
  window_ = ChromeViews::Window::CreateChromeWindow(NULL, gfx::Rect(), this);
}

void ViewEventTestBase::TearDown() {
  if (window_) {
    DestroyWindow(window_->GetHWND());
    window_ = NULL;
  }
  OleUninitialize();
}

ChromeViews::View* ViewEventTestBase::GetContentsView() {
  if (!content_view_) {
    // Wrap the real view (as returned by CreateContentsView) in a View so
    // that we can customize the preferred size.
    TestView* test_view = new TestView();
    test_view->SetLayoutManager(new ChromeViews::FillLayout());
    test_view->set_preferred_size(GetPreferredSize());
    test_view->AddChildView(CreateContentsView());
    content_view_ = test_view;
  }
  return content_view_;
}

void ViewEventTestBase::StartMessageLoopAndRunTest() {
  window_->Show();
  // Make sure the window is the foreground window, otherwise none of the
  // mouse events are going to be targeted correctly.
  SetForegroundWindow(window_->GetHWND());

  // Flush any pending events to make sure we start with a clean slate.
  MessageLoop::current()->RunAllPending();

  // Schedule a task that starts the test. Need to do this as we're going to
  // run the message loop.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      NewRunnableMethod(this, &ViewEventTestBase::DoTestOnMessageLoop), 0);

  MessageLoop::current()->Run();
}

void ViewEventTestBase::ScheduleMouseMoveInBackground(int x, int y) {
  if (!dnd_thread_.get()) {
    dnd_thread_.reset(new Thread("mouse-move-thread"));
    dnd_thread_->Start();
  }
  dnd_thread_->message_loop()->PostDelayedTask(
      FROM_HERE, NewRunnableFunction(&ui_controls::SendMouseMove, x, y),
      kMouseMoveDelayMS);
}

void ViewEventTestBase::StopBackgroundThread() {
  dnd_thread_.reset(NULL);
}

void ViewEventTestBase::RunTestMethod(Task* task) {
  StopBackgroundThread();

  scoped_ptr<Task> task_deleter(task);
  task->Run();
  if (HasFatalFailure())
    Done();
}
