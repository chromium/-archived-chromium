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

#include "base/call_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

int global_int = 0;

void SetGlobalInt5() {
  global_int = 5;
}
void SetGlobalInt(int x) {
  global_int = x;
}
void SetInt(int* p, int x) {
  *p = x;
}
void SetIntAdd2(int* p, int x, int y) {
  *p = x + y;
}
void SetIntAdd3(int* p, int x, int y, int z) {
  *p = x + y + z;
}
void SetIntAdd4(int* p, int x, int y, int z, int w) {
  *p = x + y + z + w;
}

}  // namespace

TEST(CallWrapperTest, FunctionCall) {
  // Function call with 0 arguments.
  {
    EXPECT_EQ(0, global_int);
    CallWrapper* wrapper = NewFunctionCallWrapper(SetGlobalInt5);

    EXPECT_EQ(0, global_int);
    wrapper->Run();
    EXPECT_EQ(5, global_int);
  }
  // Function call with 1 argument.
  {
    EXPECT_EQ(5, global_int);
    CallWrapper* wrapper = NewFunctionCallWrapper(SetGlobalInt, 0);

    EXPECT_EQ(5, global_int);
    wrapper->Run();
    EXPECT_EQ(0, global_int);
  }
  // Function call with 2 arguments.
  {
    int stack_int = 4;
    CallWrapper* wrapper;

    wrapper = NewFunctionCallWrapper(SetInt, &global_int, 8);
    EXPECT_EQ(4, stack_int);
    EXPECT_EQ(0, global_int);
    wrapper->Run();
    EXPECT_EQ(4, stack_int);
    EXPECT_EQ(8, global_int);

    wrapper = NewFunctionCallWrapper(SetInt, &stack_int, 8);
    EXPECT_EQ(4, stack_int);
    EXPECT_EQ(8, global_int);
    wrapper->Run();
    EXPECT_EQ(8, stack_int);
    EXPECT_EQ(8, global_int);
  }
  // Function call with 3-5 arguments.
  {
    int stack_int = 12;
    CallWrapper* wrapper;

    wrapper = NewFunctionCallWrapper(SetIntAdd2, &stack_int, 1, 6);
    EXPECT_EQ(12, stack_int);
    wrapper->Run();
    EXPECT_EQ(7, stack_int);

    wrapper = NewFunctionCallWrapper(SetIntAdd3, &stack_int, 1, 6, 2);
    EXPECT_EQ(7, stack_int);
    wrapper->Run();
    EXPECT_EQ(9, stack_int);

    wrapper = NewFunctionCallWrapper(SetIntAdd4, &stack_int, 1, 6, 2, 3);
    EXPECT_EQ(9, stack_int);
    wrapper->Run();
    EXPECT_EQ(12, stack_int);
  }
}

namespace {

class Incrementer {
 public:
  Incrementer(int* ptr) : ptr_(ptr) { }
  void IncrementBy(int x) { *ptr_ += x; }
  void Increment() { IncrementBy(1); }
  void SetIntAdd2(int x, int y) { *ptr_ = x + y; }
  void SetIntAdd3(int x, int y, int z) { *ptr_ = x + y + z; }
  void SetIntAdd4(int x, int y, int z, int w) { *ptr_ = x + y + z + w; }
 private:
  int* ptr_;
};

}  // namespace

TEST(CallWrapperTest, MethodCall) {
  // Method call with 0 and 1 arguments.
  {
    int stack_int = 0;
    Incrementer incr(&stack_int);
    CallWrapper* wrapper;

    wrapper = NewMethodCallWrapper(&incr, &Incrementer::Increment);
    EXPECT_EQ(0, stack_int);
    wrapper->Run();
    EXPECT_EQ(1, stack_int);

    wrapper = NewMethodCallWrapper(&incr, &Incrementer::IncrementBy, 10);
    EXPECT_EQ(1, stack_int);
    wrapper->Run();
    EXPECT_EQ(11, stack_int);
  }
  // Method call with 2-5 arguments.
  {
    int stack_int = 0;
    Incrementer incr(&stack_int);
    CallWrapper* wrapper;

    wrapper = NewMethodCallWrapper(&incr, &Incrementer::SetIntAdd2, 1, 5);
    EXPECT_EQ(0, stack_int);
    wrapper->Run();
    EXPECT_EQ(6, stack_int);

    wrapper = NewMethodCallWrapper(&incr, &Incrementer::SetIntAdd3, 1, 5, 7);
    EXPECT_EQ(6, stack_int);
    wrapper->Run();
    EXPECT_EQ(13, stack_int);

    wrapper = NewMethodCallWrapper(&incr, &Incrementer::SetIntAdd4, 1, 5, 7, 2);
    EXPECT_EQ(13, stack_int);
    wrapper->Run();
    EXPECT_EQ(15, stack_int);
  }
}
