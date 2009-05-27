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


// This file contains the tests for class VisitorBase.

#include "core/cross/class_manager.h"
#include "core/cross/object_manager.h"
#include "core/cross/service_locator.h"
#include "core/cross/visitor_base.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

namespace {
class TestClass : public ObjectBase {
 public:
  explicit TestClass(ServiceLocator* service_locator)
      : ObjectBase(service_locator) {
  }
  static ObjectBase::Ref Create(ServiceLocator* service_locator) {
    return ObjectBase::Ref(new TestClass(service_locator));
  }
  O3D_DECL_CLASS(TestClass, ObjectBase);
};
O3D_DEFN_CLASS(TestClass, ObjectBase);

class TestVisitor : public VisitorBase<TestVisitor> {
 public:
  TestVisitor()
      : visit_object_base_calls_(0),
        visit_object_base_param_(NULL),
        visit_test_class_calls_(0),
        visit_test_class_param_(NULL) {
  }

  void VisitObjectBase(ObjectBase* param) {
    ++visit_object_base_calls_;
    visit_object_base_param_ = param;
  }

  void VisitTestClass(TestClass* param) {
    ++visit_test_class_calls_;
    visit_test_class_param_ = param;
  }

  int visit_object_base_calls_;
  ObjectBase* visit_object_base_param_;

  int visit_test_class_calls_;
  TestClass* visit_test_class_param_;
};
}

class VisitorBaseTest : public testing::Test {
 protected:
  virtual void SetUp() {
    service_locator_ = new ServiceLocator;
    class_manager_ = new ClassManager(service_locator_);
    class_manager_->AddTypedClass<TestClass>();
    object_manager_ = new ObjectManager(service_locator_);
  }

  virtual void TearDown() {
    delete object_manager_;
    delete class_manager_;
    delete service_locator_;
  }

  ServiceLocator* service_locator_;
  ObjectManager* object_manager_;
  ClassManager* class_manager_;
  Pack* pack_;
  TestVisitor visitor_;
};

TEST_F(VisitorBaseTest, AcceptShouldForwardToRegisteredFunction) {
  visitor_.Enable(&TestVisitor::VisitTestClass);
  ObjectBase::Ref object(class_manager_->CreateObjectByClass(
      TestClass::GetApparentClass()));
  visitor_.Accept(object);
  EXPECT_EQ(1, visitor_.visit_test_class_calls_);
  EXPECT_EQ(object.Get(), visitor_.visit_test_class_param_);
  EXPECT_EQ(0, visitor_.visit_object_base_calls_);
}

TEST_F(VisitorBaseTest, AcceptShouldDoNothingIfNoFunctionIsRegisteredForClass) {
  ObjectBase::Ref object(class_manager_->CreateObjectByClass(
      TestClass::GetApparentClass()));
  visitor_.Accept(object);
  EXPECT_EQ(0, visitor_.visit_test_class_calls_);
  EXPECT_EQ(0, visitor_.visit_object_base_calls_);
}

TEST_F(VisitorBaseTest,
    AcceptShouldForwardToRegisteredFunctionForParentIfClassNotRegistered) {
  visitor_.Enable(&TestVisitor::VisitObjectBase);
  ObjectBase::Ref object(class_manager_->CreateObjectByClass(
      TestClass::GetApparentClass()));
  visitor_.Accept(object);
  EXPECT_EQ(1, visitor_.visit_object_base_calls_);
  EXPECT_EQ(object.Get(), visitor_.visit_object_base_param_);
  EXPECT_EQ(0, visitor_.visit_test_class_calls_);
}

TEST_F(VisitorBaseTest, AcceptShouldDoNothingForNull) {
  visitor_.Accept(NULL);
  EXPECT_EQ(0, visitor_.visit_test_class_calls_);
  EXPECT_EQ(0, visitor_.visit_object_base_calls_);
}

TEST_F(VisitorBaseTest,
    VisitorBaseAcceptShouldForwardToRegisteredFunctionOnceForEachAccept) {
  visitor_.Enable(&TestVisitor::VisitTestClass);
  ObjectBase::Ref object(class_manager_->CreateObjectByClass(
      TestClass::GetApparentClass()));
  visitor_.Accept(object);
  visitor_.Accept(object);
  EXPECT_EQ(2, visitor_.visit_test_class_calls_);
  EXPECT_EQ(object.Get(), visitor_.visit_test_class_param_);
  EXPECT_EQ(0, visitor_.visit_object_base_calls_);
}

TEST_F(VisitorBaseTest, IsHandledReturnsTrueIfAFunctionIsRegistered) {
  visitor_.Enable(&TestVisitor::VisitTestClass);
  EXPECT_TRUE(visitor_.IsHandled(TestClass::GetApparentClass()));
}

TEST_F(VisitorBaseTest,
    IsHandledReturnsTrueIfAFunctionIsRegisteredForABaseClass) {
  visitor_.Enable(&TestVisitor::VisitObjectBase);
  EXPECT_TRUE(visitor_.IsHandled(TestClass::GetApparentClass()));
}

TEST_F(VisitorBaseTest, IsHandledReturnsFalseIfNoFunctionIsRegistered) {
  EXPECT_FALSE(visitor_.IsHandled(TestClass::GetApparentClass()));
}

}  // namespace o3d
