/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file implements unit tests for class Counter.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/counter.h"
#include "core/cross/timer.h"

namespace o3d {

class CounterTest : public testing::Test {
 protected:

  CounterTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  CounterManager* counter_manager() { return counter_manager_; }
  Pack* pack() { return pack_; }

 private:

  ServiceDependency<ObjectManager> object_manager_;
  CounterManager *counter_manager_;
  Pack* pack_;
};

void CounterTest::SetUp() {
  counter_manager_ = new CounterManager(g_service_locator);
  pack_ = object_manager_->CreatePack();
}

void CounterTest::TearDown() {
  pack_->Destroy();
  delete counter_manager_;
}

// Tests the creation of a Counter.
TEST_F(CounterTest, Basic) {
  Counter* counter = pack()->Create<Counter>();
  ASSERT_TRUE(counter != NULL);

  // Check that its default params got created.
  EXPECT_TRUE(
      counter->GetParam<ParamBoolean>(Counter::kRunningParamName) != NULL);
  EXPECT_TRUE(
      counter->GetParam<ParamBoolean>(Counter::kForwardParamName) != NULL);
  EXPECT_TRUE(
      counter->GetParam<ParamInteger>(Counter::kCountModeParamName) != NULL);
  EXPECT_TRUE(
      counter->GetParam<ParamFloat>(Counter::kStartParamName) != NULL);
  EXPECT_TRUE(
      counter->GetParam<ParamFloat>(Counter::kEndParamName) != NULL);
  EXPECT_TRUE(
      counter->GetParam<ParamFloat>(Counter::kCountParamName) != NULL);
  EXPECT_TRUE(
      counter->GetParam<ParamFloat>(Counter::kMultiplierParamName) != NULL);

  // Check that the defaults are correct
  EXPECT_EQ(counter->forward(), true);
  EXPECT_EQ(counter->running(), true);
  EXPECT_EQ(counter->count_mode(), Counter::CONTINUOUS);
  EXPECT_EQ(counter->count(), 0.0f);
  EXPECT_EQ(counter->start(), 0.0f);
  EXPECT_EQ(counter->end(), 0.0f);
  EXPECT_EQ(counter->multiplier(), 1.0f);
}

// Tests Counter::Advance
TEST_F(CounterTest, Advance) {
  Counter* counter = pack()->Create<Counter>();
  ASSERT_TRUE(counter != NULL);
  Counter::CounterCallbackQueue queue;

  counter->set_start(1.0f);
  counter->set_end(5.0f);
  counter->Reset();

  // See that it advances.
  EXPECT_EQ(counter->count(), 1.0f);
  counter->Advance(1.0f, &queue);
  EXPECT_EQ(counter->count(), 2.0f);

  // See that forward works.
  counter->set_forward(false);
  counter->Advance(1.0f, &queue);
  EXPECT_EQ(counter->count(), 1.0f);
  counter->set_forward(true);

  // Check that CONTINUOUS works.
  counter->Advance(10.0f, &queue);
  EXPECT_EQ(counter->count(), 11.0f);
  // Check backward as well.
  counter->Advance(-20.0f, &queue);
  EXPECT_EQ(counter->count(), -9.0f);

  // Check that ONCE works.
  counter->Reset();
  counter->set_running(true);
  counter->set_count_mode(Counter::ONCE);
  counter->Advance(10.0f, &queue);
  EXPECT_EQ(counter->count(), counter->end());
  EXPECT_EQ(counter->running(), false);
  // Check backward as well.
  counter->set_running(true);
  counter->Advance(-10.0f, &queue);
  EXPECT_EQ(counter->count(), counter->start());
  EXPECT_EQ(counter->running(), false);
  // Check bounds end.
  counter->SetCount(counter->end());
  counter->set_running(true);
  counter->Advance(1.0f, &queue);
  EXPECT_EQ(counter->count(), counter->end());
  EXPECT_EQ(counter->running(), false);
  // Check bounds start
  counter->SetCount(counter->start());
  counter->set_running(true);
  counter->Advance(-1.0f, &queue);
  EXPECT_EQ(counter->count(), counter->start());
  EXPECT_EQ(counter->running(), false);

  // Check that CYCLE works.
  counter->Reset();
  counter->set_count_mode(Counter::CYCLE);
  counter->Advance(4.0f, &queue);
  EXPECT_EQ(counter->count(), counter->start());
  counter->Advance(5.0f, &queue);
  EXPECT_EQ(counter->count(), counter->start() + 1.0f);
  counter->Advance(-4.0f, &queue);
  EXPECT_EQ(counter->count(), counter->start() + 1.0f);

  // Check that OSCILLATE works.
  counter->Reset();
  counter->set_count_mode(Counter::OSCILLATE);
  counter->Advance(5.0f, &queue);
  EXPECT_EQ(counter->count(), counter->start() + 3.0f);
  EXPECT_EQ(counter->forward(), false);
  counter->Advance(4.0f, &queue);
  EXPECT_EQ(counter->count(), counter->start() + 1.0f);
  EXPECT_EQ(counter->forward(), true);
  counter->Advance(-3.0f, &queue);
  EXPECT_EQ(counter->count(), counter->start() + 2.0f);
  EXPECT_EQ(counter->forward(), false);
  counter->Advance(-4.0f, &queue);
  EXPECT_EQ(counter->count(), counter->start() + 2.0f);
  EXPECT_EQ(counter->forward(), true);
}

// Tests Counter::Reset
TEST_F(CounterTest, Reset) {
  Counter* counter = pack()->Create<SecondCounter>();
  ASSERT_TRUE(counter != NULL);

  counter->set_start(20.0f);
  counter->set_end(40.0f);
  EXPECT_EQ(counter->count(), 0.0f);
  counter->Reset();
  EXPECT_EQ(counter->count(), 20.0f);
  counter->set_forward(false);
  counter->Reset();
  EXPECT_EQ(counter->count(), 40.0f);
}

namespace {

// A CounterCallback that tracks how many times it has been called
// and whether or not it has been deleted.
class TestCounterCallback : public Counter::CounterCallback {
 public:
  explicit TestCounterCallback(bool* deleted)
      : call_count_(0),
        deleted_(deleted) {
    *deleted_ = false;
  }

