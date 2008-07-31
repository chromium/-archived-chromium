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

#include <algorithm>

#include "base/message_loop.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/thread_local_storage.h"
#include "base/win_util.h"

// a TLS index to the message loop for the current thread
// Note that if we start doing complex stuff in other static initializers
// this could cause problems.
/*static*/ TLSSlot MessageLoop::tls_index_ = ThreadLocalStorage::Alloc();

//------------------------------------------------------------------------------

static const wchar_t kWndClass[] = L"Chrome_MessageLoopWindow";

// Windows Message numbers handled by WindowMessageProc.

// Message sent to get an additional time slice for pumping (processing) another
// task (a series of such messages creates a continuous task pump).
static const int kMsgPumpATask =        WM_USER + 1;

// Message sent by Quit() to cause our main message pump to terminate as soon as
// all pending task and message queues have been emptied.
static const int kMsgQuit =             WM_USER + 2;

// Logical events for Histogram profiling. Run with -message-loop-histogrammer
// to get an accounting of messages and actions taken on each thread.
static const int kTaskRunEvent =        WM_USER + 16;  // 0x411
static const int kSleepingApcEvent =    WM_USER + 17;  // 0x411
static const int kPollingSignalEvent =  WM_USER + 18;  // 0x412
static const int kSleepingSignalEvent = WM_USER + 19;  // 0x413
static const int kTimerEvent =          WM_USER + 20;  // 0x414

// Provide range of message IDs for use in histogramming and debug display.
static const int kLeastNonZeroMessageId = 1;
static const int kMaxMessageId = 1099;
static const int kNumberOfDistinctMessagesDisplayed = 1100;

//------------------------------------------------------------------------------

static LRESULT CALLBACK MessageLoopWndProc(HWND hwnd, UINT message,
                                           WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case kMsgQuit:
    case kMsgPumpATask: {
      UINT_PTR message_loop_id = static_cast<UINT_PTR>(wparam);
      MessageLoop* current_message_loop =
          reinterpret_cast<MessageLoop*>(message_loop_id);
      DCHECK(MessageLoop::current() == current_message_loop);
      return current_message_loop->MessageWndProc(hwnd, message, wparam,
                                                  lparam);
    }
  }
  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

#ifndef NDEBUG
// Force exercise of polling model.
#define CHROME_MAXIMUM_WAIT_OBJECTS 8
#else
#define CHROME_MAXIMUM_WAIT_OBJECTS MAXIMUM_WAIT_OBJECTS
#endif

//------------------------------------------------------------------------------
// A strategy of -1 uses the default case. All strategies are selected as
// positive integers.
// static
int MessageLoop::strategy_selector_ = -1;

// static
void MessageLoop::SetStrategy(int strategy) {
  DCHECK(-1 == strategy_selector_);
  strategy_selector_ = strategy;
}

//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------

MessageLoop::MessageLoop() : message_hwnd_(NULL),
                             exception_restoration_(false),
                             nestable_tasks_allowed_(true),
                             dispatcher_(NULL),
                             quit_received_(false),
                             quit_now_(false),
                             task_pump_message_pending_(false),
                             run_depth_(0) {
  DCHECK(tls_index_) << "static initializer failed";
  DCHECK(!current()) << "should only have one message loop per thread";
  ThreadLocalStorage::Set(tls_index_, this);
  InitMessageWnd();
}

MessageLoop::~MessageLoop() {
  DCHECK(this == current());
  ThreadLocalStorage::Set(tls_index_, NULL);
  DCHECK(!dispatcher_);
  DCHECK(!quit_received_ && !quit_now_);
  // Most tasks that have not been Run() are deleted in the |timer_manager_|
  // destructor after we remove our tls index.  We delete the tasks in our
  // queues here so their destuction is similar to the tasks in the
  // |timer_manager_|.
  DeletePendingTasks();
  ReloadWorkQueue();
  DeletePendingTasks();
}

void MessageLoop::SetThreadName(const std::string& thread_name) {
  DCHECK(thread_name_.empty());
  thread_name_ = thread_name;
  StartHistogrammer();
}

void MessageLoop::AddObserver(Observer *obs) {
  DCHECK(this == current());
  observers_.AddObserver(obs);
}

void MessageLoop::RemoveObserver(Observer *obs) {
  DCHECK(this == current());
  observers_.RemoveObserver(obs);
}

