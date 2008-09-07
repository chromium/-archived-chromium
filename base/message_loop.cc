// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"

#include <algorithm>

#if defined(OS_WIN)
#include <mmsystem.h>
#endif

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_pump_default.h"
#include "base/string_util.h"
#include "base/thread_local_storage.h"

// a TLS index to the message loop for the current thread
// Note that if we start doing complex stuff in other static initializers
// this could cause problems.
// TODO(evanm): this shouldn't rely on static initialization.
// static
TLSSlot MessageLoop::tls_index_;

//------------------------------------------------------------------------------

// Logical events for Histogram profiling. Run with -message-loop-histogrammer
// to get an accounting of messages and actions taken on each thread.
static const int kTaskRunEvent = 0x1;
static const int kTimerEvent = 0x2;

// Provide range of message IDs for use in histogramming and debug display.
static const int kLeastNonZeroMessageId = 1;
static const int kMaxMessageId = 1099;
static const int kNumberOfDistinctMessagesDisplayed = 1100;

//------------------------------------------------------------------------------

#if defined(OS_WIN)

// Upon a SEH exception in this thread, it restores the original unhandled
// exception filter.
static int SEHFilter(LPTOP_LEVEL_EXCEPTION_FILTER old_filter) {
  ::SetUnhandledExceptionFilter(old_filter);
  return EXCEPTION_CONTINUE_SEARCH;
}

// Retrieves a pointer to the current unhandled exception filter. There
// is no standalone getter method.
static LPTOP_LEVEL_EXCEPTION_FILTER GetTopSEHFilter() {
  LPTOP_LEVEL_EXCEPTION_FILTER top_filter = NULL;
  top_filter = ::SetUnhandledExceptionFilter(0);
  ::SetUnhandledExceptionFilter(top_filter);
  return top_filter;
}

#endif  // defined(OS_WIN)

//------------------------------------------------------------------------------

MessageLoop::MessageLoop(Type type)
    : type_(type),
      nestable_tasks_allowed_(true),
      exception_restoration_(false),
      state_(NULL),
      next_sequence_num_(0) {
  DCHECK(!tls_index_.Get()) << "should only have one message loop per thread";
  tls_index_.Set(this);

  // TODO(darin): This does not seem like the best place for this code to live!
#if defined(OS_WIN)
  // We've experimented with all sorts of timers, and initially tried
  // to avoid using timeBeginPeriod because it does affect the system
  // globally.  However, after much investigation, it turns out that all
  // of the major plugins (flash, windows media 9-11, and quicktime)
  // already use timeBeginPeriod to increase the speed of the clock.
  // Since the browser must work with these plugins, the browser already
  // needs to support a fast clock.  We may as well use this ourselves,
  // as it really is the best timer mechanism for our needs.
  timeBeginPeriod(1);
#endif

  // TODO(darin): Choose the pump based on the requested type.
#if defined(OS_WIN)
  if (type_ == TYPE_DEFAULT) {
    pump_ = new base::MessagePumpDefault();
  } else {
    pump_ = new base::MessagePumpWin();
  }
#else
  pump_ = new base::MessagePumpDefault();
#endif
}

MessageLoop::~MessageLoop() {
  DCHECK(this == current());

  // Let interested parties have one last shot at accessing this.
  FOR_EACH_OBSERVER(DestructionObserver, destruction_observers_,
                    WillDestroyCurrentMessageLoop());

  // OK, now make it so that no one can find us.
  tls_index_.Set(NULL);

  DCHECK(!state_);

  // Most tasks that have not been Run() are deleted in the |timer_manager_|
  // destructor after we remove our tls index.  We delete the tasks in our
  // queues here so their destuction is similar to the tasks in the
  // |timer_manager_|.
  DeletePendingTasks();
  ReloadWorkQueue();
  DeletePendingTasks();

#if defined(OS_WIN)
  // Match timeBeginPeriod() from construction.
  timeEndPeriod(1);
#endif
}

void MessageLoop::AddDestructionObserver(DestructionObserver *obs) {
  DCHECK(this == current());
  destruction_observers_.AddObserver(obs);
}

void MessageLoop::RemoveDestructionObserver(DestructionObserver *obs) {
  DCHECK(this == current());
  destruction_observers_.RemoveObserver(obs);
}