  ~TestCounterCallback() {
    *deleted_ = true;
  }

  virtual void Run() {
    ++call_count_;
  }

  int call_count() {
    return call_count_;
  }

  void Reset() {
    call_count_ = 0;
  }

 private:
  int call_count_;  // number of times this callback was called.
  bool* deleted_;  // pointer to bool to get set to true when deleted.

  DISALLOW_COPY_AND_ASSIGN(TestCounterCallback);
};

}  // anonymous namespace

// Tests Counter::AddCallback/RemoveCallback
TEST_F(CounterTest, AddRemoveGetCallback) {
  Counter* counter = pack()->Create<Counter>();
  ASSERT_TRUE(counter != NULL);

  const Counter::CounterCallbackInfoArray& counters = counter->GetCallbacks();
  Counter::CounterCallbackInfoArray::const_iterator iter;

  bool test1_deleted;
  bool test2_deleted;
  TestCounterCallback* test1 = new TestCounterCallback(&test1_deleted);
  TestCounterCallback* test2 = new TestCounterCallback(&test2_deleted);
  ASSERT_TRUE(test1 != NULL);
  ASSERT_TRUE(test2 != NULL);
  ASSERT_FALSE(test1_deleted);
  ASSERT_FALSE(test2_deleted);

  // Testing Adding
  counter->AddCallback(2.0f, test2);
  counter->AddCallback(1.0f, test1);
  // Check that both got added.
  ASSERT_EQ(counters.size(), 2);
  iter = counters.begin();
  EXPECT_EQ(iter->count(), 1.0f);
  ++iter;
  EXPECT_EQ(iter->count(), 2.0f);

  // Check that adding one at the same time removes the other
  counter->AddCallback(2.0f, test1);
  EXPECT_TRUE(test2_deleted);
  ASSERT_EQ(counters.size(), 2);

  // Check that removing a non-existant callback fails.
  EXPECT_FALSE(counter->RemoveCallback(3.0f));

  // Check that removing one of the duplicated callbacks does not remove the
  // other.
  EXPECT_TRUE(counter->RemoveCallback(1.0f));
  ASSERT_EQ(counters.size(), 1);
  EXPECT_FALSE(test1_deleted);

  // Check that removing final callback deletes the callback.
  EXPECT_TRUE(counter->RemoveCallback(2.0f));
  ASSERT_EQ(counters.size(), 0);
  EXPECT_TRUE(test1_deleted);
  EXPECT_FALSE(counter->RemoveCallback(2.0f));

  // Check that callbacks get called correct number of times.
  test1 = new TestCounterCallback(&test1_deleted);
  test2 = new TestCounterCallback(&test2_deleted);
  bool test3_deleted;
  bool test4_deleted;
  bool test5_deleted;
  TestCounterCallback* test3 = new TestCounterCallback(&test3_deleted);
  TestCounterCallback* test4 = new TestCounterCallback(&test4_deleted);
  TestCounterCallback* test5 = new TestCounterCallback(&test5_deleted);
  ASSERT_TRUE(test1 != NULL);
  ASSERT_TRUE(test2 != NULL);
  ASSERT_FALSE(test1_deleted);
  ASSERT_FALSE(test2_deleted);
  counter->set_start(1.0f);
  counter->set_end(5.0f);
  counter->AddCallback(0.0f, test1);  // before start
  counter->AddCallback(1.0f, test2);  // at start
  counter->AddCallback(2.0f, test3);  // in between
  counter->AddCallback(5.0f, test4);  // end end
  counter->AddCallback(6.0f, test5);  // after end.

  Counter::CounterCallbackQueue queue;

  // Test CONTINUOUS
  counter->Reset();
  counter->Advance(0.0f, &queue);  // At 1
  EXPECT_EQ(counter->count(), 1.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 0);
  EXPECT_EQ(test3->call_count(), 0);
  EXPECT_EQ(test4->call_count(), 0);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(1.0f, &queue);  // At 2
  EXPECT_EQ(counter->count(), 2.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 1);
  EXPECT_EQ(test3->call_count(), 1);
  EXPECT_EQ(test4->call_count(), 0);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(3.0f, &queue);  // At 5
  EXPECT_EQ(counter->count(), 5.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 1);
  EXPECT_EQ(test3->call_count(), 1);
  EXPECT_EQ(test4->call_count(), 1);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(3.0f, &queue);  // At 8
  EXPECT_EQ(counter->count(), 8.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 1);
  EXPECT_EQ(test3->call_count(), 1);
  EXPECT_EQ(test4->call_count(), 1);
  EXPECT_EQ(test5->call_count(), 1);
  counter->Advance(-3.0f, &queue);  // At 5
  EXPECT_EQ(counter->count(), 5.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 1);
  EXPECT_EQ(test3->call_count(), 1);
  EXPECT_EQ(test4->call_count(), 2);
  EXPECT_EQ(test5->call_count(), 2);
  counter->Advance(-3.0f, &queue);  // At 2
  EXPECT_EQ(counter->count(), 2.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 1);
  EXPECT_EQ(test3->call_count(), 2);
  EXPECT_EQ(test4->call_count(), 2);
  EXPECT_EQ(test5->call_count(), 2);
  counter->Advance(-2.0f, &queue);  // At 0
  EXPECT_EQ(counter->count(), 0.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 1);
  EXPECT_EQ(test2->call_count(), 2);
  EXPECT_EQ(test3->call_count(), 2);
  EXPECT_EQ(test4->call_count(), 2);
  EXPECT_EQ(test5->call_count(), 2);

  // Test ONCE
  test1->Reset();
  test2->Reset();
  test3->Reset();
  test4->Reset();
  test5->Reset();
  counter->Reset();
  counter->set_count_mode(Counter::ONCE);
  counter->Advance(0.0f, &queue);  // At 1
  EXPECT_EQ(counter->count(), 1.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 0);
  EXPECT_EQ(test3->call_count(), 0);
  EXPECT_EQ(test4->call_count(), 0);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(10.0f, &queue);  // At 5
  EXPECT_EQ(counter->count(), 5.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 1);
  EXPECT_EQ(test3->call_count(), 1);
  EXPECT_EQ(test4->call_count(), 1);
  EXPECT_EQ(test5->call_count(), 0);
  counter->set_forward(false);
  counter->Advance(10.0f, &queue);  // At 1
  EXPECT_EQ(counter->count(), 1.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 2);
  EXPECT_EQ(test3->call_count(), 2);
  EXPECT_EQ(test4->call_count(), 2);
  EXPECT_EQ(test5->call_count(), 0);

  // Test CYCLE
  test1->Reset();
  test2->Reset();
  test3->Reset();
  test4->Reset();
  test5->Reset();
  counter->set_forward(true);
  counter->Reset();
  counter->set_count_mode(Counter::CYCLE);
  counter->Advance(4.0f, &queue);  // At 1.0 (1 cycle)
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 1);
  EXPECT_EQ(test3->call_count(), 1);
  EXPECT_EQ(test4->call_count(), 1);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(22.0f, &queue);  // At 3.0 (5 cycles + 2)
  EXPECT_EQ(counter->count(), 3.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 7);
  EXPECT_EQ(test3->call_count(), 7);
  EXPECT_EQ(test4->call_count(), 6);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(-21.0f, &queue);  // At 2.0 (5 cycles + 1)
  EXPECT_EQ(counter->count(), 2.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 12);
  EXPECT_EQ(test3->call_count(), 13);
  EXPECT_EQ(test4->call_count(), 11);
  EXPECT_EQ(test5->call_count(), 0);