void MessageLoop::Run() {
  RunHandler(NULL, false);
}

void MessageLoop::Run(Dispatcher* dispatcher) {
  RunHandler(dispatcher, false);
}

void MessageLoop::RunAllPending() {
  RunHandler(NULL, true);
}

// Runs the loop in two different SEH modes:
// enable_SEH_restoration_ = false : any unhandled exception goes to the last
// one that calls SetUnhandledExceptionFilter().
// enable_SEH_restoration_ = true : any unhandled exception goes to the filter
// that was existed before the loop was run.
void MessageLoop::RunHandler(Dispatcher* dispatcher, bool non_blocking) {
  if (exception_restoration_) {
    LPTOP_LEVEL_EXCEPTION_FILTER current_filter = GetTopSEHFilter();
    __try {
      RunInternal(dispatcher, non_blocking);
    } __except(SEHFilter(current_filter)) {
    }
  } else {
    RunInternal(dispatcher, non_blocking);
  }
}

//------------------------------------------------------------------------------
// IF this was just a simple PeekMessage() loop (servicing all passible work
// queues), then Windows would try to achieve the following order according to
// MSDN documentation about PeekMessage with no filter):
//    * Sent messages
//    * Posted messages
//    * Sent messages (again)
//    * WM_PAINT messages
//    * WM_TIMER messages
//
// Summary: none of the above classes is starved, and sent messages has twice
// the chance of being processed (i.e., reduced service time).

void MessageLoop::RunInternal(Dispatcher* dispatcher, bool non_blocking) {
  // Preserve ability to be called recursively.
  ScopedStateSave save(this);  // State is restored on exit.
  dispatcher_ = dispatcher;
  StartHistogrammer();

  DCHECK(this == current());
  //
  // Process pending messages and signaled objects.
  //
  // Flush these queues before exiting due to a kMsgQuit or else we risk not
  // shutting down properly as some operations may depend on further event
  // processing. (Note: some tests may use quit_now_ to exit more swiftly,
  // and leave messages pending, so don't assert the above fact).
  RunTraditional(non_blocking);
  DCHECK(non_blocking || quit_received_ || quit_now_);
}

void MessageLoop::RunTraditional(bool non_blocking) {
  for (;;) {
    // If we do any work, we may create more messages etc., and more work
    // may possibly be waiting in another task group.  When we (for example)
    // ProcessNextWindowsMessage(), there is a good chance there are still more
    // messages waiting (same thing for ProcessNextObject(), which responds to
    // only one signaled object; etc.).  On the other hand, when any of these
    // methods return having done no work, then it is pretty unlikely that
    // calling them again quickly will find any work to do.
    // Finally, if they all say they had no work, then it is a good time to
    // consider sleeping (waiting) for more work.
    bool more_work_is_plausible = ProcessNextWindowsMessage();
    if (quit_now_)
      return;

    more_work_is_plausible |= ProcessNextDeferredTask();
    more_work_is_plausible |= ProcessNextObject();
    if (more_work_is_plausible)
      continue;

    if (quit_received_)
      return;

    // Run any timer that is ready to run. It may create messages etc.
    if (ProcessSomeTimers())
      continue;

    // We run delayed non nestable tasks only after all nestable tasks have
    // run, to preserve FIFO ordering.
    if (ProcessNextDelayedNonNestableTask())
      continue;

    if (non_blocking)
      return;

    // We service APCs in WaitForWork, without returning.
    WaitForWork();  // Wait (sleep) until we have work to do again.
  }
}

//------------------------------------------------------------------------------
// Wrapper functions for use in above message loop framework.

bool MessageLoop::ProcessNextDelayedNonNestableTask() {
  if (run_depth_ != 1)
    return false;

  if (delayed_non_nestable_queue_.Empty())
    return false;

  RunTask(delayed_non_nestable_queue_.Pop());
  return true;
}

bool MessageLoop::ProcessNextDeferredTask() {
  ReloadWorkQueue();
  return QueueOrRunTask(NULL);
}

bool MessageLoop::ProcessSomeTimers() {
  return timer_manager_.RunSomePendingTimers();
}

//------------------------------------------------------------------------------

void MessageLoop::Quit() {
  EnsureMessageGetsPosted(kMsgQuit);
}

