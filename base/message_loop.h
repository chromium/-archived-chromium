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

#ifndef BASE_MESSAGE_LOOP_H_
#define BASE_MESSAGE_LOOP_H_

#include <deque>
#include <queue>
#include <string>
#include <vector>

#include "base/histogram.h"
#include "base/message_pump.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "base/timer.h"
#include "base/thread_local_storage.h"

#if defined(OS_WIN)
// We need this to declare base::MessagePumpWin::Dispatcher, which we should
// really just eliminate.
#include "base/message_pump_win.h"
#endif

// A MessageLoop is used to process events for a particular thread.  There is
// at most one MessageLoop instance per thread.
//
// Events include at a minimum Task instances submitted to PostTask or those
// managed by TimerManager.  Depending on the type of message pump used by the
// MessageLoop other events such as UI messages may be processed.  On Windows
// APC calls (as time permits) and signals sent to a registered set of HANDLEs
// may also be processed.
//
// NOTE: Unless otherwise specified, a MessageLoop's methods may only be called
// on the thread where the MessageLoop's Run method executes.
//
// NOTE: MessageLoop has task reentrancy protection.  This means that if a
// task is being processed, a second task cannot start until the first task is
// finished.  Reentrancy can happen when processing a task, and an inner
// message pump is created.  That inner pump then processes native messages
// which could implicitly start an inner task.  Inner message pumps are created
// with dialogs (DialogBox), common dialogs (GetOpenFileName), OLE functions
// (DoDragDrop), printer functions (StartDoc) and *many* others.
//
// Sample workaround when inner task processing is needed:
//   bool old_state = MessageLoop::current()->NestableTasksAllowed();
//   MessageLoop::current()->SetNestableTasksAllowed(true);
//   HRESULT hr = DoDragDrop(...); // Implicitly runs a modal message loop here.
//   MessageLoop::current()->SetNestableTasksAllowed(old_state);
//   // Process hr  (the result returned by DoDragDrop().
//
// Please be SURE your task is reentrant (nestable) and all global variables
// are stable and accessible before calling SetNestableTasksAllowed(true).
//
class MessageLoop : public base::MessagePump::Delegate {
 public:
  static void EnableHistogrammer(bool enable_histogrammer);

  // A DestructionObserver is notified when the current MessageLoop is being
  // destroyed.  These obsevers are notified prior to MessageLoop::current()
  // being changed to return NULL.  This gives interested parties the chance to
  // do final cleanup that depends on the MessageLoop.
  //
  // NOTE: Any tasks posted to the MessageLoop during this notification will
  // not be run.  Instead, they will be deleted.
  //
  class DestructionObserver {
   public:
    virtual ~DestructionObserver() {}
    virtual void WillDestroyCurrentMessageLoop() = 0;
  };

  // Add a DestructionObserver, which will start receiving notifications
  // immediately.
  void AddDestructionObserver(DestructionObserver* destruction_observer);

  // Remove a DestructionObserver.  It is safe to call this method while a
  // DestructionObserver is receiving a notification callback.
  void RemoveDestructionObserver(DestructionObserver* destruction_observer);

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

  // Run the message loop.
  void Run();

  // Process all pending tasks, windows messages, etc., but don't wait/sleep.
  // Return as soon as all items that can be run are taken care of.
  void RunAllPending();

  // Signals the Run method to return after it is done processing all pending
  // messages.  This method may only be called on the same thread that called
  // Run, and Run must still be on the call stack.
  //
  // Use QuitTask if you need to Quit another thread's MessageLoop, but note
  // that doing so is fairly dangerous if the target thread makes nested calls
  // to MessageLoop::Run.  The problem being that you won't know which nested
  // run loop you are quiting, so be careful!
  //
  void Quit();

  // Invokes Quit on the current MessageLoop when run.  Useful to schedule an
  // arbitrary MessageLoop to Quit.
  class QuitTask : public Task {
   public:
    virtual void Run() {
      MessageLoop::current()->Quit();
    }
  };

  // Normally, it is not necessary to instantiate a MessageLoop.  Instead, it
  // is typical to make use of the current thread's MessageLoop instance.
  MessageLoop();
  ~MessageLoop();

  // Optional call to connect the thread name with this loop.
  void set_thread_name(const std::string& thread_name) {
    DCHECK(thread_name_.empty()) << "Should not rename this thread!";
    thread_name_ = thread_name;
  }
  const std::string& thread_name() const { return thread_name_; }

  // Returns the MessageLoop object for the current thread, or null if none.
  static MessageLoop* current() {
    return static_cast<MessageLoop*>(tls_index_.Get());
  }

  // Returns the TimerManager object for the current thread.
  TimerManager* timer_manager() { return &timer_manager_; }

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

  //----------------------------------------------------------------------------
#if defined(OS_WIN)
  // Backwards-compat for the old Windows-specific MessageLoop API.  These APIs
  // are deprecated.

  typedef base::MessagePumpWin::Dispatcher Dispatcher;
  typedef base::MessagePumpWin::Observer Observer;
  typedef base::MessagePumpWin::Watcher Watcher;

