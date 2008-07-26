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

#ifndef BASE_MESSAGE_LOOP_H__
#define BASE_MESSAGE_LOOP_H__

#include <windows.h>
#include <deque>
#include <queue>
#include <string>
#include <vector>

#include "base/histogram.h"
#include "base/observer_list.h"
#include "base/id_map.h"
#include "base/task.h"
#include "base/timer.h"
#include "base/thread_local_storage.h"

//
// A MessageLoop is used to process events for a particular thread.
// There is at most one MessageLoop instance per thread.
// Events include Windows Message Queue messages, Tasks submitted to PostTask
// or managed by TimerManager, APC calls (as time permits), and signals sent to
// a registered set of HANDLES.
// Processing events corresponds (respectively) to dispatching Windows messages,
// running Tasks, yielding time to APCs, and calling Watchers when the
// corresponding HANDLE is signaled.

//
// NOTE: Unless otherwise specified, a MessageLoop's methods may only be called
// on the thread where the MessageLoop's Run method executes.
//
// WARNING: MessageLoop has task reentrancy protection. This means that if a
// task is being processed, a second task cannot start until the first task is
// finished. Reentrancy can happen when processing a task, and an inner message
// pump is created.  That inner pump then processes windows messages which could
// implicitly start an inner task. Inner messages pumps are created with dialogs
// (DialogBox), common dialogs (GetOpenFileName), OLE functions (DoDragDrop),
// printer functions (StartDoc) and *many* others.
// Sample workaround when inner task processing is needed:
//   bool old_state = MessageLoop::current()->NestableTasksAllowed();
//   MessageLoop::current()->SetNestableTasksAllowed(true);
//   HRESULT hr = DoDragDrop(...); // Implicitly runs a modal message loop here.
//   MessageLoop::current()->SetNestableTasksAllowed(old_state);
//   // Process hr  (the result returned by DoDragDrop().
//
// Please be **SURE** your task is reentrant and all global variables are stable
// and accessible before calling SetNestableTasksAllowed(true).
//

// Message loop has several distinct functions.  It provides message pumps,
// responds to windows message dispatches, manipulates queues of Tasks.
// The most central operation is the implementation of message pumps, along with
// several subtleties.

// MessageLoop currently implements several different message pumps.  A message
// pump is (traditionally) something that reads from an incoming queue, and then
// dispatches the work.
//
// The first message pump, RunTraditional(), is among other things a
// traditional Windows Message pump.  It contains a nearly infinite loop that
// peeks out messages, and then dispatches them.
// Intermixed with those peeks are checks on a queue of Tasks, checks for
// signaled objects, and checks to see if TimerManager has tasks to run.
// When there are no events to be serviced, this pump goes into a wait state.
// For 99.99% of all events, this first message pump handles all processing.
//
// When a task, or windows event, invokes on the stack a native dialog box or
// such, that window typically provides a bare bones (native?) message pump.
// That bare-bones message pump generally supports little more than a peek of
// the Windows message queue, followed by a dispatch of the peeked message.
// MessageLoop extends that bare-bones message pump to also service Tasks, at
// the cost of some complexity.
// The basic structure of the extension (refered to as a sub-pump) is that a
// special message,kMsgPumpATask, is repeatedly injected into the Windows
// Message queue. Each time the kMsgPumpATask message is peeked, checks are made
// for an extended set of events, including the availability of Tasks to run.
//
// After running a task, the special message kMsgPumpATask is again posted to
// the Windows Message queue, ensuring a future time slice for processing a
// future event.
//
// To prevent flooding the Windows Message queue, care is taken to be sure that
// at most one kMsgPumpATask message is EVER pending in the Winow's Message
// queue.
//
// There are a few additional complexities in this system where, when there are
// no Tasks to run, this otherwise infinite stream of messages which drives the
// sub-pump is halted.  The pump is automatically re-started when Tasks are
// queued.
//
// A second complexity is that the presence of this stream of posted tasks may
// prevent a bare-bones message pump from ever peeking a WM_PAINT or WM_TIMER.
// Such paint and timer events always give priority to a posted message, such as
// kMsgPumpATask messages.  As a result, care is taken to do some peeking in
// between the posting of each kMsgPumpATask message (i.e., after kMsgPumpATask
// is peeked, and before a replacement kMsgPumpATask is posted).
//
//
// NOTE: Although it may seem odd that messages are used to start and stop this
// flow (as opposed to signaling objects, etc.), it should be understood that
// the native message pump will *only* respond to messages.  As a result, it is
// an excellent choice.  It is also helpful that the starter messages that are
// placed in the queue when new task arrive also awakens the RunTraditional()
// loop.

