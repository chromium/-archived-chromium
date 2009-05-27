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


#include "core/cross/smart_ptr.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

// A simple testing helper class that tracks the number of live instances of
// itself.
class InstanceCount : public RefCounted {
 public:
  // Use construction & destruction to inc/dec a static instance counter.
  InstanceCount() { ++instance_count_; }
  ~InstanceCount() { --instance_count_; }

  static int instance_count() {
    return instance_count_;
  }

 private:
  static int instance_count_;
};

int InstanceCount::instance_count_ = 0;

// Test case for the smart pointer class
class SmartPtrTest : public testing::Test {
};

TEST_F(SmartPtrTest, Construct) {
  SmartPointer<InstanceCount> smart_ptr;
  ASSERT_TRUE(smart_ptr.Get() == NULL);
  ASSERT_TRUE(static_cast<InstanceCount*>(smart_ptr) == NULL);
}

// Test the behaviour of a single smart pointer without aliasing.
TEST_F(SmartPtrTest, SingleReference) {
  InstanceCount* raw_pointer = new InstanceCount();
  SmartPointer<InstanceCount> smart_ptr(raw_pointer);

  // Validate that a single instance was created, and that the pointer
  // points to the correct instance.
  EXPECT_TRUE(smart_ptr.Get() == raw_pointer);
  EXPECT_TRUE(static_cast<InstanceCount*>(smart_ptr) == raw_pointer);
  EXPECT_EQ(InstanceCount::instance_count(), 1);

  // Validate the release mechanism when the ref-count reaches 0.
  smart_ptr.Reset();
  EXPECT_EQ(InstanceCount::instance_count(), 0);
  EXPECT_TRUE(smart_ptr.Get() == NULL);
  EXPECT_TRUE(static_cast<InstanceCount*>(smart_ptr) == NULL);
}

// Validate the behaviour of multiple smart pointers aliasing each other.
TEST_F(SmartPtrTest, MultipleReferences) {
  SmartPointer<InstanceCount> smart_ptr(new InstanceCount());
  SmartPointer<InstanceCount> second_ref(smart_ptr);
  InstanceCount* raw_ptr = smart_ptr.Get();

  // Validate that only a single instance was created.
  EXPECT_EQ(InstanceCount::instance_count(), 1);
  EXPECT_TRUE(smart_ptr.Get() == second_ref.Get());

  // Ensure that removal of a single reference does not destroy the instance.
  smart_ptr.Reset();
  EXPECT_TRUE(smart_ptr.Get() == NULL);
  EXPECT_TRUE(second_ref.Get() == raw_ptr);
  EXPECT_EQ(InstanceCount::instance_count(), 1);

  // Validate the behaviour of the removal of the last reference.
  second_ref.Reset();
  EXPECT_TRUE(second_ref.Get() ==  NULL);
  EXPECT_EQ(InstanceCount::instance_count(), 0);
}

// Validate the relase behaviour of the smart pointer assignment operator.
TEST_F(SmartPtrTest, AssignmentRelease) {
  SmartPointer<InstanceCount> smart_ptr1(new InstanceCount());
  SmartPointer<InstanceCount> smart_ptr2(new InstanceCount());
  InstanceCount *raw_ptr1 = smart_ptr1.Get();
  InstanceCount *raw_ptr2 = smart_ptr2.Get();

  // Validate that assignment destroys the previous instance.
  smart_ptr2 = smart_ptr1;
  EXPECT_EQ(InstanceCount::instance_count(), 1);
  EXPECT_TRUE(smart_ptr2.Get() == smart_ptr1.Get());

  // Validate that assignment of one of the ptrs to NULL does not delete
  // the instance.
  smart_ptr2 = SmartPointer<InstanceCount>(NULL);
  EXPECT_EQ(InstanceCount::instance_count(), 1);

  // Assignment of the last instance to NULL should delete the instance.
  smart_ptr1 = smart_ptr2;
  EXPECT_EQ(InstanceCount::instance_count(), 0);
}

// Validate the behaviour of self-assignment edge-case.
TEST_F(SmartPtrTest, SelfAssignment) {
  SmartPointer<InstanceCount> smart_ptr1(new InstanceCount());
  smart_ptr1 = smart_ptr1;
  EXPECT_EQ(InstanceCount::instance_count(), 1);
}

TEST_F(SmartPtrTest, EqualityTest) {
  SmartPointer<InstanceCount> smart_ptr1(new InstanceCount());
  SmartPointer<InstanceCount> smart_ptr2(smart_ptr1);
  EXPECT_TRUE(smart_ptr1 == smart_ptr2);
}

// A testing helper class that exposes the add-ref and release methods of the
// RefCounted class.
class RefCountedHelper : public RefCounted {
 public:
  using RefCounted::AddRef;
  using RefCounted::Release;
};

class RefCountedTest : public testing::Test {
};

// Validate the behaviour of the reference-counting mechanism.
TEST_F(RefCountedTest, Basic) {
  RefCountedHelper instance;
  instance.AddRef();
  EXPECT_EQ(instance.Release(), 0);

  instance.AddRef();
  instance.AddRef();
  instance.AddRef();
  instance.AddRef();
  EXPECT_EQ(instance.Release(), 3);
}

}  // namespace o3d
