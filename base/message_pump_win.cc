// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_win.h"

#include <math.h>

#include "base/histogram.h"
#include "base/win_util.h"

namespace base {

static const wchar_t kWndClass[] = L"Chrome_MessagePumpWindow";

// Message sent to get an additional time slice for pumping (processing) another
// task (a series of such messages creates a continuous task pump).
static const int kMsgHaveWork = WM_USER + 1;

#ifndef NDEBUG
// Force exercise of polling model.
static const int kMaxWaitObjects = 8;
#else
static const int kMaxWaitObjects = MAXIMUM_WAIT_OBJECTS;
#endif

//-----------------------------------------------------------------------------
// MessagePumpWin public:

MessagePumpWin::MessagePumpWin() : have_work_(0), state_(NULL) {
  InitMessageWnd();
}

MessagePumpWin::~MessagePumpWin() {
  DestroyWindow(message_hwnd_);
}

void MessagePumpWin::WatchObject(HANDLE object, Watcher* watcher) {
  DCHECK(object);
  DCHECK_NE(object, INVALID_HANDLE_VALUE);

  std::vector<HANDLE>::iterator it =
      find(objects_.begin(), objects_.end(), object);
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
}

void MessagePumpWin::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MessagePumpWin::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MessagePumpWin::WillProcessMessage(const MSG& msg) {
  FOR_EACH_OBSERVER(Observer, observers_, WillProcessMessage(msg));
}

void MessagePumpWin::DidProcessMessage(const MSG& msg) {
  FOR_EACH_OBSERVER(Observer, observers_, DidProcessMessage(msg));
}

void MessagePumpWin::PumpOutPendingPaintMessages() {
  // Create a mini-message-pump to force immediate processing of only Windows
  // WM_PAINT messages.  Don't provide an infinite loop, but do enough peeking
  // to get the job done.  Actual common max is 4 peeks, but we'll be a little
  // safe here.
  const int kMaxPeekCount = 20;
  bool win2k = win_util::GetWinVersion() <= win_util::WINVERSION_2000;
  int peek_count;
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
    if (state_->should_quit)  // Handle WM_QUIT.
      break;
  }
  // Histogram what was really being used, to help to adjust kMaxPeekCount.
  DHISTOGRAM_COUNTS(L"Loop.PumpOutPendingPaintMessages Peeks", peek_count);
}

void MessagePumpWin::RunWithDispatcher(
    Delegate* delegate, Dispatcher* dispatcher) {
  RunState s;
  s.delegate = delegate;
  s.dispatcher = dispatcher;
  s.should_quit = false;
  s.run_depth = state_ ? state_->run_depth + 1 : 1;

  RunState* previous_state = state_;
  state_ = &s;

  DoRunLoop();

  state_ = previous_state;
}

void MessagePumpWin::Quit() {
  DCHECK(state_);
  state_->should_quit = true;
}

void MessagePumpWin::ScheduleWork() {
  if (InterlockedExchange(&have_work_, 1))
    return;  // Someone else continued the pumping.

  // Make sure the MessagePump does some work for us.
  PostMessage(message_hwnd_, kMsgHaveWork, reinterpret_cast<WPARAM>(this), 0);
}

void MessagePumpWin::ScheduleDelayedWork(const Time& delayed_work_time) {
  //
  // We would *like* to provide high resolution timers.  Windows timers using
  // SetTimer() have a 10ms granularity.  We have to use WM_TIMER as a wakeup
  // mechanism because the application can enter modal windows loops where it
  // is not running our MessageLoop; the only way to have our timers fire in
  // these cases is to post messages there.
  //
  // To provide sub-10ms timers, we process timers directly from our run loop.
  // For the common case, timers will be processed there as the run loop does
  // its normal work.  However, we *also* set the system timer so that WM_TIMER
  // events fire.  This mops up the case of timers not being able to work in
  // modal message loops.  It is possible for the SetTimer to pop and have no
  // pending timers, because they could have already been processed by the
  // run loop itself.
  //
  // We use a single SetTimer corresponding to the timer that will expire
  // soonest.  As new timers are created and destroyed, we update SetTimer.
  // Getting a spurrious SetTimer event firing is benign, as we'll just be
  // processing an empty timer queue.
  //
  delayed_work_time_ = delayed_work_time;

  int delay_msec = GetCurrentDelay();
  DCHECK(delay_msec >= 0);
  if (delay_msec < USER_TIMER_MINIMUM)
    delay_msec = USER_TIMER_MINIMUM;

  // Create a WM_TIMER event that will wake us up to check for any pending
  // timers (in case we are running within a nested, external sub-pump).
  SetTimer(message_hwnd_, reinterpret_cast<UINT_PTR>(this), delay_msec, NULL);
}

