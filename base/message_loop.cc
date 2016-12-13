// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_pump_default.h"
#include "base/string_util.h"
#include "base/thread_local.h"

#if defined(OS_MACOSX)
#include "base/message_pump_mac.h"
#endif
#if defined(OS_POSIX)
#include "base/message_pump_libevent.h"
#include "base/third_party/valgrind/valgrind.h"
#endif
#if defined(OS_LINUX)
#include "base/message_pump_glib.h"
#endif

using base::Time;
using base::TimeDelta;

// A lazily created thread local storage for quick access to a thread's message
// loop, if one exists.  This should be safe and free of static constructors.
static base::LazyInstance<base::ThreadLocalPointer<MessageLoop> > lazy_tls_ptr(
    base::LINKER_INITIALIZED);

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

// static
MessageLoop* MessageLoop::current() {
  // TODO(darin): sadly, we cannot enable this yet since people call us even
  // when they have no intention of using us.
  //DCHECK(loop) << "Ouch, did you forget to initialize me?";
  return lazy_tls_ptr.Pointer()->Get();
}

MessageLoop::MessageLoop(Type type)
    : type_(type),
      nestable_tasks_allowed_(true),
      exception_restoration_(false),
      state_(NULL),
      next_sequence_num_(0) {
  DCHECK(!current()) << "should only have one message loop per thread";
  lazy_tls_ptr.Pointer()->Set(this);

#if defined(OS_WIN)
  // TODO(rvargas): Get rid of the OS guards.
  if (type_ == TYPE_DEFAULT) {
    pump_ = new base::MessagePumpDefault();
  } else if (type_ == TYPE_IO) {
    pump_ = new base::MessagePumpForIO();
  } else {
    DCHECK(type_ == TYPE_UI);
    pump_ = new base::MessagePumpForUI();
  }
#elif defined(OS_POSIX)
  if (type_ == TYPE_UI) {
#if defined(OS_MACOSX)
    pump_ = base::MessagePumpMac::Create();
#elif defined(OS_LINUX)
    pump_ = new base::MessagePumpForUI();
#endif  // OS_LINUX
  } else if (type_ == TYPE_IO) {
    pump_ = new base::MessagePumpLibevent();
  } else {
    pump_ = new base::MessagePumpDefault();
  }
#endif  // OS_POSIX
}

MessageLoop::~MessageLoop() {
  DCHECK(this == current());

  // Let interested parties have one last shot at accessing this.
  FOR_EACH_OBSERVER(DestructionObserver, destruction_observers_,
                    WillDestroyCurrentMessageLoop());

  DCHECK(!state_);

  // Clean up any unprocessed tasks, but take care: deleting a task could
  // result in the addition of more tasks (e.g., via DeleteSoon).  We set a
  // limit on the number of times we will allow a deleted task to generate more
  // tasks.  Normally, we should only pass through this loop once or twice.  If
  // we end up hitting the loop limit, then it is probably due to one task that
  // is being stubborn.  Inspect the queues to see who is left.
  bool did_work;
  for (int i = 0; i < 100; ++i) {
    DeletePendingTasks();
    ReloadWorkQueue();
    // If we end up with empty queues, then break out of the loop.
    did_work = DeletePendingTasks();
    if (!did_work)
      break;
  }
  DCHECK(!did_work);

  // OK, now make it so that no one can find us.
  lazy_tls_ptr.Pointer()->Set(NULL);
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
    const tracked_objects::Location& from_here, Task* task, int64 delay_ms) {
  PostTask_Helper(from_here, task, delay_ms, true);
}

void MessageLoop::PostNonNestableTask(
    const tracked_objects::Location& from_here, Task* task) {
  PostTask_Helper(from_here, task, 0, false);
}

void MessageLoop::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here, Task* task, int64 delay_ms) {
  PostTask_Helper(from_here, task, delay_ms, false);
}