  void Run(Dispatcher* dispatcher);

  void WatchObject(HANDLE object, Watcher* watcher) {
    pump_win()->WatchObject(object, watcher);
  }
  void AddObserver(Observer* observer) {
    pump_win()->AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) {
    pump_win()->RemoveObserver(observer);
  }
  void WillProcessMessage(const MSG& message) {
    pump_win()->WillProcessMessage(message);
  }
  void DidProcessMessage(const MSG& message) {
    pump_win()->DidProcessMessage(message);
  }
  void PumpOutPendingPaintMessages() {
    pump_win()->PumpOutPendingPaintMessages();
  }
#endif  // defined(OS_WIN)

  //----------------------------------------------------------------------------
 private:
  friend class TimerManager;  // So it can call DidChangeNextTimerExpiry

  struct RunState {
    // Used to count how many Run() invocations are on the stack.
    int run_depth;

    // Used to record that Quit() was called, or that we should quit the pump
    // once it becomes idle.
    bool quit_received;

#if defined(OS_WIN)
    base::MessagePumpWin::Dispatcher* dispatcher;
#endif
  };

  class AutoRunState : RunState {
   public:
    AutoRunState(MessageLoop* loop);
    ~AutoRunState();
   private:
    MessageLoop* loop_;
    RunState* previous_state_;
  };

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
      Task* task() const { return task_; }
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

#if defined(OS_WIN)
  base::MessagePumpWin* pump_win() {
    return static_cast<base::MessagePumpWin*>(pump_.get());
  }
#endif

  // A function to encapsulate all the exception handling capability in the
  // stacks around the running of a main message loop.  It will run the message
  // loop in a SEH try block or not depending on the set_SEH_restoration()
  // flag.
  void RunHandler();

  // A surrounding stack frame around the running of the message loop that
  // supports all saving and restoring of state, as is needed for any/all (ugly)
  // recursive calls.
  void RunInternal();

  // Called to process any delayed non-nestable tasks.
  bool ProcessNextDelayedNonNestableTask();

  //----------------------------------------------------------------------------
  // Run a work_queue_ task or new_task, and delete it (if it was processed by
  // PostTask). If there are queued tasks, the oldest one is executed and
  // new_task is queued. new_task is optional and can be NULL. In this NULL
  // case, the method will run one pending task (if any exist). Returns true if
  // it executes a task.  Queued tasks accumulate only when there is a
  // non-nestable task currently processing, in which case the new_task is
  // appended to the list work_queue_.  Such re-entrancy generally happens when
  // an unrequested message pump (typical of a native dialog) is executing in
  // the context of a task.
  bool QueueOrRunTask(Task* new_task);

  // Runs the specified task and deletes it.
  void RunTask(Task* task);

  // Make state adjustments just before and after running tasks so that we can
  // continue to work if a native message loop is employed during a task.
  void BeforeTaskRunSetup();
  void AfterTaskRunRestore();

  // Load tasks from the incoming_queue_ into work_queue_ if the latter is
  // empty.  The former requires a lock to access, while the latter is directly
  // accessible on this thread.
  void ReloadWorkQueue();

  // Delete tasks that haven't run yet without running them.  Used in the
  // destructor to make sure all the task's destructors get called.
  void DeletePendingTasks();

  // Post a task to our incomming queue.
  void PostTaskInternal(Task* task);

  // Called by the TimerManager when its next timer changes.
  void DidChangeNextTimerExpiry();

  // Entry point for TimerManager to request the Run() of a task.  If we
  // created the task during an PostTask(FROM_HERE, ), then we will also
  // perform destructions, and we'll have the option of queueing the task.  If
  // we didn't create the timer, then we will Run it immediately.
  bool RunTimerTask(Timer* timer);

  // Since some Timer's are owned by MessageLoop, the TimerManager (when it is
  // being destructed) passses us the timers to discard (without doing a Run()).
  void DiscardTimer(Timer* timer);

  // base::MessagePump::Delegate methods:
  virtual bool DoWork();
  virtual bool DoDelayedWork(Time* next_delayed_work_time);
  virtual bool DoIdleWork();

  // Start recording histogram info about events and action IF it was enabled
  // and IF the statistics recorder can accept a registration of our histogram.
  void StartHistogrammer();

  // Add occurence of event to our histogram, so that we can see what is being
  // done in a specific MessageLoop instance (i.e., specific thread).
  // If message_histogram_ is NULL, this is a no-op.
  void HistogramEvent(int event);

  static TLSSlot tls_index_;
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

  scoped_refptr<base::MessagePump> pump_;

  ObserverList<DestructionObserver> destruction_observers_;
  // A recursion block that prevents accidentally running additonal tasks when
  // insider a (accidentally induced?) nested message pump.
  bool nestable_tasks_allowed_;

  bool exception_restoration_;

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

  RunState* state_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoop);
};

#endif  // BASE_MESSAGE_LOOP_H_