  // Test OSCILLATE
  test1->Reset();
  test2->Reset();
  test3->Reset();
  test4->Reset();
  test5->Reset();
  counter->Reset();
  counter->set_forward(true);
  counter->set_count_mode(Counter::OSCILLATE);
  counter->Advance(22.0f, &queue);  // At 3.0 (5 cycles)
  EXPECT_EQ(counter->count(), 3.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 5);
  EXPECT_EQ(test3->call_count(), 5);
  EXPECT_EQ(test4->call_count(), 6);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(-21.0f, &queue);  // At 2.0 (5 cycles)
  EXPECT_EQ(counter->count(), 2.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 9);
  EXPECT_EQ(test3->call_count(), 10);
  EXPECT_EQ(test4->call_count(), 12);
  EXPECT_EQ(test5->call_count(), 0);

  // Check start < end case ----------------------------

  counter->set_start(5.0f);
  counter->set_end(1.0f);

  // Test CONTINUOUS
  test1->Reset();
  test2->Reset();
  test3->Reset();
  test4->Reset();
  test5->Reset();
  counter->set_count_mode(Counter::CONTINUOUS);
  counter->Reset();
  counter->Advance(0.0f, &queue);  // At 5
  EXPECT_EQ(counter->count(), 5.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 0);
  EXPECT_EQ(test3->call_count(), 0);
  EXPECT_EQ(test4->call_count(), 0);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(1.0f, &queue);  // At 4
  EXPECT_EQ(counter->count(), 4.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 0);
  EXPECT_EQ(test3->call_count(), 0);
  EXPECT_EQ(test4->call_count(), 1);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(3.0f, &queue);  // At 1
  EXPECT_EQ(counter->count(), 1.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 1);
  EXPECT_EQ(test3->call_count(), 1);
  EXPECT_EQ(test4->call_count(), 1);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(3.0f, &queue);  // At -2
  EXPECT_EQ(counter->count(), -2.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 1);
  EXPECT_EQ(test2->call_count(), 1);
  EXPECT_EQ(test3->call_count(), 1);
  EXPECT_EQ(test4->call_count(), 1);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(-3.0f, &queue);  // At 1
  EXPECT_EQ(counter->count(), 1.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 2);
  EXPECT_EQ(test2->call_count(), 2);
  EXPECT_EQ(test3->call_count(), 1);
  EXPECT_EQ(test4->call_count(), 1);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(-3.0f, &queue);  // At 4
  EXPECT_EQ(counter->count(), 4.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 2);
  EXPECT_EQ(test2->call_count(), 2);
  EXPECT_EQ(test3->call_count(), 2);
  EXPECT_EQ(test4->call_count(), 1);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(-2.0f, &queue);  // At 6
  EXPECT_EQ(counter->count(), 6.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 2);
  EXPECT_EQ(test2->call_count(), 2);
  EXPECT_EQ(test3->call_count(), 2);
  EXPECT_EQ(test4->call_count(), 2);
  EXPECT_EQ(test5->call_count(), 1);