bool MessageLoop::WatchObject(HANDLE object, Watcher* watcher) {
  DCHECK(this == current());
  DCHECK(object);
  DCHECK_NE(object, INVALID_HANDLE_VALUE);

  std::vector<HANDLE>::iterator it = find(objects_.begin(), objects_.end(),
                                          object);
  if (watcher) {
    if (it == objects_.end()) {
     static size_t warning_multiple = 1;
     if (objects_.size() >= warning_multiple * MAXIMUM_WAIT_OBJECTS / 2) {
       LOG(INFO) << "More than " << warning_multiple * MAXIMUM_WAIT_OBJECTS / 2
           << " objects being watched";
       // This DCHECK() is an artificial limitation, meant to warn us if we
       // start creating too many objects.  It can safely be raised to a higher
       // level, and the program is designed to handle much larger values.
       // Before raising this limit, make sure that there is a very good reason
       // (in your debug testing) to be watching this many objects.
       DCHECK(2 <= warning_multiple);
       ++warning_multiple;
      }
      objects_.push_back(object);
      watchers_.push_back(watcher);
    } else {
      watchers_[it - objects_.begin()] = watcher;
    }
  } else if (it != objects_.end()) {
    std::vector<HANDLE>::difference_type index = it - objects_.begin();
    objects_.erase(it);
    watchers_.erase(watchers_.begin() + index);
  }
  return true;
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

  // Local stack variables to use IF we need to process after releasing locks.
  HWND message_hwnd;
  {
    AutoLock lock1(incoming_queue_lock_);
    bool was_empty = incoming_queue_.Empty();
    incoming_queue_.Push(task);
    if (!was_empty)
      return;  // Someone else should have started the sub-pump.

    // We may have to start the sub-pump.
    AutoLock lock2(task_pump_message_lock_);
    if (task_pump_message_pending_)
      return;  // Someone else continued the pumping.
    task_pump_message_pending_ = true;  // We'll send one.
    message_hwnd = message_hwnd_;
  }  // Release both locks.
  // We may have just posted a kMsgQuit, and so this instance may now destroyed!
  // Do not invoke non-static methods, or members in any way!

  // PostMessage may fail, as the hwnd may have vanished due to kMsgQuit.
  PostMessage(message_hwnd, kMsgPumpATask, reinterpret_cast<UINT_PTR>(this), 0);
}

void MessageLoop::InitMessageWnd() {
  HINSTANCE hinst = GetModuleHandle(NULL);

  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = MessageLoopWndProc;
  wc.hInstance = hinst;
  wc.lpszClassName = kWndClass;
  RegisterClassEx(&wc);

  message_hwnd_ = CreateWindow(kWndClass, 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0,
                               hinst, 0);
  DCHECK(message_hwnd_);
}

LRESULT MessageLoop::MessageWndProc(HWND hwnd, UINT message,
                                    WPARAM wparam, LPARAM lparam) {
  DCHECK(hwnd == message_hwnd_);
  switch (message) {
    case kMsgPumpATask: {
      ProcessPumpReplacementMessage();  // Avoid starving paint and timer.
      if (!nestable_tasks_allowed_)
        return 0;
      PumpATaskDuringWndProc();
      return 0;
    }

    case kMsgQuit: {
      // TODO(jar): bug 1300541 The following assert should be used, but
      // currently too much code actually triggers the assert, especially in
      // tests :-(.
      //CHECK(!quit_received_);  // Discarding a second quit will cause a hang.
      quit_received_ = true;
      return 0;
    }
  }
  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

void MessageLoop::WillProcessMessage(const MSG& msg) {
  FOR_EACH_OBSERVER(Observer, observers_, WillProcessMessage(msg));
}

void MessageLoop::DidProcessMessage(const MSG& msg) {
  FOR_EACH_OBSERVER(Observer, observers_, DidProcessMessage(msg));
}

void MessageLoop::SetNestableTasksAllowed(bool allowed) {
  nestable_tasks_allowed_ = allowed;
  if (!nestable_tasks_allowed_)
    return;
  // Start the native pump if we are not already pumping.
  EnsurePumpATaskWasPosted();
}

bool MessageLoop::NestableTasksAllowed() const {
  return nestable_tasks_allowed_;
}


bool MessageLoop::ProcessNextWindowsMessage() {
  MSG msg;
  if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    return ProcessMessageHelper(msg);
  }
  return false;
}