//-----------------------------------------------------------------------------
// MessagePumpWin private:

// static
LRESULT CALLBACK MessagePumpWin::WndProcThunk(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case kMsgHaveWork:
      reinterpret_cast<MessagePumpWin*>(wparam)->HandleWorkMessage();
      break;
    case WM_TIMER:
      reinterpret_cast<MessagePumpWin*>(wparam)->HandleTimerMessage();
      break;
  }
  return DefWindowProc(hwnd, message, wparam, lparam);
}

void MessagePumpWin::InitMessageWnd() {
  HINSTANCE hinst = GetModuleHandle(NULL);

  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProcThunk;
  wc.hInstance = hinst;
  wc.lpszClassName = kWndClass;
  RegisterClassEx(&wc);

  message_hwnd_ =
      CreateWindow(kWndClass, 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, hinst, 0);
  DCHECK(message_hwnd_);
}

void MessagePumpWin::HandleWorkMessage() {
  // If we are being called outside of the context of Run, then don't do
  // anything.  This could correspond to a MessageBox call or something of
  // that sort.
  if (!state_)
    return;

  // Let whatever would have run had we not been putting messages in the queue
  // run now.  This is an attempt to make our dummy message not starve other
  // messages that may be in the Windows message queue.
  ProcessPumpReplacementMessage();

  // Now give the delegate a chance to do some work.  He'll let us know if he
  // needs to do more work.
  if (state_->delegate->DoWork())
    ScheduleWork();
}

void MessagePumpWin::HandleTimerMessage() {
  KillTimer(message_hwnd_, reinterpret_cast<UINT_PTR>(this));

  // If we are being called outside of the context of Run, then don't do
  // anything.  This could correspond to a MessageBox call or something of
  // that sort.
  if (!state_)
    return;

  state_->delegate->DoDelayedWork(&delayed_work_time_);
  if (!delayed_work_time_.is_null()) {
    // A bit gratuitous to set delayed_work_time_ again, but oh well.
    ScheduleDelayedWork(delayed_work_time_);
  }
}

void MessagePumpWin::DoRunLoop() {
  // IF this was just a simple PeekMessage() loop (servicing all passible work
  // queues), then Windows would try to achieve the following order according
  // to MSDN documentation about PeekMessage with no filter):
  //    * Sent messages
  //    * Posted messages
  //    * Sent messages (again)
  //    * WM_PAINT messages
  //    * WM_TIMER messages
  //
  // Summary: none of the above classes is starved, and sent messages has twice
  // the chance of being processed (i.e., reduced service time).

  for (;;) {
    // If we do any work, we may create more messages etc., and more work may
    // possibly be waiting in another task group.  When we (for example)
    // ProcessNextWindowsMessage(), there is a good chance there are still more
    // messages waiting (same thing for ProcessNextObject(), which responds to
    // only one signaled object; etc.).  On the other hand, when any of these
    // methods return having done no work, then it is pretty unlikely that
    // calling them again quickly will find any work to do.  Finally, if they
    // all say they had no work, then it is a good time to consider sleeping
    // (waiting) for more work.

    bool more_work_is_plausible = ProcessNextWindowsMessage();
    if (state_->should_quit)
      break;

    more_work_is_plausible |= state_->delegate->DoWork();
    if (state_->should_quit)
      break;

    more_work_is_plausible |= ProcessNextObject();
    if (state_->should_quit)
      break;

    if (more_work_is_plausible)
      continue;

    more_work_is_plausible =
        state_->delegate->DoDelayedWork(&delayed_work_time_);
    // If we did not process any delayed work, then we can assume that our
    // existing WM_TIMER if any will fire when delayed work should run.  We
    // don't want to disturb that timer if it is already in flight.  However,
    // if we did do all remaining delayed work, then lets kill the WM_TIMER.
    if (more_work_is_plausible && delayed_work_time_.is_null())
      KillTimer(message_hwnd_, reinterpret_cast<UINT_PTR>(this));
    if (state_->should_quit)
      break;

    if (more_work_is_plausible)
      continue;

    more_work_is_plausible = state_->delegate->DoIdleWork();
    if (state_->should_quit)
      break;

    if (more_work_is_plausible)
      continue;

    // We service APCs in WaitForWork, without returning.
    WaitForWork();  // Wait (sleep) until we have work to do again.
  }
}

