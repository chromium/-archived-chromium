// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"

#include <algorithm>

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
      ALLOW_THIS_IN_INITIALIZER_LIST(timer_manager_(this)),
      nestable_tasks_allowed_(true),
      exception_restoration_(false),
      state_(NULL) {
  DCHECK(!tls_index_.Get()) << "should only have one message loop per thread";
  tls_index_.Set(this);

  // TODO(darin): Choose the pump based on the requested type.
#if defined(OS_WIN)
  pump_ = new base::MessagePumpWin();
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

  if (delayed_non_nestable_queue_.Empty())
    return false;

  RunTask(delayed_non_nestable_queue_.Pop());
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

// Possibly called on a background thread!
void MessageLoop::PostDelayedTask(const tracked_objects::Location& from_here,
                                  Task* task, int delay_ms) {
  task->SetBirthPlace(from_here);
  DCHECK(delay_ms >= 0);
  DCHECK(!task->is_owned_by_message_loop());
  task->set_posted_task_delay(delay_ms);
  DCHECK(task->is_owned_by_message_loop());
  PostTaskInternal(task);
}

void MessageLoop::PostTaskInternal(Task* task) {
  // Warning: Don't try to short-circuit, and handle this thread's tasks more
  // directly, as it could starve handling of foreign threads.  Put every task
  // into this queue.

  scoped_refptr<base::MessagePump> pump;
  {
    AutoLock locked(incoming_queue_lock_);

    bool was_empty = incoming_queue_.Empty();
    incoming_queue_.Push(task);
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

bool MessageLoop::RunTimerTask(Timer* timer) {
  HistogramEvent(kTimerEvent);

  Task* task = timer->task();
  if (task->is_owned_by_message_loop()) {
    // We constructed it through PostDelayedTask().
    DCHECK(!timer->repeating());
    timer->set_task(NULL);
    delete timer;
    task->ResetBirthTime();
    return QueueOrRunTask(task);
  }

  // This is an unknown timer task, and we *can't* delay running it, as a user
  // might try to cancel it with TimerManager at any moment.
  DCHECK(nestable_tasks_allowed_);
  RunTask(task);
  return true;
}

void MessageLoop::DiscardTimer(Timer* timer) {
  Task* task = timer->task();
  if (task->is_owned_by_message_loop()) {
    DCHECK(!timer->repeating());
    timer->set_task(NULL);
    delete timer;  // We constructed it through PostDelayedTask().
    delete task;  // We were given ouwnership in PostTask().
  }
}

bool MessageLoop::QueueOrRunTask(Task* new_task) {
  if (!nestable_tasks_allowed_) {
    // Task can't be executed right now. Add it to the queue.
    if (new_task)
      work_queue_.Push(new_task);
    return false;
  }

  // Queue new_task first so we execute the task in FIFO order.
  if (new_task)
    work_queue_.Push(new_task);

  // Execute oldest task.
  while (!work_queue_.Empty()) {
    Task* task = work_queue_.Pop();
    if (task->nestable() || state_->run_depth == 1) {
      RunTask(task);
      // Show that we ran a task (Note: a new one might arrive as a
      // consequence!).
      return true;
    }
    // We couldn't run the task now because we're in a nested message loop
    // and the task isn't nestable.
    delayed_non_nestable_queue_.Push(task);
  }

  // Nothing happened.
  return false;
}

void MessageLoop::RunTask(Task* task) {
  BeforeTaskRunSetup();
  HistogramEvent(kTaskRunEvent);
  // task may self-delete during Run() if we don't happen to own it.
  // ...so check *before* we Run, since we can't check after.
  bool we_own_task = task->is_owned_by_message_loop();
  task->Run();
  if (we_own_task)
    task->RecycleOrDelete();  // Relinquish control, and probably delete.
  AfterTaskRunRestore();
}

void MessageLoop::BeforeTaskRunSetup() {
  DCHECK(nestable_tasks_allowed_);
  // Execute the task and assume the worst: It is probably not reentrant.
  nestable_tasks_allowed_ = false;
}

void MessageLoop::AfterTaskRunRestore() {
  nestable_tasks_allowed_ = true;
}

void MessageLoop::ReloadWorkQueue() {
  // We can improve performance of our loading tasks from incoming_queue_ to
  // work_queue_ by waiting until the last minute (work_queue_ is empty) to
  // load.  That reduces the number of locks-per-task significantly when our
  // queues get large.  The optimization is disabled on threads that make use
  // of the priority queue (prioritization requires all our tasks to be in the
  // work_queue_ ASAP).
  if (!work_queue_.Empty() && !work_queue_.use_priority_queue())
    return;  // Wait till we *really* need to lock and load.

  // Acquire all we can from the inter-thread queue with one lock acquisition.
  TaskQueue new_task_list;  // Null terminated list.
  {
    AutoLock lock(incoming_queue_lock_);
    if (incoming_queue_.Empty())
      return;
    std::swap(incoming_queue_, new_task_list);
    DCHECK(incoming_queue_.Empty());
  }  // Release lock.

  while (!new_task_list.Empty()) {
    Task* task = new_task_list.Pop();
    DCHECK(task->is_owned_by_message_loop());

    if (task->posted_task_delay() > 0)
      timer_manager_.StartTimer(task->posted_task_delay(), task, false);
    else
      work_queue_.Push(task);
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

void MessageLoop::DidChangeNextTimerExpiry() {
  Time next_delayed_work_time = timer_manager_.GetNextFireTime(); 
  if (next_delayed_work_time.is_null())
    return;

  // Simulates malfunctioning, early firing timers. Pending tasks should only
  // be invoked when the delay they specify has elapsed.
  if (timer_manager_.use_broken_delay())
    next_delayed_work_time = Time::Now() + TimeDelta::FromMilliseconds(10);

  pump_->ScheduleDelayedWork(next_delayed_work_time);
}

bool MessageLoop::DoWork() {
  ReloadWorkQueue();
  return QueueOrRunTask(NULL);
}

bool MessageLoop::DoDelayedWork(Time* next_delayed_work_time) {
  bool did_work = timer_manager_.RunSomePendingTimers();

  // We may not have run any timers, but we may still have future timers to
  // run, so we need to inform the pump again of pending timers.
  *next_delayed_work_time = timer_manager_.GetNextFireTime();

  return did_work;
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
// Implementation of the work_queue_ as a ProiritizedTaskQueue

void MessageLoop::PrioritizedTaskQueue::push(Task * task) {
  queue_.push(PrioritizedTask(task, --next_sequence_number_));
}

bool MessageLoop::PrioritizedTaskQueue::PrioritizedTask::operator < (
    PrioritizedTask const & right) const {
  int compare = task_->priority() - right.task_->priority();
  if (compare)
    return compare < 0;
  // Don't compare directly, but rather subtract.  This handles overflow
  // as sequence numbers wrap around.
  compare = sequence_number_ - right.sequence_number_;
  DCHECK(compare);  // Sequence number are unique for a "long time."
  // Make sure we don't starve anything with a low priority.
  CHECK(INT_MAX/8 > compare);  // We don't get close to wrapping.
  CHECK(INT_MIN/8 < compare);  // We don't get close to wrapping.
  return compare < 0;
}

//------------------------------------------------------------------------------
// Implementation of a TaskQueue as a null terminated list, with end pointers.

void MessageLoop::TaskQueue::Push(Task* task) {
  if (!first_)
    first_ = task;
  else
    last_->set_next_task(task);
  last_ = task;
}

Task* MessageLoop::TaskQueue::Pop() {
  DCHECK((!first_) == !last_);
  Task* task = first_;
  if (first_) {
    first_ = task->next_task();
    if (!first_)
      last_ = NULL;
    else
      task->set_next_task(NULL);
  }
  return task;
}

//------------------------------------------------------------------------------
// Implementation of a Task queue that automatically switches into a priority
// queue if it observes any non-zero priorities on tasks.

void MessageLoop::OptionallyPrioritizedTaskQueue::Push(Task* task) {
  if (use_priority_queue_) {
    prioritized_queue_.push(task);
  } else {
    queue_.Push(task);
    if (task->priority()) {
      use_priority_queue_ = true;  // From now on.
      while (!queue_.Empty())
        prioritized_queue_.push(queue_.Pop());
    }
  }
}

Task* MessageLoop::OptionallyPrioritizedTaskQueue::Pop() {
  if (!use_priority_queue_)
    return queue_.Pop();
  Task* task = prioritized_queue_.front();
  prioritized_queue_.pop();
  return task;
}

bool MessageLoop::OptionallyPrioritizedTaskQueue::Empty() {
  if (use_priority_queue_)
    return prioritized_queue_.empty();
  return queue_.Empty();
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