bool MessageLoop::ProcessMessageHelper(const MSG& msg) {
  HistogramEvent(msg.message);

  if (WM_QUIT == msg.message) {
    // Repost the QUIT message so that it will be retrieved by the primary
    // GetMessage() loop.
    quit_now_ = true;
    PostQuitMessage(static_cast<int>(msg.wParam));
    return false;
  }

  // While running our main message pump, we discard kMsgPumpATask messages.
  if  (msg.message == kMsgPumpATask && msg.hwnd == message_hwnd_)
    return ProcessPumpReplacementMessage();

  WillProcessMessage(msg);

  if (dispatcher_) {
    if (!dispatcher_->Dispatch(msg))
      quit_now_ = true;
  } else {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  DidProcessMessage(msg);
  return true;
}

bool MessageLoop::ProcessPumpReplacementMessage() {
  MSG msg;
  bool have_message = (0 != PeekMessage(&msg, NULL, 0, 0, PM_REMOVE));
  DCHECK(!have_message || kMsgPumpATask != msg.message
         || msg.hwnd != message_hwnd_);
  {
    // Since we discarded a kMsgPumpATask message, we must update the flag.
    AutoLock lock(task_pump_message_lock_);
    DCHECK(task_pump_message_pending_);
    task_pump_message_pending_ = false;
  }
  return have_message && ProcessMessageHelper(msg);
}

// Create a mini-message-pump to force immediate processing of only Windows
// WM_PAINT messages.
void MessageLoop::PumpOutPendingPaintMessages() {
  // Don't provide an infinite loop, but do enough peeking to get the job done.
  // Actual common max is 4 peeks, but we'll be a little safe here.
  const int kMaxPeekCount = 20;
  int peek_count;
  bool win2k(true);
  if (win_util::GetWinVersion() > win_util::WINVERSION_2000)
    win2k = false;
  for (peek_count = 0; peek_count < kMaxPeekCount; ++peek_count) {
    MSG msg;
    if (win2k) {
      if (!PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE))
        break;
    } else {
      if (!PeekMessage(&msg, NULL, 0, 0, PM_REMOVE | PM_QS_PAINT))
        break;
    }
    ProcessMessageHelper(msg);
    if (quit_now_ )  // Handle WM_QUIT.
      break;
  }
  // Histogram what was really being used, to help to adjust kMaxPeekCount.
  DHISTOGRAM_COUNTS(L"Loop.PumpOutPendingPaintMessages Peeks", peek_count);
}

//------------------------------------------------------------------------------
// If we handle more than the OS limit on the number of objects that can be
// waited for, we'll need to poll (sequencing through subsets of the objects
// that can be passed in a single OS wait call).  The following is the polling
// interval used in that (unusual) case. (I don't have a lot of justifcation
// for the specific value, but it needed to be short enough that it would not
// add a lot of latency, and long enough that we wouldn't thrash the CPU for no
// reason... especially considering the silly user probably has a million tabs
// open, etc.)
static const int kMultipleWaitPollingInterval = 20;

void MessageLoop::WaitForWork() {
  bool original_can_run = nestable_tasks_allowed_;
  int wait_flags = original_can_run ? MWMO_ALERTABLE | MWMO_INPUTAVAILABLE
                                    : MWMO_INPUTAVAILABLE;

  bool use_polling = false;  // Poll if too many objects for one OS Wait call.
  for (;;) {
    // Do initialization here, in case APC modifies object list.
    size_t total_objs = original_can_run ? objects_.size() : 0;

    int delay;
    size_t polling_index = 0;  // The first unprocessed object index.
    do {
      size_t objs_len =
          (polling_index < total_objs) ? total_objs - polling_index : 0;
      if (objs_len >= CHROME_MAXIMUM_WAIT_OBJECTS) {
        objs_len = CHROME_MAXIMUM_WAIT_OBJECTS - 1;
        use_polling = true;
      }
      HANDLE* objs = objs_len ? polling_index + &objects_.front() : NULL;

      // Only wait up to the time needed by the timer manager to fire the next
      // set of timers.
      delay = timer_manager_.GetCurrentDelay();
      if (use_polling && delay > kMultipleWaitPollingInterval)
        delay = kMultipleWaitPollingInterval;
      if (delay < 0)  // Negative value means no timers waiting.
        delay = INFINITE;

      DWORD result;
      result = MsgWaitForMultipleObjectsEx(static_cast<DWORD>(objs_len), objs,
                                           delay, QS_ALLINPUT, wait_flags);

      if (WAIT_IO_COMPLETION == result) {
        HistogramEvent(kSleepingApcEvent);
        // We'll loop here when we service an APC.  At it currently stands,
        // *ONLY* the IO thread uses *any* APCs, so this should have no impact
        // on the UI thread.
        break;  // Break to outer loop, and waitforwork() again.
      }

      // Use unsigned type to simplify range detection;
      size_t signaled_index = result - WAIT_OBJECT_0;
      if (signaled_index < objs_len) {
        SignalWatcher(polling_index + signaled_index);
        HistogramEvent(kSleepingSignalEvent);
        return;  // We serviced a signaled object.
      }

      if (objs_len == signaled_index)
        return;  // A WM_* message is available.

      DCHECK_NE(WAIT_FAILED, result) << GetLastError();

      DCHECK(!objs || result == WAIT_TIMEOUT);
      if (!use_polling)
        return;
      polling_index += objs_len;
    } while (polling_index < total_objs);
    // For compatibility, we didn't return sooner.  This made us do *some* wait
    // call(s) before returning. This will probably change in next rev.
    if (!delay || !timer_manager_.GetCurrentDelay())
      return;  // No work done, but timer is ready to fire.
  }
}

