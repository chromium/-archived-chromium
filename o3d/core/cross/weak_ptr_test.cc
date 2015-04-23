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


#include "core/cross/weak_ptr.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

namespace {

// A simple testing helper class that tracks the number of live instances of
// itself.
class InstanceCount : public RefCounted {
 public:
  typedef SmartPointer<InstanceCount> Ref;
  typedef WeakPointer<InstanceCount> WeakPointerType;

  // Use construction & destruction to inc/dec a static instance counter.
  InstanceCount() : weak_pointer_manager_(this) {
    ++instance_count_;
  }

  ~InstanceCount() {
    --instance_count_;
  }

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

  static int instance_count() {
    return instance_count_;
  }

 private:
  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  static int instance_count_;
};

int InstanceCount::instance_count_ = 0;

}  // anonymous namespace.

// Test case for the Weak pointer class
class WeakPtrTest : public testing::Test {
};

TEST_F(WeakPtrTest, Construct) {
  WeakPointer<InstanceCount> weak_ptr;
  ASSERT_TRUE(weak_ptr.Get() == NULL);
}

// Test the behaviour of a single weak pointer.
TEST_F(WeakPtrTest, SingleReference) {
  InstanceCount* raw_pointer = new InstanceCount();
  WeakPointer<InstanceCount> weak_ptr(raw_pointer->GetWeakPointer());

  // Validate that a single instance was created, and that the pointer
  // points to the correct instance.
  EXPECT_TRUE(weak_ptr.Get() == raw_pointer);
  EXPECT_EQ(InstanceCount::instance_count(), 1);

  // Validate we can release it and the weak pointer goes to NULL.
  delete raw_pointer;
  EXPECT_EQ(InstanceCount::instance_count(), 0);
  EXPECT_TRUE(weak_ptr.Get() == NULL);
}

// Validate the behaviour of multiple weak pointers.
TEST_F(WeakPtrTest, MultipleReferences) {
  InstanceCount* raw_ptr = new InstanceCount();
  WeakPointer<InstanceCount> weak_ptr1(raw_ptr->GetWeakPointer());
  WeakPointer<InstanceCount> weak_ptr2(raw_ptr->GetWeakPointer());

  // Validate that only a single instance was created.
  EXPECT_EQ(InstanceCount::instance_count(), 1);
  EXPECT_TRUE(weak_ptr1.Get() == raw_ptr);
  EXPECT_TRUE(weak_ptr1.Get() == weak_ptr2.Get());

  // Check that if we delete the object both weak pointers go NULL.
  delete raw_ptr;
  EXPECT_TRUE(weak_ptr1.Get() == NULL);
  EXPECT_TRUE(weak_ptr2.Get() == NULL);
  EXPECT_EQ(InstanceCount::instance_count(), 0);
}

// Check that assignment works..
TEST_F(WeakPtrTest, Assignment) {
  InstanceCount* raw_ptr = new InstanceCount();
  WeakPointer<InstanceCount> weak_ptr1;
  WeakPointer<InstanceCount> weak_ptr2;

  EXPECT_TRUE(weak_ptr1.Get() == NULL);
  EXPECT_TRUE(weak_ptr2.Get() == NULL);
  weak_ptr1 = raw_ptr->GetWeakPointer();
  weak_ptr2 = weak_ptr1;

  EXPECT_TRUE(weak_ptr1.Get() == raw_ptr);
  EXPECT_TRUE(weak_ptr1.Get() == weak_ptr2.Get());

  // Check that if we delete the object both weak pointers go NULL.
  delete raw_ptr;
  EXPECT_TRUE(weak_ptr1.Get() == NULL);
  EXPECT_TRUE(weak_ptr2.Get() == NULL);
  EXPECT_EQ(InstanceCount::instance_count(), 0);
}

// Validate the behaviour of self-assignment edge-case.
TEST_F(WeakPtrTest, SelfAssignment) {
  InstanceCount* raw_ptr = new InstanceCount();
  WeakPointer<InstanceCount> weak_ptr1(raw_ptr->GetWeakPointer());

  EXPECT_TRUE(weak_ptr1.Get() == raw_ptr);
  weak_ptr1 = weak_ptr1;
  EXPECT_TRUE(weak_ptr1.Get() == raw_ptr);
  delete raw_ptr;
  EXPECT_TRUE(weak_ptr1.Get() == NULL);
}

TEST_F(WeakPtrTest, EqualityTest) {
  InstanceCount* raw_ptr = new InstanceCount();
  WeakPointer<InstanceCount> weak_ptr1(raw_ptr->GetWeakPointer());
  WeakPointer<InstanceCount> weak_ptr2(raw_ptr->GetWeakPointer());

  // Validate that only a single instance was created.
  EXPECT_EQ(InstanceCount::instance_count(), 1);
  EXPECT_TRUE(weak_ptr1.Get() == raw_ptr);
  EXPECT_TRUE(weak_ptr1.Get() == weak_ptr2.Get());

  EXPECT_TRUE(weak_ptr1 == weak_ptr2);

  // Check that if we delete the object both weak pointers go NULL.
  delete raw_ptr;
  EXPECT_TRUE(weak_ptr1 == weak_ptr2);
}

}  // namespace o3d
