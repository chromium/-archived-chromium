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

#ifndef BASE_MESSAGE_PUMP_WIN_H_
#define BASE_MESSAGE_PUMP_WIN_H_

#include <vector>

#include <windows.h>

#include "base/lock.h"
#include "base/message_pump.h"
#include "base/observer_list.h"
#include "base/time.h"

namespace base {

// MessagePumpWin implements a "traditional" Windows message pump.  It contains
// a nearly infinite loop that peeks out messages, and then dispatches them.
// Intermixed with those peeks are callouts to DoWork for pending tasks,
// DoDelayedWork for pending timers, and OnObjectSignaled for signaled objects.
// When there are no events to be serviced, this pump goes into a wait state.
// In most cases, this message pump handles all processing.
//
// However, when a task, or windows event, invokes on the stack a native dialog
// box or such, that window typically provides a bare bones (native?) message
// pump.  That bare-bones message pump generally supports little more than a
// peek of the Windows message queue, followed by a dispatch of the peeked
// message.  MessageLoop extends that bare-bones message pump to also service
// Tasks, at the cost of some complexity.
//
// The basic structure of the extension (refered to as a sub-pump) is that a
// special message, kMsgHaveWork, is repeatedly injected into the Windows
// Message queue.  Each time the kMsgHaveWork message is peeked, checks are
// made for an extended set of events, including the availability of Tasks to
// run.
//
// After running a task, the special message kMsgHaveWork is again posted to
// the Windows Message queue, ensuring a future time slice for processing a
// future event.  To prevent flooding the Windows Message queue, care is taken
// to be sure that at most one kMsgHaveWork message is EVER pending in the
// Window's Message queue.
//
// There are a few additional complexities in this system where, when there are
// no Tasks to run, this otherwise infinite stream of messages which drives the
// sub-pump is halted.  The pump is automatically re-started when Tasks are
// queued.
//
// A second complexity is that the presence of this stream of posted tasks may
// prevent a bare-bones message pump from ever peeking a WM_PAINT or WM_TIMER.
// Such paint and timer events always give priority to a posted message, such as
// kMsgHaveWork messages.  As a result, care is taken to do some peeking in
// between the posting of each kMsgHaveWork message (i.e., after kMsgHaveWork
// is peeked, and before a replacement kMsgHaveWork is posted).
//
// NOTE: Although it may seem odd that messages are used to start and stop this
// flow (as opposed to signaling objects, etc.), it should be understood that
// the native message pump will *only* respond to messages.  As a result, it is
// an excellent choice.  It is also helpful that the starter messages that are
// placed in the queue when new task arrive also awakens DoRunLoop.
//
class MessagePumpWin : public MessagePump {
 public:
  // Used with WatchObject to asynchronously monitor the signaled state of a
  // HANDLE object.
  class Watcher {
   public:
    virtual ~Watcher() {}
    // Called from MessageLoop::Run when a signalled object is detected.
    virtual void OnObjectSignaled(HANDLE object) = 0;
  };

  // An Observer is an object that receives global notifications from the
  // MessageLoop.
  //
  // NOTE: An Observer implementation should be extremely fast!
  //
  class Observer {
   public:
    virtual ~Observer() {}

    // This method is called before processing a message.
    // The message may be undefined in which case msg.message is 0
    virtual void WillProcessMessage(const MSG& msg) = 0;

    // This method is called when control returns from processing a UI message.
    // The message may be undefined in which case msg.message is 0
    virtual void DidProcessMessage(const MSG& msg) = 0;
  };

  // Dispatcher is used during a nested invocation of Run to dispatch events.
  // If Run is invoked with a non-NULL Dispatcher, MessageLoop does not
  // dispatch events (or invoke TranslateMessage), rather every message is
  // passed to Dispatcher's Dispatch method for dispatch. It is up to the
  // Dispatcher to dispatch, or not, the event.
  //
  // The nested loop is exited by either posting a quit, or returning false
  // from Dispatch.
  class Dispatcher {
   public:
    virtual ~Dispatcher() {}
    // Dispatches the event. If true is returned processing continues as
    // normal. If false is returned, the nested loop exits immediately.
    virtual bool Dispatch(const MSG& msg) = 0;
  };

  MessagePumpWin();
  ~MessagePumpWin();

  // Have the current thread's message loop watch for a signaled object.
  // Pass a null watcher to stop watching the object.
  void WatchObject(HANDLE, Watcher*);

  // Add an Observer, which will start receiving notifications immediately.
  void AddObserver(Observer* observer);

  // Remove an Observer.  It is safe to call this method while an Observer is
  // receiving a notification callback.
  void RemoveObserver(Observer* observer);

  // Give a chance to code processing additional messages to notify the
  // message loop observers that another message has been processed.
  void WillProcessMessage(const MSG& msg);
  void DidProcessMessage(const MSG& msg);

  // Applications can call this to encourage us to process all pending WM_PAINT
  // messages.  This method will process all paint messages the Windows Message
  // queue can provide, up to some fixed number (to avoid any infinite loops).
  void PumpOutPendingPaintMessages();

  // Like MessagePump::Run, but MSG objects are routed through dispatcher.
  void RunWithDispatcher(Delegate* delegate, Dispatcher* dispatcher);

  // MessagePump methods:
  virtual void Run(Delegate* delegate) { RunWithDispatcher(delegate, NULL); }
  virtual void Quit();
  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const TimeDelta& delay);

 private:
  struct RunState {
    Delegate* delegate;
    Dispatcher* dispatcher;

    // Used to flag that the current Run() invocation should return ASAP.
    bool should_quit;

    // Used to count how many Run() invocations are on the stack.
    int run_depth;
  };

  static LRESULT CALLBACK WndProcThunk(
      HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  void InitMessageWnd();
  void HandleWorkMessage();
  void HandleTimerMessage();
  void DoRunLoop();
  void WaitForWork();
  bool ProcessNextWindowsMessage();
  bool ProcessMessageHelper(const MSG& msg);
  bool ProcessPumpReplacementMessage();
  bool ProcessNextObject();
  bool SignalWatcher(size_t object_index);
  int GetCurrentDelay() const;

  // A hidden message-only window.
  HWND message_hwnd_;

  // A vector of objects (and corresponding watchers) that are routinely
  // serviced by this message pump.
  std::vector<HANDLE> objects_;
  std::vector<Watcher*> watchers_;

  ObserverList<Observer> observers_;

  // The time at which delayed work should run.
  Time delayed_work_time_;

  // A boolean value used to indicate if there is a kMsgDoWork message pending
  // in the Windows Message queue.  There is at most one such message, and it
  // can drive execution of tasks when a native message pump is running.
  LONG have_work_;

  // State for the current invocation of Run.
  RunState* state_;
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_WIN_H_
