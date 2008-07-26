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

#ifndef CHROME_COMMON_TASK_QUEUE_H__
#define CHROME_COMMON_TASK_QUEUE_H__

#include <deque>

#include "base/task.h"

// A TaskQueue is a queue of tasks waiting to be run.  To run the tasks, call
// the Run method.  A task queue is itself a Task so that it can be placed in a
// message loop or another task queue.
class TaskQueue : public Task {
 public:
  TaskQueue();
  ~TaskQueue();

  // Run all the tasks in the queue.  New tasks pushed onto the queue during
  // a run will be run next time |Run| is called.
  virtual void Run();

  // Push the specified task onto the queue.  When the queue is run, the tasks
  // will be run in the order they are pushed.
  //
  // This method takes ownership of |task| and will delete task after it is run
  // (or when the TaskQueue is destroyed, if we never got a chance to run it).
  void Push(Task* task);

  // Remove all tasks from the queue.  The tasks are deleted.
  void Clear();

  // Returns true if this queue contains no tasks.
  bool Empty() const;

 private:
   // The list of tasks we are waiting to run.
   std::deque<Task*> queue_;
};

#endif  // CHROME_COMMON_TASK_QUEUE_H__