//------------------------------------------------------------------------------
// Define a macro to record where (in the sourec code) each Task is posted from.
#define FROM_HERE tracked_objects::Location(__FUNCTION__, __FILE__, __LINE__)

//------------------------------------------------------------------------------
class MessageLoop {
 public:

  // Select a non-default strategy for serving pending requests, that is to be
  // used by all MessageLoop instances.  This is called only once before
  // constructing any instances.
  static void SetStrategy(int strategy);
  static void EnableHistogrammer(bool enable_histogrammer);

  // Used with WatchObject to asynchronously monitor the signaled state of a
  // HANDLE object.
  class Watcher {
   public:
    virtual ~Watcher() {}
    // Called from MessageLoop::Run when a signalled object is detected.
    virtual void OnObjectSignaled(HANDLE object) = 0;
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
    // Define a macro for use in the PostTask() or PostDelayedTask()
    // invocations.  The definition varies depending upon mode (DEBUG, etc.),
    // but for now we'll just define it as an int.  In other modes it may
    // encapsulate the file and line number of the source code where it is
    // expanded.

    virtual ~Dispatcher() {}
    // Dispatches the event. If true is returned processing continues as
    // normal. If false is returned, the nested loop exits immediately.
    virtual bool Dispatch(const MSG& msg) = 0;
  };

  // Have the current thread's message loop watch for a signaled object.
  // Pass a null watcher to stop watching the object.
  bool WatchObject(HANDLE, Watcher*);

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

  // Add an Observer, which will start receiving notifications immediately.
  void AddObserver(Observer* observer);

  // Remove an Observer.  It is safe to call this method while an Observer is
  // receiving a notification callback.
  void RemoveObserver(Observer* observer);

  // Call the task's Run method asynchronously from within a message loop at
  // some point in the future.  With the PostTask variant, tasks are invoked in
  // FIFO order, inter-mixed with normal UI event processing.  With the
  // PostDelayedTask variant, tasks are called after at least approximately
  // 'delay_ms' have elapsed.
  //
  // The MessageLoop takes ownership of the Task, and deletes it after it
  // has been Run().
  //
  // NOTE: This method may be called on any thread.  The Task will be invoked
  // on the thread that executes MessageLoop::Run().

  void PostTask(const tracked_objects::Location& from_here, Task* task) {
    PostDelayedTask(from_here, task, 0);
  }

  void PostDelayedTask(const tracked_objects::Location& from_here, Task* task,
                       int delay_ms);

  // A variant on PostTask that deletes the given object.  This is useful
  // if the object needs to live until the next run of the MessageLoop (for
  // example, deleting a RenderProcessHost from within an IPC callback is not
  // good).
  //
  // NOTE: This method may be called on any thread.  The object will be deleted
  // on the thread that executes MessageLoop::Run().  If this is not the same
  // as the thread that calls PostDelayedTask(FROM_HERE, ), then T MUST inherit
  // from RefCountedThreadSafe<T>!
  template <class T>
  void DeleteSoon(const tracked_objects::Location& from_here, T* object) {
    PostTask(from_here, new DeleteTask<T>(object));
  }

  // A variant on PostTask that releases the given reference counted object
  // (by calling its Release method).  This is useful if the object needs to
  // live until the next run of the MessageLoop, or if the object needs to be
  // released on a particular thread.
  //
  // NOTE: This method may be called on any thread.  The object will be
  // released (and thus possibly deleted) on the thread that executes
  // MessageLoop::Run().  If this is not the same as the thread that calls
  // PostDelayedTask(FROM_HERE, ), then T MUST inherit from
  // RefCountedThreadSafe<T>!
  template <class T>
  void ReleaseSoon(const tracked_objects::Location& from_here, T* object) {
    PostTask(from_here, new ReleaseTask<T>(object));
  }

  // Run the message loop
  void Run();

  // See description of Dispatcher for how Run uses Dispatcher.
  void Run(Dispatcher* dispatcher);

  // Signals the Run method to return after it is done processing all pending
  // messages.  This method may be called from any thread, but no effort is
  // made to support concurrent calls to this method from multiple threads.
  //
  // For example, the first call to Quit may lead to the MessageLoop being
  // deleted once its Run method returns, so a second call from another thread
  // could be problematic.
  void Quit();