  // Test ONCE
  test1->Reset();
  test2->Reset();
  test3->Reset();
  test4->Reset();
  test5->Reset();
  counter->Reset();
  counter->set_count_mode(Counter::ONCE);
  counter->Advance(0.0f, &queue);  // At 5
  EXPECT_EQ(counter->count(), 5.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 0);
  EXPECT_EQ(test3->call_count(), 0);
  EXPECT_EQ(test4->call_count(), 0);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(10.0f, &queue);  // At 1
  EXPECT_EQ(counter->count(), 1.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 1);
  EXPECT_EQ(test3->call_count(), 1);
  EXPECT_EQ(test4->call_count(), 1);
  EXPECT_EQ(test5->call_count(), 0);
  counter->set_forward(false);
  counter->Advance(10.0f, &queue);  // At 5
  EXPECT_EQ(counter->count(), 5.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 2);
  EXPECT_EQ(test3->call_count(), 2);
  EXPECT_EQ(test4->call_count(), 2);
  EXPECT_EQ(test5->call_count(), 0);

  // Test CYCLE
  test1->Reset();
  test2->Reset();
  test3->Reset();
  test4->Reset();
  test5->Reset();
  counter->Reset();
  counter->set_forward(true);
  counter->set_count_mode(Counter::CYCLE);
  counter->Advance(22.0f, &queue);  // At 3.0 (5 cycles + 2)
  EXPECT_EQ(counter->count(), 3.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 5);
  EXPECT_EQ(test3->call_count(), 5);
  EXPECT_EQ(test4->call_count(), 6);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(-21.0f, &queue);  // At 4.0 (5 cycles + 1)
  EXPECT_EQ(counter->count(), 4.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 10);
  EXPECT_EQ(test3->call_count(), 10);
  EXPECT_EQ(test4->call_count(), 11);
  EXPECT_EQ(test5->call_count(), 0);

