// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_glib.h"

#include <gtk/gtk.h>
#include <math.h>

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/ref_counted.h"
#include "base/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This class injects dummy "events" into the GLib loop. When "handled" these
// events can run tasks. This is intended to mock gtk events (the corresponding
// GLib source runs at the same priority).
class EventInjector {
 public:
  EventInjector() : processed_events_(0) {
    source_ = static_cast<Source*>(g_source_new(&SourceFuncs, sizeof(Source)));
    source_->injector = this;
    g_source_attach(source_, NULL);
    g_source_set_can_recurse(source_, TRUE);
  }

  ~EventInjector() {
    g_source_destroy(source_);
    g_source_unref(source_);
  }

  int HandlePrepare() {
    // If the queue is empty, block.
    if (events_.empty())
      return -1;
    base::TimeDelta delta = events_[0].time - base::Time::NowFromSystemTime();
    return std::max(0, static_cast<int>(ceil(delta.InMillisecondsF())));
  }

  bool HandleCheck() {
    if (events_.empty())
      return false;
    Event event = events_[0];
    return events_[0].time <= base::Time::NowFromSystemTime();
  }

  void HandleDispatch() {
    if (events_.empty())
      return;
    Event event = events_[0];
    events_.erase(events_.begin());
    ++processed_events_;
    if (event.task) {
      event.task->Run();
      delete event.task;
    }
  }

  // Adds an event to the queue. When "handled", executes |task|.
  // delay_ms is relative to the last event if any, or to Now() otherwise.
  void AddEvent(int delay_ms, Task* task) {
    base::Time last_time;
    if (!events_.empty()) {
      last_time = (events_.end()-1)->time;
    } else {
      last_time = base::Time::NowFromSystemTime();
    }
    base::Time future = last_time + base::TimeDelta::FromMilliseconds(delay_ms);
    EventInjector::Event event = { future, task };
    events_.push_back(event);
  }

  void Reset() {
    processed_events_ = 0;
    events_.clear();
  }

  int processed_events() const { return processed_events_; }

 private:
  struct Event {
    base::Time time;
    Task* task;
  };

  struct Source : public GSource {
    EventInjector* injector;
  };

  static gboolean Prepare(GSource* source, gint* timeout_ms) {
    *timeout_ms = static_cast<Source*>(source)->injector->HandlePrepare();
    return FALSE;
  }

  static gboolean Check(GSource* source) {
    return static_cast<Source*>(source)->injector->HandleCheck();
  }

  static gboolean Dispatch(GSource* source,
                           GSourceFunc unused_func,
                           gpointer unused_data) {
    static_cast<Source*>(source)->injector->HandleDispatch();
    return TRUE;
  }

  Source* source_;
  std::vector<Event> events_;
  int processed_events_;
  static GSourceFuncs SourceFuncs;
  DISALLOW_COPY_AND_ASSIGN(EventInjector);
};

GSourceFuncs EventInjector::SourceFuncs = {
  EventInjector::Prepare,
  EventInjector::Check,
  EventInjector::Dispatch,
  NULL
};

// Does nothing. This function can be called from a task.
void DoNothing() {
}

void IncrementInt(int *value) {
  ++*value;
}

// Checks how many events have been processed by the injector.
void ExpectProcessedEvents(EventInjector* injector, int count) {
  EXPECT_EQ(injector->processed_events(), count);
}

// Quits the current message loop.
void QuitMessageLoop() {
  MessageLoop::current()->Quit();
}

// Returns a new task that quits the main loop.
Task* NewQuitTask() {
  return NewRunnableFunction(QuitMessageLoop);
}

// Posts a task on the current message loop.
void PostMessageLoopTask(const tracked_objects::Location& from_here,
                         Task* task) {
  MessageLoop::current()->PostTask(from_here, task);
}

// Test fixture.
class MessagePumpGLibTest : public testing::Test {
 public:
  MessagePumpGLibTest() : loop_(NULL), injector_(NULL) { }

  virtual void SetUp() {
    loop_ = new MessageLoop(MessageLoop::TYPE_UI);
    injector_ = new EventInjector();
  }

  virtual void TearDown() {
    delete injector_;
    injector_ = NULL;
    delete loop_;
    loop_ = NULL;
  }

  MessageLoop* loop() const { return loop_; }
  EventInjector* injector() const { return injector_; }