// Possibly called on a background thread!
void MessageLoop::PostTask_Helper(
    const tracked_objects::Location& from_here, Task* task, int64 delay_ms,
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

bool MessageLoop::IsNested() {
  return state_->run_depth > 1;
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

void MessageLoop::AddToDelayedWorkQueue(const PendingTask& pending_task) {
  // Move to the delayed work queue.  Initialize the sequence number
  // before inserting into the delayed_work_queue_.  The sequence number
  // is used to faciliate FIFO sorting when two tasks have the same
  // delayed_run_time value.
  PendingTask new_pending_task(pending_task);
  new_pending_task.sequence_num = next_sequence_num_++;
  delayed_work_queue_.push(new_pending_task);
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

bool MessageLoop::DeletePendingTasks() {
  bool did_work = !work_queue_.empty();
  while (!work_queue_.empty()) {
    PendingTask pending_task = work_queue_.front();
    work_queue_.pop();
    if (!pending_task.delayed_run_time.is_null()) {
      // We want to delete delayed tasks in the same order in which they would
      // normally be deleted in case of any funny dependencies between delayed
      // tasks.
      AddToDelayedWorkQueue(pending_task);
    } else {
      // TODO(darin): Delete all tasks once it is safe to do so.
      // Until it is totally safe, just do it when running Purify or
      // Valgrind.
#if defined(OS_WIN)
#ifdef PURIFY
      delete pending_task.task;
#endif  // PURIFY
#elif defined(OS_POSIX)
      if (RUNNING_ON_VALGRIND)
        delete pending_task.task;
#endif  // defined(OS_POSIX)
    }
  }
  did_work |= !deferred_non_nestable_work_queue_.empty();
  while (!deferred_non_nestable_work_queue_.empty()) {
    // TODO(darin): Delete all tasks once it is safe to do so.
    // Until it is totaly safe, just delete them to keep purify happy.
#ifdef PURIFY
    Task* task = deferred_non_nestable_work_queue_.front().task;
#endif
    deferred_non_nestable_work_queue_.pop();
#ifdef PURIFY
    delete task;
#endif
  }
  did_work |= !delayed_work_queue_.empty();
  while (!delayed_work_queue_.empty()) {
    Task* task = delayed_work_queue_.top().task;
    delayed_work_queue_.pop();
    delete task;
  }
  return did_work;
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
        AddToDelayedWorkQueue(pending_task);
        // If we changed the topmost task, then it is time to re-schedule.
        if (delayed_work_queue_.top().task == pending_task.task)
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
    message_histogram_.reset(
        new LinearHistogram(("MsgLoop:" + thread_name_).c_str(),
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

#if defined(OS_LINUX) || defined(OS_WIN)

void MessageLoopForUI::AddObserver(Observer* observer) {
  pump_ui()->AddObserver(observer);
}

void MessageLoopForUI::RemoveObserver(Observer* observer) {
  pump_ui()->RemoveObserver(observer);
}

#endif

#if defined(OS_WIN)

void MessageLoopForUI::Run(Dispatcher* dispatcher) {
  AutoRunState save_state(this);
  state_->dispatcher = dispatcher;
  RunHandler();
}

void MessageLoopForUI::WillProcessMessage(const MSG& message) {
  pump_win()->WillProcessMessage(message);
}
void MessageLoopForUI::DidProcessMessage(const MSG& message) {
  pump_win()->DidProcessMessage(message);
}
void MessageLoopForUI::PumpOutPendingPaintMessages() {
  pump_ui()->PumpOutPendingPaintMessages();
}

#endif  // defined(OS_WIN)

//------------------------------------------------------------------------------
// MessageLoopForIO

#if defined(OS_WIN)

void MessageLoopForIO::RegisterIOHandler(HANDLE file, IOHandler* handler) {
  pump_io()->RegisterIOHandler(file, handler);
}

bool MessageLoopForIO::WaitForIOCompletion(DWORD timeout, IOHandler* filter) {
  return pump_io()->WaitForIOCompletion(timeout, filter);
}

#elif defined(OS_POSIX)

bool MessageLoopForIO::WatchFileDescriptor(int fd,
                                           bool persistent,
                                           Mode mode,
                                           FileDescriptorWatcher *controller,
                                           Watcher *delegate) {
  return pump_libevent()->WatchFileDescriptor(
      fd,
      persistent,
      static_cast<base::MessagePumpLibevent::Mode>(mode),
      controller,
      delegate);
}

#endif