// If we handle more than the OS limit on the number of objects that can be
// waited for, we'll need to poll (sequencing through subsets of the objects
// that can be passed in a single OS wait call).  The following is the polling
// interval used in that (unusual) case. (I don't have a lot of justifcation
// for the specific value, but it needed to be short enough that it would not
// add a lot of latency, and long enough that we wouldn't thrash the CPU for no
// reason... especially considering the silly user probably has a million tabs
// open, etc.)
static const int kMultipleWaitPollingInterval = 20;

void MessagePumpWin::WaitForWork() {
  // Wait until either an object is signaled or a message is available.  Handle
  // (without returning) any APCs (only the IO thread currently has APCs.)

  // We do not support nested message loops when we have watched objects.  This
  // is to avoid messy recursion problems.
  DCHECK(objects_.empty() || state_->run_depth == 1) <<
      "Cannot nest a message loop when there are watched objects!";

  int wait_flags = MWMO_ALERTABLE | MWMO_INPUTAVAILABLE;

  bool use_polling = false;  // Poll if too many objects for one OS Wait call.
  for (;;) {
    // Do initialization here, in case APC modifies object list.
    size_t total_objs = objects_.size();

    int delay;
    size_t polling_index = 0;  // The first unprocessed object index.
    do {
      size_t objs_len =
          (polling_index < total_objs) ? total_objs - polling_index : 0;
      if (objs_len >= MAXIMUM_WAIT_OBJECTS) {
        objs_len = MAXIMUM_WAIT_OBJECTS - 1;
        use_polling = true;
      }
      HANDLE* objs = objs_len ? polling_index + &objects_.front() : NULL;

      // Only wait up to the time needed by the timer manager to fire the next
      // set of timers.
      delay = GetCurrentDelay();
      if (use_polling && delay > kMultipleWaitPollingInterval)
        delay = kMultipleWaitPollingInterval;
      if (delay < 0)  // Negative value means no timers waiting.
        delay = INFINITE;

      DWORD result;
      result = MsgWaitForMultipleObjectsEx(static_cast<DWORD>(objs_len), objs,
                                           delay, QS_ALLINPUT, wait_flags);

      if (WAIT_IO_COMPLETION == result) {
        // We'll loop here when we service an APC.  At it currently stands,
        // *ONLY* the IO thread uses *any* APCs, so this should have no impact
        // on the UI thread.
        break;  // Break to outer loop, and waitforwork() again.
      }

      // Use unsigned type to simplify range detection;
      size_t signaled_index = result - WAIT_OBJECT_0;
      if (signaled_index < objs_len) {
        SignalWatcher(polling_index + signaled_index);
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
    if (!delay || !GetCurrentDelay())
      return;  // No work done, but timer is ready to fire.
  }
}

bool MessagePumpWin::ProcessNextWindowsMessage() {
  MSG msg;
  if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    return ProcessMessageHelper(msg);
  return false;
}

bool MessagePumpWin::ProcessMessageHelper(const MSG& msg) {
  if (WM_QUIT == msg.message) {
    // Repost the QUIT message so that it will be retrieved by the primary
    // GetMessage() loop.
    state_->should_quit = true;
    PostQuitMessage(static_cast<int>(msg.wParam));
    return false;
  }

  // While running our main message pump, we discard kMsgHaveWork messages.
  if (msg.message == kMsgHaveWork && msg.hwnd == message_hwnd_)
    return ProcessPumpReplacementMessage();

  WillProcessMessage(msg);

  if (state_->dispatcher) {
    if (!state_->dispatcher->Dispatch(msg))
      state_->should_quit = true;
  } else {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  DidProcessMessage(msg);
  return true;
}

bool MessagePumpWin::ProcessPumpReplacementMessage() {
  // When we encounter a kMsgHaveWork message, this method is called to peek
  // and process a replacement message, such as a WM_PAINT or WM_TIMER.  The
  // goal is to make the kMsgHaveWork as non-intrusive as possible, even though
  // a continuous stream of such messages are posted.  This method carefully
  // peeks a message while there is no chance for a kMsgHaveWork to be pending,
  // then resets the have_work_ flag (allowing a replacement kMsgHaveWork to
  // possibly be posted), and finally dispatches that peeked replacement.  Note
  // that the re-post of kMsgHaveWork may be asynchronous to this thread!!

  MSG msg;
  bool have_message = (0 != PeekMessage(&msg, NULL, 0, 0, PM_REMOVE));
  DCHECK(!have_message || kMsgHaveWork != msg.message ||
         msg.hwnd != message_hwnd_);
  
  // Since we discarded a kMsgHaveWork message, we must update the flag.
  InterlockedExchange(&have_work_, 0);

  // TODO(darin,jar): There is risk of being lost in a sub-pump within the call
  // to ProcessMessageHelper, which could result in no longer getting a
  // kMsgHaveWork message until the next out-of-band call to ScheduleWork.

  return have_message && ProcessMessageHelper(msg);
}

// Note: MsgWaitMultipleObjects() can't take a nil list, and that is why I had
// to use SleepEx() to handle APCs when there were no objects.
bool MessagePumpWin::ProcessNextObject() {
  size_t total_objs = objects_.size();
  if (!total_objs) {
    return false;
  }

  size_t polling_index = 0;  // The first unprocessed object index.
  do {
    DCHECK(polling_index < total_objs);
    size_t objs_len = total_objs - polling_index;
    if (objs_len >= kMaxWaitObjects)
      objs_len = kMaxWaitObjects - 1;
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
      return true;  // We serviced a signaled object.
    }

    // If an handle is invalid, it will be WAIT_FAILED.
    DCHECK_EQ(WAIT_TIMEOUT, result) << GetLastError();
    polling_index += objs_len;
  } while (polling_index < total_objs);
  return false;  // We serviced nothing.
}

bool MessagePumpWin::SignalWatcher(size_t object_index) {
  // Signal the watcher corresponding to the given index.

  DCHECK(objects_.size() > object_index);

  // On reception of OnObjectSignaled() to a Watcher object, it may call
  // WatchObject(). watchers_ and objects_ will be modified. This is expected,
  // so don't be afraid if, while tracing a OnObjectSignaled() function, the
  // corresponding watchers_[result] is non-existant.
  watchers_[object_index]->OnObjectSignaled(objects_[object_index]);

  // Signaled objects tend to be removed from the watch list, and then added
  // back (appended).  As a result, they move to the end of the objects_ array,
  // and this should make their service "fair" (no HANDLEs should be starved).

  return true;
}

int MessagePumpWin::GetCurrentDelay() const {
  if (delayed_work_time_.is_null())
    return -1;

  // Be careful here.  TimeDelta has a precision of microseconds, but we want a
  // value in milliseconds.  If there are 5.5ms left, should the delay be 5 or
  // 6?  It should be 6 to avoid executing delayed work too early.
  double timeout = ceil((delayed_work_time_ - Time::Now()).InMillisecondsF());

  // If this value is negative, then we need to run delayed work soon.
  int delay = static_cast<int>(timeout);
  if (delay < 0)
    delay = 0;

  return delay;
}

}  // namespace base

