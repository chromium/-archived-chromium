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

#include "base/observer_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ObserverListTest : public testing::Test {
};

class Foo {
 public:
  virtual void Observe(int x) = 0;
};

class Adder : public Foo {
 public:
  Adder(int scaler) : scaler_(scaler), total(0) {}
  virtual void Observe(int x) {
    total += x * scaler_;
  }
  int total;
 private:
  int scaler_;
};

class Disrupter : public Foo {
 public:
  Disrupter(ObserverList<Foo>& list, Foo* doomed) : list_(list), doomed_(doomed) {
  }
  virtual void Observe(int x) {
    list_.RemoveObserver(doomed_);
  }
 private:
  ObserverList<Foo>& list_;
  Foo* doomed_;
};

}  // namespace

TEST(ObserverListTest, BasicTest) {
  ObserverList<Foo> observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter evil(observer_list, &c);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);

  FOR_EACH_OBSERVER(Foo, observer_list, Observe(10));

  observer_list.AddObserver(&evil);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  FOR_EACH_OBSERVER(Foo, observer_list, Observe(10));

  EXPECT_EQ(a.total, 20);
  EXPECT_EQ(b.total, -20);
  EXPECT_EQ(c.total, 0);
  EXPECT_EQ(d.total, -10);
}
