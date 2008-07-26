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

#include "chrome/common/stl_util-inl.h"

#include "chrome/common/task_queue.h"

TaskQueue::TaskQueue() {
}

TaskQueue::~TaskQueue() {
  // We own all the pointes in |queue_|.  It is our job to delete them.
  STLDeleteElements(&queue_);
}

void TaskQueue::Run() {
  // Nothing to run if our queue is empty.
  if (queue_.empty())
    return;

  std::deque<Task*> ready;
  queue_.swap(ready);

  // Run the tasks that are ready.
  std::deque<Task*>::const_iterator task;
  for (task = ready.begin(); task != ready.end(); ++task) {
    // Run the task and then delete it.
    (*task)->Run();
    delete (*task);
  }
}

void TaskQueue::Push(Task* task) {
  // Add the task to the back of the queue.
  queue_.push_back(task);
}

void TaskQueue::Clear() {
  // Delete all the elements in the queue and clear the dead pointers.
  STLDeleteElements(&queue_);
}

bool TaskQueue::Empty() const {
  return queue_.empty();
}