  // Test OSCILLATE
  test1->Reset();
  test2->Reset();
  test3->Reset();
  test4->Reset();
  test5->Reset();
  counter->Reset();
  counter->set_forward(true);
  counter->set_count_mode(Counter::OSCILLATE);
  counter->Advance(22.0f, &queue);  // At 3.0 (5 cycles)
  EXPECT_EQ(counter->count(), 3.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 6);
  EXPECT_EQ(test3->call_count(), 6);
  EXPECT_EQ(test4->call_count(), 5);
  EXPECT_EQ(test5->call_count(), 0);
  counter->Advance(-21.0f, &queue);  // At 4.0 (5 cycles)
  EXPECT_EQ(counter->count(), 4.0f);
  queue.CallCounterCallbacks();
  EXPECT_EQ(test1->call_count(), 0);
  EXPECT_EQ(test2->call_count(), 12);
  EXPECT_EQ(test3->call_count(), 12);
  EXPECT_EQ(test4->call_count(), 9);
  EXPECT_EQ(test5->call_count(), 0);

  // Check RemoveAllCallbacks.
  counter->RemoveAllCallbacks();
  EXPECT_EQ(counters.size(), 0);
  EXPECT_TRUE(test1_deleted);
  EXPECT_TRUE(test2_deleted);
  EXPECT_TRUE(test3_deleted);
  EXPECT_TRUE(test4_deleted);
  EXPECT_TRUE(test5_deleted);
}

// Tests SecondCounter.
TEST_F(CounterTest, SecondCounter) {
  SecondCounter* counter = pack()->Create<SecondCounter>();
  ASSERT_TRUE(counter != NULL);

  EXPECT_EQ(0.0f, counter->count());
  counter_manager()->AdvanceCounters(2.0f, 1.0f);
  EXPECT_EQ(1.0f, counter->count());
}

// Tests RenderFrameCounter.
TEST_F(CounterTest, RenderFrameCounter) {
  RenderFrameCounter* counter = pack()->Create<RenderFrameCounter>();
  ASSERT_TRUE(counter != NULL);

  EXPECT_EQ(0.0f, counter->count());
  counter_manager()->AdvanceRenderFrameCounters(1.0f);
  EXPECT_EQ(1.0f, counter->count());
}

// Tests TickCounter.
TEST_F(CounterTest, TickCounter) {
  TickCounter* counter = pack()->Create<TickCounter>();
  ASSERT_TRUE(counter != NULL);

  EXPECT_EQ(0.0f, counter->count());
  counter_manager()->AdvanceCounters(1.0f, 2.0f);
  EXPECT_EQ(1.0f, counter->count());
}

}  // namespace o3d