// Note: MsgWaitMultipleObjects() can't take a nil list, and that is why I had
// to use SleepEx() to handle APCs when there were no objects.
bool MessageLoop::ProcessNextObject() {
  if (!nestable_tasks_allowed_)
    return false;

  size_t total_objs = objects_.size();
  if (!total_objs) {
    return false;
  }

  size_t polling_index = 0;  // The first unprocessed object index.
  do {
    DCHECK(polling_index < total_objs);
    size_t objs_len = total_objs - polling_index;
    if (objs_len >= CHROME_MAXIMUM_WAIT_OBJECTS)
      objs_len = CHROME_MAXIMUM_WAIT_OBJECTS - 1;
    HANDLE* objs = polling_index + &objects_.front();

    // Identify 1 pending object, or allow an IO APC to be completed.
    DWORD result = WaitForMultipleObjectsEx(static_cast<DWORD>(objs_len), objs,
                                            FALSE,    // 1 signal is sufficient.
                                            0,        // Wait 0ms.
                                            false);  // Not alertable (no APC).

    // Use unsigned type to simplify range detection;
    size_t signaled_index = result - WAIT_OBJECT_0;
    if (signaled_index < objs_len) {
      SignalWatcher(polling_index + signaled_index);
      HistogramEvent(kPollingSignalEvent);
      return true;  // We serviced a signaled object.
    }

    // If an handle is invalid, it will be WAIT_FAILED.
    DCHECK_EQ(WAIT_TIMEOUT, result) << GetLastError();
    polling_index += objs_len;
  } while (polling_index < total_objs);
  return false;  // We serviced nothing.
}

bool MessageLoop::SignalWatcher(size_t object_index) {
  BeforeTaskRunSetup();
  DCHECK(objects_.size() > object_index);
  // On reception of OnObjectSignaled() to a Watcher object, it may call
  // WatchObject(). watchers_ and objects_ will be modified. This is
  // expected, so don't be afraid if, while tracing a OnObjectSignaled()
  // function, the corresponding watchers_[result] is inexistant.
  watchers_[object_index]->OnObjectSignaled(objects_[object_index]);
  // Signaled objects tend to be removed from the watch list, and then added
  // back (appended).  As a result, they move to the end of the objects_ array,
  // and this should make their service "fair" (no HANDLEs should be starved).
  AfterTaskRunRestore();
  return true;
}