void MessageLoop::Run() {
  AutoRunState save_state(this);
  RunHandler();
}

void MessageLoop::RunAllPending() {
  AutoRunState save_state(this);
  state_->quit_received = true;  // Means run until we would otherwise block.
  RunHandler();
}

// Runs the loop in two different SEH modes:
// enable_SEH_restoration_ = false : any unhandled exception goes to the last
// one that calls SetUnhandledExceptionFilter().
// enable_SEH_restoration_ = true : any unhandled exception goes to the filter
// that was existed before the loop was run.
void MessageLoop::RunHandler() {
#if defined(OS_WIN)
  if (exception_restoration_) {
    LPTOP_LEVEL_EXCEPTION_FILTER current_filter = GetTopSEHFilter();
    __try {
      RunInternal();
    } __except(SEHFilter(current_filter)) {
    }
    return;
  }
#endif

  RunInternal();
}

//------------------------------------------------------------------------------

void MessageLoop::RunInternal() {
  DCHECK(this == current());

  StartHistogrammer();

#if defined(OS_WIN)
  if (state_->dispatcher) {
    pump_win()->RunWithDispatcher(this, state_->dispatcher);
    return;
  }
#endif
  
  pump_->Run(this);
}

//------------------------------------------------------------------------------
// Wrapper functions for use in above message loop framework.

bool MessageLoop::ProcessNextDelayedNonNestableTask() {
  if (state_->run_depth != 1)
    return false;

  if (deferred_non_nestable_work_queue_.empty())
    return false;
  
  Task* task = deferred_non_nestable_work_queue_.front().task;
  deferred_non_nestable_work_queue_.pop();
  
  RunTask(task);
  return true;
}

//------------------------------------------------------------------------------

void MessageLoop::Quit() {
  DCHECK(current() == this);
  if (state_) {
    state_->quit_received = true;
  } else {
    NOTREACHED() << "Must be inside Run to call Quit";
  }
}

void MessageLoop::PostTask(
    const tracked_objects::Location& from_here, Task* task) {
  PostTask_Helper(from_here, task, 0, true);
}

void MessageLoop::PostDelayedTask(
    const tracked_objects::Location& from_here, Task* task, int delay_ms) {
  PostTask_Helper(from_here, task, delay_ms, true);
}

void MessageLoop::PostNonNestableTask(
    const tracked_objects::Location& from_here, Task* task) {
  PostTask_Helper(from_here, task, 0, false);
}

void MessageLoop::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here, Task* task, int delay_ms) {
  PostTask_Helper(from_here, task, delay_ms, false);
}

// Possibly called on a background thread!
void MessageLoop::PostTask_Helper(
    const tracked_objects::Location& from_here, Task* task, int delay_ms,
    bool nestable) {
  task->SetBirthPlace(from_here);

  PendingTask pending_task(task, nestable);

  if (delay_ms > 0) {
    pending_task.delayed_run_time =
        Time::Now() + TimeDelta::FromMilliseconds(delay_ms);
  } else {
    DCHECK(delay_ms == 0) << "delay should not be negative";
  }

  // Warning: Don't try to short-circuit, and handle this thread's tasks more
  // directly, as it could starve handling of foreign threads.  Put every task
  // into this queue.

  scoped_refptr<base::MessagePump> pump;
  {
    AutoLock locked(incoming_queue_lock_);

    bool was_empty = incoming_queue_.empty();
    incoming_queue_.push(pending_task);
    if (!was_empty)
      return;  // Someone else should have started the sub-pump.

    pump = pump_;
  }
  // Since the incoming_queue_ may contain a task that destroys this message
  // loop, we cannot exit incoming_queue_lock_ until we are done with |this|.
  // We use a stack-based reference to the message pump so that we can call
  // ScheduleWork outside of incoming_queue_lock_.

  pump->ScheduleWork();
}

void MessageLoop::SetNestableTasksAllowed(bool allowed) {
  if (nestable_tasks_allowed_ != allowed) {
    nestable_tasks_allowed_ = allowed;
    if (!nestable_tasks_allowed_)
      return;
    // Start the native pump if we are not already pumping.
    pump_->ScheduleWork();
  }
}

bool MessageLoop::NestableTasksAllowed() const {
  return nestable_tasks_allowed_;
}

//------------------------------------------------------------------------------