 private:
  MessageLoop* loop_;
  EventInjector* injector_;
  DISALLOW_COPY_AND_ASSIGN(MessagePumpGLibTest);
};

}  // namespace

// EventInjector is expected to always live longer than the runnable methods.
// This lets us call NewRunnableMethod on EventInjector instances.
template<>
struct RunnableMethodTraits<EventInjector> {
  static void RetainCallee(EventInjector* obj) { }
  static void ReleaseCallee(EventInjector* obj) { }
};

TEST_F(MessagePumpGLibTest, TestQuit) {
  // Checks that Quit works and that the basic infrastructure is working.

  // Quit from a task
  loop()->PostTask(FROM_HERE, NewQuitTask());
  loop()->Run();
  EXPECT_EQ(0, injector()->processed_events());

  injector()->Reset();
  // Quit from an event
  injector()->AddEvent(0, NewQuitTask());
  loop()->Run();
  EXPECT_EQ(1, injector()->processed_events());
}

TEST_F(MessagePumpGLibTest, TestEventTaskInterleave) {
  // Checks that tasks posted by events are executed before the next event if
  // the posted task queue is empty.
  // MessageLoop doesn't make strong guarantees that it is the case, but the
  // current implementation ensures it and the tests below rely on it.
  // If changes cause this test to fail, it is reasonable to change it, but
  // TestWorkWhileWaitingForEvents and TestEventsWhileWaitingForWork have to be
  // changed accordingly, otherwise they can become flaky.
  injector()->AddEvent(0, NewRunnableFunction(DoNothing));
  Task* check_task = NewRunnableFunction(ExpectProcessedEvents, injector(), 2);
  Task* posted_task = NewRunnableFunction(PostMessageLoopTask,
                                          FROM_HERE, check_task);
  injector()->AddEvent(0, posted_task);
  injector()->AddEvent(0, NewRunnableFunction(DoNothing));
  injector()->AddEvent(0, NewQuitTask());
  loop()->Run();
  EXPECT_EQ(4, injector()->processed_events());

  injector()->Reset();
  injector()->AddEvent(0, NewRunnableFunction(DoNothing));
  check_task = NewRunnableFunction(ExpectProcessedEvents, injector(), 2);
  posted_task = NewRunnableFunction(PostMessageLoopTask, FROM_HERE, check_task);
  injector()->AddEvent(0, posted_task);
  injector()->AddEvent(10, NewRunnableFunction(DoNothing));
  injector()->AddEvent(0, NewQuitTask());
  loop()->Run();
  EXPECT_EQ(4, injector()->processed_events());
}

TEST_F(MessagePumpGLibTest, TestWorkWhileWaitingForEvents) {
  int task_count = 0;
  // Tests that we process tasks while waiting for new events.
  // The event queue is empty at first.
  for (int i = 0; i < 10; ++i) {
    loop()->PostTask(FROM_HERE, NewRunnableFunction(IncrementInt, &task_count));
  }
  // After all the previous tasks have executed, enqueue an event that will
  // quit.
  loop()->PostTask(
      FROM_HERE, NewRunnableMethod(injector(), &EventInjector::AddEvent,
                                   0, NewQuitTask()));
  loop()->Run();
  ASSERT_EQ(10, task_count);
  EXPECT_EQ(1, injector()->processed_events());

  // Tests that we process delayed tasks while waiting for new events.
  injector()->Reset();
  task_count = 0;
  for (int i = 0; i < 10; ++i) {
    loop()->PostDelayedTask(
        FROM_HERE, NewRunnableFunction(IncrementInt, &task_count), 10*i);
  }
  // After all the previous tasks have executed, enqueue an event that will
  // quit.
  // This relies on the fact that delayed tasks are executed in delay order.
  // That is verified in message_loop_unittest.cc.
  loop()->PostDelayedTask(
      FROM_HERE, NewRunnableMethod(injector(), &EventInjector::AddEvent,
                                   10, NewQuitTask()), 150);
  loop()->Run();
  ASSERT_EQ(10, task_count);
  EXPECT_EQ(1, injector()->processed_events());
}