bool MessageLoop::RunTimerTask(Timer* timer) {
  HistogramEvent(kTimerEvent);
  Task* task = timer->task();
  if (task->is_owned_by_message_loop()) {
    // We constructed it through PostTask().
    DCHECK(!timer->repeating());
    timer->set_task(NULL);
    delete timer;
    return QueueOrRunTask(task);
  } else {
    // This is an unknown timer task, and we *can't* delay running it, as a
    // user might try to cancel it with TimerManager at any moment.
    DCHECK(nestable_tasks_allowed_);
    RunTask(task);
    return true;
  }
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
    if (task->nestable() || run_depth_ == 1) {
      RunTask(task);
      // Show that we ran a task (Note: a new one might arrive as a
      // consequence!).
      return true;
    } else {
      // We couldn't run the task now because we're in a nested message loop
      // and the task isn't nestable.
      delayed_non_nestable_queue_.Push(task);
    }
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

void MessageLoop::PumpATaskDuringWndProc() {
  // TODO(jar): Perchance we should check on signaled objects here??
  // Signals are generally starved during a native message loop.  Even if we
  // try to service a signaled object now, we wouldn't automatically get here
  // (i.e., the native pump would not re-start) when the next object was
  // signaled. If we really want to avoid starving signaled objects, we need
  // to translate them into Tasks that can be passed in via PostTask.
  // If these native message loops (and sub-pumping activities) are short
  // lived, then the starvation won't be that long :-/.

  if (!ProcessNextDeferredTask())
    return;  // Nothing to do, so lets stop the sub-pump.

  // We ran a task, so make sure we come back and try to run more tasks.
  EnsurePumpATaskWasPosted();
}

void MessageLoop::EnsurePumpATaskWasPosted() {
  {
    AutoLock lock(task_pump_message_lock_);
    if (task_pump_message_pending_)
      return;  // Someone else continued the pumping.
    task_pump_message_pending_ = true;  // We'll send one.
  }
  EnsureMessageGetsPosted(kMsgPumpATask);
}

void MessageLoop::EnsureMessageGetsPosted(int message) const {
  const int kRetryCount = 30;
  const int kSleepDurationWhenFailing = 100;
  for (int i = 0; i < kRetryCount; ++i) {
    // Posting to our own windows should always succeed.  If it doesn't we're in
    // big trouble.
    if (PostMessage(message_hwnd_, message,
                    reinterpret_cast<UINT_PTR>(this), 0))
      return;
    Sleep(kSleepDurationWhenFailing);
  }
  LOG(FATAL) << "Crash with last error " << GetLastError();
  int* p = NULL;
  *p = 0;  // Crash.
}

void MessageLoop::ReloadWorkQueue() {
  // We can improve performance of our loading tasks from incoming_queue_ to
  // work_queue_ by wating until the last minute (work_queue_ is empty) to load.
  // That reduces the number of locks-per-task significantly when our queues get
  // large.  The optimization is disabled on threads that make use of the
  // priority queue (prioritization requires all our tasks to be in the
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

//------------------------------------------------------------------------------
// Implementation of the work_queue_ as a ProiritizedTaskQueue

void MessageLoop::PrioritizedTaskQueue::push(Task * task) {
  queue_.push(PrioritizedTask(task, --next_sequence_number_));
}

bool MessageLoop::PrioritizedTaskQueue::PrioritizedTask::operator < (
    PrioritizedTask const & right) const {
  int compare = task_->priority_ - right.task_->priority_;
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

// Add one undocumented windows message to clean up our display.
#ifndef WM_SYSTIMER
#define WM_SYSTIMER 0x118
#endif

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
  // Only provide an extensive list in debug mode.  In release mode, we have to
  // read the octal values.... but we save about 450 strings, each of length
  // 10 from our binary image.
#ifndef NDEBUG
  // Prepare to include a list of names provided in a special header file4.
#define A_NAMED_MESSAGE_FROM_WINUSER_H VALUE_TO_NUMBER_AND_NAME
#include "base/windows_message_list.h"
#undef A_NAMED_MESSAGE_FROM_WINUSER_H
  // Add an undocumented message that appeared in our list :-/.
  VALUE_TO_NUMBER_AND_NAME(WM_SYSTIMER)
#endif  // NDEBUG

  // Provide some pretty print capability in our histogram for our internal
  // messages.

  // Values we use for WM_USER+n
  VALUE_TO_NUMBER_AND_NAME(kMsgPumpATask)
  VALUE_TO_NUMBER_AND_NAME(kMsgQuit)

  // A few events we handle (kindred to messages), and used to profile actions.
  VALUE_TO_NUMBER_AND_NAME(kTaskRunEvent)
  VALUE_TO_NUMBER_AND_NAME(kSleepingApcEvent)
  VALUE_TO_NUMBER_AND_NAME(kSleepingSignalEvent)
  VALUE_TO_NUMBER_AND_NAME(kPollingSignalEvent)
  VALUE_TO_NUMBER_AND_NAME(kTimerEvent)

  {-1, NULL}  // The list must be null terminated, per API to histogram.
};