  // Invokes Quit on the current MessageLoop when run. Useful to schedule an
  // arbitrary MessageLoop to Quit.
  class QuitTask : public Task {
   public:
    virtual void Run() {
      MessageLoop::current()->Quit();
    }
  };

  // Wnd Proc for message_hwnd_.
  LRESULT MessageWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Normally, it is not necessary to instantiate a MessageLoop.  Instead, it
  // is typical to make use of the current thread's MessageLoop instance.
  MessageLoop();
  ~MessageLoop();

  // Optional call to connect the thread name with this loop.
  void SetThreadName(const std::string& thread_name);
  std::string thread_name() const { return thread_name_; }

  // Returns the MessageLoop object for the current thread, or null if none.
  static MessageLoop* current() {
    return static_cast<MessageLoop*>(ThreadLocalStorage::Get(tls_index_));
  }

  // Returns the TimerManager object for the current thread.
  TimerManager* timer_manager() { return &timer_manager_; }

  // Give a chance to code processing additional messages to notify the
  // message loop delegates that another message has been processed.
  void WillProcessMessage(const MSG& msg);
  void DidProcessMessage(const MSG& msg);

  // Enables or disables the recursive task processing. This happens in the case
  // of recursive message loops. Some unwanted message loop may occurs when
  // using common controls or printer functions. By default, recursive task
  // processing is disabled.
  //
  // The specific case where tasks get queued is:
  // - The thread is running a message loop.
  // - It receives a task #1 and execute it.
  // - The task #1 implicitly start a message loop, like a MessageBox in the
  //   unit test. This can also be StartDoc or GetSaveFileName.
  // - The thread receives a task #2 before or while in this second message
  //   loop.
  // - With NestableTasksAllowed set to true, the task #2 will run right away.
  //   Otherwise, it will get executed right after task #1 completes at "thread
  //   message loop level".
  void SetNestableTasksAllowed(bool allowed);
  bool NestableTasksAllowed() const;

  // Enables or disables the restoration during an exception of the unhandled
  // exception filter that was active when Run() was called. This can happen
  // if some third party code call SetUnhandledExceptionFilter() and never
  // restores the previous filter.
  void set_exception_restoration(bool restore) {
    exception_restoration_ = restore;
  }

  // Public entry point for TimerManager to request the Run() of a task.  If we
  // created the task during an PostTask(FROM_HERE, ), then we will also perform
  // destructions, and we'll have the option of queueing the task.  If we didn't
  // create the timer, then we will Run it immediately.
  bool RunTimerTask(Timer* timer);

  // Since some Timer's are owned by MessageLoop, the TimerManager (when it is
  // being destructed) passses us the timers to discard (without doing a Run()).
  void DiscardTimer(Timer* timer);

  // Applications can call this to encourage us to process all pending WM_PAINT
  // messages.
  // This method will process all paint messages the Windows Message queue can
  // provide, up to some fixed number (to avoid any infinite loops).
  void PumpOutPendingPaintMessages();

  //----------------------------------------------------------------------------
 private:
  struct ScopedStateSave {
    explicit ScopedStateSave(MessageLoop* loop)
        : loop_(loop),
          dispatcher_(loop->dispatcher_),
          quit_now_(loop->quit_now_),
          quit_received_(loop->quit_received_) {
      loop->quit_now_ = loop->quit_received_ = false;
    }

    ~ScopedStateSave() {
      loop_->quit_received_ = quit_received_;
      loop_->quit_now_ = quit_now_;
      loop_->dispatcher_ = dispatcher_;
    }

   private:
    MessageLoop* loop_;
    Dispatcher* dispatcher_;
    bool quit_now_;
    bool quit_received_;
  };  // struct ScopedStateSave

  // A prioritized queue with interface that mostly matches std::queue<>.
  // For debugging/performance testing, you can swap in std::queue<Task*>.
  class PrioritizedTaskQueue {
   public:
    PrioritizedTaskQueue() : next_sequence_number_(0) {}
    ~PrioritizedTaskQueue() {}
    void pop()    { queue_.pop(); }
    bool empty()  { return queue_.empty(); }
    size_t size() { return queue_.size(); }
    Task* front() { return queue_.top().task(); }
    void push(Task * task);