TEST_F(MessagePumpGLibTest, TestEventsWhileWaitingForWork) {
  // Tests that we process events while waiting for work.
  // The event queue is empty at first.
  for (int i = 0; i < 10; ++i) {
    injector()->AddEvent(0, NULL);
  }
  // After all the events have been processed, post a task that will check that
  // the events have been processed (note: the task executes after the event
  // that posted it has been handled, so we expect 11 at that point).
  Task* check_task = NewRunnableFunction(ExpectProcessedEvents, injector(), 11);
  Task* posted_task = NewRunnableFunction(PostMessageLoopTask,
                                          FROM_HERE, check_task);
  injector()->AddEvent(10, posted_task);

  // And then quit (relies on the condition tested by TestEventTaskInterleave).
  injector()->AddEvent(10, NewQuitTask());
  loop()->Run();

  EXPECT_EQ(12, injector()->processed_events());
}

namespace {

// This class is a helper for the concurrent events / posted tasks test below.
// It will quit the main loop once enough tasks and events have been processed,
// while making sure there is always work to do and events in the queue.
class ConcurrentHelper : public base::RefCounted<ConcurrentHelper>  {
 public:
  ConcurrentHelper(EventInjector* injector)
      : injector_(injector),
        event_count_(kStartingEventCount),
        task_count_(kStartingTaskCount) {
  }

  void FromTask() {
    if (task_count_ > 0) {
      --task_count_;
    }
    if (task_count_ == 0 && event_count_ == 0) {
        MessageLoop::current()->Quit();
    } else {
      MessageLoop::current()->PostTask(
          FROM_HERE, NewRunnableMethod(this, &ConcurrentHelper::FromTask));
    }
  }

  void FromEvent() {
    if (event_count_ > 0) {
      --event_count_;
    }
    if (task_count_ == 0 && event_count_ == 0) {
        MessageLoop::current()->Quit();
    } else {
      injector_->AddEvent(
          0, NewRunnableMethod(this, &ConcurrentHelper::FromEvent));
    }
  }

  int event_count() const { return event_count_; }
  int task_count() const { return task_count_; }

 private:
  static const int kStartingEventCount = 20;
  static const int kStartingTaskCount = 20;

  EventInjector* injector_;
  int event_count_;
  int task_count_;
};

}  // namespace

TEST_F(MessagePumpGLibTest, TestConcurrentEventPostedTask) {
  // Tests that posted tasks don't starve events, nor the opposite.
  // We use the helper class above. We keep both event and posted task queues
  // full, the helper verifies that both tasks and events get processed.
  // If that is not the case, either event_count_ or task_count_ will not get
  // to 0, and MessageLoop::Quit() will never be called.
  scoped_refptr<ConcurrentHelper> helper = new ConcurrentHelper(injector());

  // Add 2 events to the queue to make sure it is always full (when we remove
  // the event before processing it).
  injector()->AddEvent(
      0, NewRunnableMethod(helper.get(), &ConcurrentHelper::FromEvent));
  injector()->AddEvent(
      0, NewRunnableMethod(helper.get(), &ConcurrentHelper::FromEvent));

  // Similarly post 2 tasks.
  loop()->PostTask(
      FROM_HERE, NewRunnableMethod(helper.get(), &ConcurrentHelper::FromTask));
  loop()->PostTask(
      FROM_HERE, NewRunnableMethod(helper.get(), &ConcurrentHelper::FromTask));

  loop()->Run();
  EXPECT_EQ(0, helper->event_count());
  EXPECT_EQ(0, helper->task_count());
}

namespace {

void AddEventsAndDrainGLib(EventInjector* injector) {
  // Add a couple of dummy events
  injector->AddEvent(0, NULL);
  injector->AddEvent(0, NULL);
  // Then add an event that will quit the main loop.
  injector->AddEvent(0, NewQuitTask());

  // Post a couple of dummy tasks
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableFunction(DoNothing));
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableFunction(DoNothing));

  // Drain the events
  while (g_main_context_pending(NULL)) {
    g_main_context_iteration(NULL, FALSE);
  }
}

}  // namespace

TEST_F(MessagePumpGLibTest, TestDrainingGLib) {
  // Tests that draining events using GLib works.
  loop()->PostTask(
      FROM_HERE, NewRunnableFunction(AddEventsAndDrainGLib, injector()));
  loop()->Run();

  EXPECT_EQ(3, injector()->processed_events());
}


