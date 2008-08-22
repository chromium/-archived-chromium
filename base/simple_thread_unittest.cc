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

#include <vector>

#include "base/call_wrapper.h"
#include "base/scoped_ptr.h"
#include "base/simple_thread.h"
#include "base/string_util.h"
#include "base/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void SetInt(int* p, int x) {
  *p = x;
}

void SignalEvent(base::WaitableEvent* event) {
  EXPECT_FALSE(event->IsSignaled());
  event->Signal();
  EXPECT_TRUE(event->IsSignaled());
}

}  // namespace

TEST(SimpleThreadTest, CreateAndJoin) {
  int stack_int = 0;

  CallWrapper* wrapper = NewFunctionCallWrapper(SetInt, &stack_int, 7);
  EXPECT_EQ(0, stack_int);
  scoped_ptr<base::SimpleThread> thread(
      new base::CallWrapperSimpleThread(wrapper));
  EXPECT_FALSE(thread->HasBeenStarted());
  EXPECT_FALSE(thread->HasBeenJoined());
  EXPECT_EQ(0, stack_int);

  thread->Start();
  EXPECT_TRUE(thread->HasBeenStarted());
  EXPECT_FALSE(thread->HasBeenJoined());

  thread->Join();
  EXPECT_TRUE(thread->HasBeenStarted());
  EXPECT_TRUE(thread->HasBeenJoined());
  EXPECT_EQ(7, stack_int);
}

TEST(SimpleThreadTest, WaitForEvent) {
  // Create a thread, and wait for it to signal us.
  base::WaitableEvent event(true, false);

  scoped_ptr<base::SimpleThread> thread(new base::CallWrapperSimpleThread(
      NewFunctionCallWrapper(SignalEvent, &event)));

  EXPECT_FALSE(event.IsSignaled());
  thread->Start();
  event.Wait();
  EXPECT_TRUE(event.IsSignaled());
  thread->Join();
}

TEST(SimpleThreadTest, Named) {
  base::WaitableEvent event(true, false);

  base::SimpleThread::Options options;
  scoped_ptr<base::SimpleThread> thread(new base::CallWrapperSimpleThread(
      NewFunctionCallWrapper(SignalEvent, &event), options, "testy"));
  EXPECT_EQ(thread->name_prefix(), "testy");
  EXPECT_FALSE(event.IsSignaled());

  thread->Start();
  EXPECT_EQ(thread->name_prefix(), "testy");
  EXPECT_EQ(thread->name(), std::string("testy/") + IntToString(thread->tid()));
  event.Wait();

  EXPECT_TRUE(event.IsSignaled());
  thread->Join();

  // We keep the name and tid, even after the thread is gone.
  EXPECT_EQ(thread->name_prefix(), "testy");
  EXPECT_EQ(thread->name(), std::string("testy/") + IntToString(thread->tid()));
}