   private:
    class PrioritizedTask {
     public:
      PrioritizedTask(Task* task, int sequence_number)
        : task_(task),
          sequence_number_(sequence_number),
          priority_(task->priority()) {}
      Task* task() { return task_; }
      bool operator < (PrioritizedTask const & right) const ;

     private:
      Task* task_;
      // Number to ensure (default) FIFO ordering in a PriorityQueue.
      int sequence_number_;
      // Priority of task when pushed.
      int priority_;
    };  // class PrioritizedTask

    std::priority_queue<PrioritizedTask> queue_;
    // Default sequence number used when push'ing (monotonically decreasing).
    int next_sequence_number_;
    DISALLOW_EVIL_CONSTRUCTORS(PrioritizedTaskQueue);
  };

  // Implementation of a TaskQueue as a null terminated list, with end pointers.
  class TaskQueue {
   public:
    TaskQueue() : first_(NULL), last_(NULL) {}
    void Push(Task* task);
    Task* Pop();  // Extract the next Task from the queue, and return it.
    bool Empty() const { return !first_; }
    friend void std::swap<TaskQueue>(TaskQueue&, TaskQueue&);
   private:
    Task* first_;
    Task* last_;
  };

  // Implementation of a Task queue that automatically switches into a priority
  // queue if it observes any non-zero priorities in tasks.
  class OptionallyPrioritizedTaskQueue {
   public:
    OptionallyPrioritizedTaskQueue() : use_priority_queue_(false) {}
    void Push(Task* task);
    Task* Pop();  // Extract next Task from queue, and return it.
    bool Empty();
    bool use_priority_queue() const { return use_priority_queue_; }

   private:
    bool use_priority_queue_;
    PrioritizedTaskQueue prioritized_queue_;
    TaskQueue queue_;
    DISALLOW_EVIL_CONSTRUCTORS(OptionallyPrioritizedTaskQueue);
  };

  void InitMessageWnd();

  // The actual message loop implementation. Called by all flavors of Run().
  // It will run the message loop in a SEH try block or not depending on the
  // set_SEH_restoration() flag.
  void RunInternal(Dispatcher* dispatcher);

  //----------------------------------------------------------------------------
  // A list of alternate message loop priority systems.  The strategy_selector_
  // determines which one to actually use.
  void RunTraditional();

  //----------------------------------------------------------------------------
  // A list of method wrappers with identical calling signatures (no arguments)
  // for use in the main message loop.  Method pointers to these methods may be
  // called round-robin from the main message loop, on any desired schedule.

  bool ProcessNextDeferredTask();
  bool ProcessNextDelayedNonNestableTask();
  bool ProcessNextObject();
  bool ProcessSomeTimers();

  //----------------------------------------------------------------------------
  // Process some pending messages.
  // Returns true if a message was processed.
  bool ProcessNextWindowsMessage();

  // Wait until either an object is signaled, a message is available, a timer
  // needs attention, or our incoming_queue_ has gotten a task.
  // Handle (without returning) any APCs (only IO thread currently has APCs.)
  void WaitForWork();

  // Helper function for processing window messages. This includes handling
  // WM_QUIT, message translation and dispatch, etc.
  //
  // If dispatcher_ is non-NULL this method does NOT dispatch the event, instead
  // it invokes Dispatch on the dispatcher_.
  bool ProcessMessageHelper(const MSG& msg);

  // When we encounter a kMsgPumpATask, the following helper can be called to
  // peek and process a replacement message, such as a WM_PAINT or WM_TIMER.
  // The goal is to make the kMsgPumpATask as non-intrusive as possible, even
  // though a continuous stream of such messages are posted.  This method
  // carefully peeks a message while there is no chance for a kMsgPumpATask to
  // be pending, then releases the lock (allowing a replacement kMsgPumpATask to
  // possibly be posted), and finally dispatches that peeked replacement.
  // Note that the re-post of kMsgPumpATask may be asynchronous to this thread!!
  bool ProcessPumpReplacementMessage();

  // Signals a watcher if a wait falls within the range of objects we're
  // waiting on.  object_index is the offset in objects_ that was signaled.
  // Returns true if an object was signaled.
  bool SignalWatcher(size_t object_index);

  // Run a work_queue_ task or new_task, and delete it (if it was processed by
  // PostTask). If there are queued tasks, the oldest one is executed and
  // new_task is queued. new_task is optional and can be NULL. In this NULL
  // case, the method will run one pending task (if any exist). Returns true if
  // it executes a task.
  // Queued tasks accumulate only when there is a nonreentrant task currently
  // processing, in which case the new_task is appended to the list
  // work_queue_.  Such re-entrancy generally happens when an unrequested
  // message pump (typical of a native dialog) is executing in the context of a
  // task.
  bool QueueOrRunTask(Task* new_task);

