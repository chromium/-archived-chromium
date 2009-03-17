// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interactive_ui/view_event_test_base.h"

#include "base/message_loop.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/views/view.h"
#include "chrome/views/window/window.h"

namespace {

// View subclass that allows you to specify the preferred size.
class TestView : public views::View {
 public:
  TestView() {}

  void set_preferred_size(const gfx::Size& size) { preferred_size_ = size; }
  gfx::Size GetPreferredSize() {
    if (!preferred_size_.IsEmpty())
      return preferred_size_;
    return View::GetPreferredSize();
  }

  virtual void Layout() {
    View* child_view = GetChildViewAt(0);
    child_view->SetBounds(0, 0, width(), height());
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
  // If we're in a nested message loop, as is the case with menus, we need
  // to quit twice. The second quit does that for us.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, new MessageLoop::QuitTask(), 0);
}

ViewEventTestBase::ViewEventTestBase() : window_(NULL), content_view_(NULL) { }

void ViewEventTestBase::SetUp() {
  OleInitialize(NULL);
  window_ = views::Window::CreateChromeWindow(NULL, gfx::Rect(), this);
}

void ViewEventTestBase::TearDown() {
  if (window_) {
    DestroyWindow(window_->GetNativeWindow());
    window_ = NULL;
  }
  OleUninitialize();
}

views::View* ViewEventTestBase::GetContentsView() {
  if (!content_view_) {
    // Wrap the real view (as returned by CreateContentsView) in a View so
    // that we can customize the preferred size.
    TestView* test_view = new TestView();
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
  SetForegroundWindow(window_->GetNativeWindow());

  // Flush any pending events to make sure we start with a clean slate.
  MessageLoop::current()->RunAllPending();

  // Schedule a task that starts the test. Need to do this as we're going to
  // run the message loop.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      NewRunnableMethod(this, &ViewEventTestBase::DoTestOnMessageLoop), 0);

  MessageLoop::current()->Run();
}

gfx::Size ViewEventTestBase::GetPreferredSize() {
  return gfx::Size();
}

void ViewEventTestBase::ScheduleMouseMoveInBackground(int x, int y) {
  if (!dnd_thread_.get()) {
    dnd_thread_.reset(new base::Thread("mouse-move-thread"));
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