void MessageLoop::RunTask(Task* task) {
  DCHECK(nestable_tasks_allowed_);
  // Execute the task and assume the worst: It is probably not reentrant.
  nestable_tasks_allowed_ = false;

  HistogramEvent(kTaskRunEvent);
  task->Run();
  delete task;

  nestable_tasks_allowed_ = true;
}

bool MessageLoop::DeferOrRunPendingTask(const PendingTask& pending_task) {
  if (pending_task.nestable || state_->run_depth == 1) {
    RunTask(pending_task.task);
    // Show that we ran a task (Note: a new one might arrive as a
    // consequence!).
    return true;
  }

  // We couldn't run the task now because we're in a nested message loop
  // and the task isn't nestable.
  deferred_non_nestable_work_queue_.push(pending_task);
  return false;
}

void MessageLoop::ReloadWorkQueue() {
  // We can improve performance of our loading tasks from incoming_queue_ to
  // work_queue_ by waiting until the last minute (work_queue_ is empty) to
  // load.  That reduces the number of locks-per-task significantly when our
  // queues get large.
  if (!work_queue_.empty())
    return;  // Wait till we *really* need to lock and load.

  // Acquire all we can from the inter-thread queue with one lock acquisition.
  {
    AutoLock lock(incoming_queue_lock_);
    if (incoming_queue_.empty())
      return;
    std::swap(incoming_queue_, work_queue_);
    DCHECK(incoming_queue_.empty());
  }
}

void MessageLoop::DeletePendingTasks() {
  /* Comment this out as it's causing crashes.
  while (!work_queue_.Empty()) {
    Task* task = work_queue_.Pop();
    if (task->is_owned_by_message_loop())
      delete task;
  }

  while (!delayed_non_nestable_queue_.Empty()) {
    Task* task = delayed_non_nestable_queue_.Pop();
    if (task->is_owned_by_message_loop())
      delete task;
  }
  */
}

bool MessageLoop::DoWork() {
  if (!nestable_tasks_allowed_) {
    // Task can't be executed right now.
    return false;
  }

  for (;;) {
    ReloadWorkQueue();
    if (work_queue_.empty())
      break;

    // Execute oldest task.
    do {
      PendingTask pending_task = work_queue_.front();
      work_queue_.pop();
      if (!pending_task.delayed_run_time.is_null()) {
        bool was_empty = delayed_work_queue_.empty();

        // Move to the delayed work queue.  Initialize the sequence number
        // before inserting into the delayed_work_queue_.  The sequence number
        // is used to faciliate FIFO sorting when two tasks have the same
        // delayed_run_time value.
        pending_task.sequence_num = next_sequence_num_++;
        delayed_work_queue_.push(pending_task);

        if (was_empty)  // We only schedule the next delayed work item.
          pump_->ScheduleDelayedWork(pending_task.delayed_run_time);
      } else {
        if (DeferOrRunPendingTask(pending_task))
          return true;
      }
    } while (!work_queue_.empty());
  }

  // Nothing happened.
  return false;
}

bool MessageLoop::DoDelayedWork(Time* next_delayed_work_time) {
  if (!nestable_tasks_allowed_ || delayed_work_queue_.empty()) {
    *next_delayed_work_time = Time();
    return false;
  }
  
  if (delayed_work_queue_.top().delayed_run_time > Time::Now()) {
    *next_delayed_work_time = delayed_work_queue_.top().delayed_run_time;
    return false;
  }

  PendingTask pending_task = delayed_work_queue_.top();
  delayed_work_queue_.pop();
  
  if (!delayed_work_queue_.empty())
    *next_delayed_work_time = delayed_work_queue_.top().delayed_run_time;

  return DeferOrRunPendingTask(pending_task);
}

bool MessageLoop::DoIdleWork() {
  if (ProcessNextDelayedNonNestableTask())
    return true;

  if (state_->quit_received)
    pump_->Quit();

  return false;
}

//------------------------------------------------------------------------------
// MessageLoop::AutoRunState

MessageLoop::AutoRunState::AutoRunState(MessageLoop* loop) : loop_(loop) {
  // Make the loop reference us.
  previous_state_ = loop_->state_;
  if (previous_state_) {
    run_depth = previous_state_->run_depth + 1;
  } else {
    run_depth = 1;
  }
  loop_->state_ = this;

  // Initialize the other fields:
  quit_received = false;
#if defined(OS_WIN)
  dispatcher = NULL;
#endif
}