  // Runs the specified task and deletes it.
  void RunTask(Task* task);

  // Make state adjustments just before and after running tasks so that we can
  // continue to work if a native message loop is employed during a task.
  void BeforeTaskRunSetup();
  void AfterTaskRunRestore();

  // When processing messages in our MessageWndProc(), we are sometimes called
  // by a native message pump (i.e., We are not called out of our Run() pump).
  // In those cases, we need to process tasks during the Windows Message
  // callback.  This method processes a task, and also posts a new kMsgPumpATask
  // messages to the Windows Msg Queue so that we are called back later (to
  // process additional tasks).
  void PumpATaskDuringWndProc();

  // Load tasks from the incoming_queue_ into work_queue_ if the latter is
  // empty.  The former requires a lock to access, while the latter is directly
  // accessible on this thread.
  void ReloadWorkQueue();

  // Delete tasks that haven't run yet without running them.  Used in the
  // destructor to make sure all the task's destructors get called.
  void DeletePendingTasks();

  // Make sure a kPumpATask message is in flight, which starts/continues the
  // sub-pump.
  void EnsurePumpATaskWasPosted();

  // Do a PostMessage(), and crash if we can't eventually do the post.
  void EnsureMessageGetsPosted(int message) const;

  // Post a task to our incomming queue.
  void MessageLoop::PostTaskInternal(Task* task);

  // Start recording histogram info about events and action IF it was enabled
  // and IF the statistics recorder can accept a registration of our histogram.
  void StartHistogrammer();

  // Add occurence of event to our histogram, so that we can see what is being
  // done in a specific MessageLoop instance (i.e., specific thread).
  // If message_histogram_ is NULL, this is a no-op.
  void HistogramEvent(int event);

  static TLSSlot tls_index_;
  static int strategy_selector_;
  static const LinearHistogram::DescriptionPair event_descriptions_[];
  static bool enable_histogrammer_;

  TimerManager timer_manager_;

  // A list of tasks that need to be processed by this instance.  Note that this
  // queue is only accessed (push/pop) by our current thread.
  // As an optimization, when we don't need to use the prioritization of
  // work_queue_, we use a null terminated list (TaskQueue) as our
  // implementation of the queue. This saves on memory (list uses pointers
  // internal to Task) and probably runs faster than the priority queue when
  // there was no real prioritization.
  OptionallyPrioritizedTaskQueue work_queue_;

  // A vector of objects (and corresponding watchers) that are routinely
  // serviced by this message loop's pump.
  std::vector<HANDLE> objects_;
  std::vector<Watcher*> watchers_;

  ObserverList<Observer> observers_;
  HWND message_hwnd_;
  IDMap<Task> timed_tasks_;
  // A recursion block that prevents accidentally running additonal tasks when
  // insider a (accidentally induced?) nested message pump.
  bool nestable_tasks_allowed_;

  bool exception_restoration_;

  Dispatcher* dispatcher_;
  bool quit_received_;
  bool quit_now_;

  std::string thread_name_;
  // A profiling histogram showing the counts of various messages and events.
  scoped_ptr<LinearHistogram> message_histogram_;

  // A null terminated list which creates an incoming_queue of tasks that are
  // aquired under a mutex for processing on this instance's thread. These tasks
  // have not yet been sorted out into items for our work_queue_ vs items that
  // will be handled by the TimerManager.
  TaskQueue incoming_queue_;
  // Protect access to incoming_queue_.
  Lock incoming_queue_lock_;

  // A null terminated list of non-nestable tasks that we had to delay because
  // when it came time to execute them we were in a nested message loop.  They
  // will execute once we're out of nested message loops.
  TaskQueue delayed_non_nestable_queue_;

  // Indicate if there is a kMsgPumpATask message pending in the Windows Message
  // queue.  There is at most one such message, and it can drive execution of
  // tasks when a native message pump is running.
  bool task_pump_message_pending_;
  // Protect access to task_pump_message_pending_.
  Lock task_pump_message_lock_;

  // Used to count how many Run() invocations are on the stack.
  int run_depth_;

  DISALLOW_EVIL_CONSTRUCTORS(MessageLoop);
};

#endif  // BASE_MESSAGE_LOOP_H__