namespace {

void AddEventsAndDrainGtk(EventInjector* injector) {
  // Add a couple of dummy events
  injector->AddEvent(0, NULL);
  injector->AddEvent(0, NULL);
  // Then add an event that will quit the main loop.
  injector->AddEvent(0, NewQuitTask());

  // Post a couple of dummy tasks
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableFunction(DoNothing));
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableFunction(DoNothing));

  // Drain the events
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }
}

}  // namespace

TEST_F(MessagePumpGLibTest, TestDrainingGtk) {
  // Tests that draining events using Gtk works.
  loop()->PostTask(
      FROM_HERE, NewRunnableFunction(AddEventsAndDrainGtk, injector()));
  loop()->Run();

  EXPECT_EQ(3, injector()->processed_events());
}

namespace {

// Helper class that lets us run the GLib message loop.
class GLibLoopRunner : public base::RefCounted<GLibLoopRunner> {
 public:
  GLibLoopRunner() : quit_(false) { }

  void RunGLib() {
    while (!quit_) {
      g_main_context_iteration(NULL, TRUE);
    }
  }

  void RunGtk() {
    while (!quit_) {
      gtk_main_iteration();
    }
  }

  void Quit() {
    quit_ = true;
  }

  void Reset() {
    quit_ = false;
  }

 private:
  bool quit_;
};

void TestGLibLoopInternal(EventInjector* injector) {
  // Allow tasks to be processed from 'native' event loops.
  MessageLoop::current()->SetNestableTasksAllowed(true);
  scoped_refptr<GLibLoopRunner> runner = new GLibLoopRunner();

  int task_count = 0;
  // Add a couple of dummy events
  injector->AddEvent(0, NULL);
  injector->AddEvent(0, NULL);
  // Post a couple of dummy tasks
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableFunction(IncrementInt, &task_count));
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableFunction(IncrementInt, &task_count));
  // Delayed events
  injector->AddEvent(10, NULL);
  injector->AddEvent(10, NULL);
  // Delayed work
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, NewRunnableFunction(IncrementInt, &task_count), 30);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, NewRunnableMethod(runner.get(), &GLibLoopRunner::Quit), 40);

  // Run a nested, straight GLib message loop.
  runner->RunGLib();

  ASSERT_EQ(3, task_count);
  EXPECT_EQ(4, injector->processed_events());
  MessageLoop::current()->Quit();
}

void TestGtkLoopInternal(EventInjector* injector) {
  // Allow tasks to be processed from 'native' event loops.
  MessageLoop::current()->SetNestableTasksAllowed(true);
  scoped_refptr<GLibLoopRunner> runner = new GLibLoopRunner();

  int task_count = 0;
  // Add a couple of dummy events
  injector->AddEvent(0, NULL);
  injector->AddEvent(0, NULL);
  // Post a couple of dummy tasks
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableFunction(IncrementInt, &task_count));
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableFunction(IncrementInt, &task_count));
  // Delayed events
  injector->AddEvent(10, NULL);
  injector->AddEvent(10, NULL);
  // Delayed work
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, NewRunnableFunction(IncrementInt, &task_count), 30);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, NewRunnableMethod(runner.get(), &GLibLoopRunner::Quit), 40);

  // Run a nested, straight Gtk message loop.
  runner->RunGtk();

  ASSERT_EQ(3, task_count);
  EXPECT_EQ(4, injector->processed_events());
  MessageLoop::current()->Quit();
}

}  // namespace

TEST_F(MessagePumpGLibTest, TestGLibLoop) {
  // Tests that events and posted tasks are correctly exectuted if the message
  // loop is not run by MessageLoop::Run() but by a straight GLib loop.
  // Note that in this case we don't make strong guarantees about niceness
  // between events and posted tasks.
  loop()->PostTask(FROM_HERE,
                   NewRunnableFunction(TestGLibLoopInternal, injector()));
  loop()->Run();
}

TEST_F(MessagePumpGLibTest, TestGtkLoop) {
  // Tests that events and posted tasks are correctly exectuted if the message
  // loop is not run by MessageLoop::Run() but by a straight Gtk loop.
  // Note that in this case we don't make strong guarantees about niceness
  // between events and posted tasks.
  loop()->PostTask(FROM_HERE,
                   NewRunnableFunction(TestGtkLoopInternal, injector()));
  loop()->Run();
}