MessageLoop::AutoRunState::~AutoRunState() {
  loop_->state_ = previous_state_;
}

//------------------------------------------------------------------------------
// MessageLoop::PendingTask

bool MessageLoop::PendingTask::operator<(const PendingTask& other) const {
  // Since the top of a priority queue is defined as the "greatest" element, we
  // need to invert the comparison here.  We want the smaller time to be at the
  // top of the heap.

  if (delayed_run_time < other.delayed_run_time)
    return false;

  if (delayed_run_time > other.delayed_run_time)
    return true;

  // If the times happen to match, then we use the sequence number to decide.
  // Compare the difference to support integer roll-over.
  return (sequence_num - other.sequence_num) > 0;
}

//------------------------------------------------------------------------------
// Method and data for histogramming events and actions taken by each instance
// on each thread.

// static
bool MessageLoop::enable_histogrammer_ = false;

// static
void MessageLoop::EnableHistogrammer(bool enable) {
  enable_histogrammer_ = enable;
}

void MessageLoop::StartHistogrammer() {
  if (enable_histogrammer_ && !message_histogram_.get()
      && StatisticsRecorder::WasStarted()) {
    DCHECK(!thread_name_.empty());
    message_histogram_.reset(new LinearHistogram(
        ASCIIToWide("MsgLoop:" + thread_name_).c_str(),
                    kLeastNonZeroMessageId,
                    kMaxMessageId,
                    kNumberOfDistinctMessagesDisplayed));
    message_histogram_->SetFlags(message_histogram_->kHexRangePrintingFlag);
    message_histogram_->SetRangeDescriptions(event_descriptions_);
  }
}

void MessageLoop::HistogramEvent(int event) {
  if (message_histogram_.get())
    message_histogram_->Add(event);
}

// Provide a macro that takes an expression (such as a constant, or macro
// constant) and creates a pair to initalize an array of pairs.  In this case,
// our pair consists of the expressions value, and the "stringized" version
// of the expression (i.e., the exrpression put in quotes).  For example, if
// we have:
//    #define FOO 2
//    #define BAR 5
// then the following:
//    VALUE_TO_NUMBER_AND_NAME(FOO + BAR)
// will expand to:
//   {7, "FOO + BAR"}
// We use the resulting array as an argument to our histogram, which reads the
// number as a bucket identifier, and proceeds to use the corresponding name
// in the pair (i.e., the quoted string) when printing out a histogram.
#define VALUE_TO_NUMBER_AND_NAME(name) {name, #name},

// static
const LinearHistogram::DescriptionPair MessageLoop::event_descriptions_[] = {
  // Provide some pretty print capability in our histogram for our internal
  // messages.

  // A few events we handle (kindred to messages), and used to profile actions.
  VALUE_TO_NUMBER_AND_NAME(kTaskRunEvent)
  VALUE_TO_NUMBER_AND_NAME(kTimerEvent)

  {-1, NULL}  // The list must be null terminated, per API to histogram.
};

//------------------------------------------------------------------------------
// MessageLoopForUI

#if defined(OS_WIN)

void MessageLoopForUI::Run(Dispatcher* dispatcher) {
  AutoRunState save_state(this);
  state_->dispatcher = dispatcher;
  RunHandler();
}

void MessageLoopForUI::AddObserver(Observer* observer) {
  pump_win()->AddObserver(observer);
}

void MessageLoopForUI::RemoveObserver(Observer* observer) {
  pump_win()->RemoveObserver(observer);
}

void MessageLoopForUI::WillProcessMessage(const MSG& message) {
  pump_win()->WillProcessMessage(message);
}
void MessageLoopForUI::DidProcessMessage(const MSG& message) {
  pump_win()->DidProcessMessage(message);
}
void MessageLoopForUI::PumpOutPendingPaintMessages() {
  pump_win()->PumpOutPendingPaintMessages();
}

#endif  // defined(OS_WIN)

//------------------------------------------------------------------------------
// MessageLoopForIO

#if defined(OS_WIN)

void MessageLoopForIO::WatchObject(HANDLE object, Watcher* watcher) {
  pump_win()->WatchObject(object, watcher);
}

#endif  // defined(OS_WIN)
