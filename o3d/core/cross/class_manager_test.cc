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


// This file implements unit tests for class ClassManager.

#include <algorithm>

#include "core/cross/class_manager.h"
#include "core/cross/object_manager.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

namespace {
class CreatableClass : public ObjectBase {
 public:
  explicit CreatableClass(ServiceLocator* service_locator)
      : ObjectBase(service_locator) {
  }
  static ObjectBase::Ref Create(ServiceLocator* service_locator) {
    return ObjectBase::Ref(new CreatableClass(service_locator));
  }
  O3D_DECL_CLASS(CreatableClass, ObjectBase)
};
O3D_DEFN_CLASS(CreatableClass, ObjectBase);

class NonCreatableClass : public ObjectBase {
 public:
  explicit NonCreatableClass(ServiceLocator* service_locator)
      : ObjectBase(service_locator) {
  }
  static ObjectBase::Ref Create(ServiceLocator* service_locator) {
    return ObjectBase::Ref();
  }
  O3D_DECL_CLASS(NonCreatableClass, ObjectBase)
};
O3D_DEFN_CLASS(NonCreatableClass, ObjectBase);
}

class ClassManagerTest : public testing::Test {
 protected:

  virtual void SetUp() {
    service_locator_ = new ServiceLocator;
    object_manager_ = new ObjectManager(service_locator_);
    class_manager_ = new ClassManager(service_locator_);
    class_manager_->AddTypedClass<CreatableClass>();
    class_manager_->AddTypedClass<NonCreatableClass>();
  }

  virtual void TearDown() {
    delete class_manager_;
    delete object_manager_;
    delete service_locator_;
  }

  ServiceLocator* service_locator_;
  ObjectManager* object_manager_;
  ClassManager* class_manager_;
};

TEST_F(ClassManagerTest, ShouldNotBeAbleToFindClassWithNameThatIsNotAdded) {
  EXPECT_EQ(NULL, class_manager_->GetClassByClassName(
      "o3d.NonexistentClass"));
}

TEST_F(ClassManagerTest, ShouldGetCreatableClassByName) {
  EXPECT_EQ(CreatableClass::GetApparentClass(),
      class_manager_->GetClassByClassName("o3d.CreatableClass"));
}

TEST_F(ClassManagerTest, ShouldCreateCreatableClass) {
  ObjectBase::Ref object = class_manager_->CreateObject(
      "o3d.CreatableClass");
  EXPECT_FALSE(object.IsNull());
}

TEST_F(ClassManagerTest, ShouldGetNonCreatableClassByName) {
  EXPECT_EQ(NonCreatableClass::GetApparentClass(),
      class_manager_->GetClassByClassName("o3d.NonCreatableClass"));
}

TEST_F(ClassManagerTest, ShouldNotCreateNonCreatableClass) {
  ObjectBase::Ref object = class_manager_->CreateObject(
      "o3d.NonCreatableClass");
  EXPECT_TRUE(object.IsNull());
}

TEST_F(ClassManagerTest, ClassShouldDeriveFromObjectBase) {
  EXPECT_TRUE(class_manager_->ClassNameIsAClass(
      "o3d.NonCreatableClass",
      ObjectBase::GetApparentClass()));
}

TEST_F(ClassManagerTest, ClassShouldDeriveFromItself) {
  EXPECT_TRUE(class_manager_->ClassNameIsAClass(
      "o3d.NonCreatableClass",
      NonCreatableClass::GetApparentClass()));
}

TEST_F(ClassManagerTest, ShouldGetAllClasses) {
  std::vector<const ObjectBase::Class*> classes =
      class_manager_->GetAllClasses();
  ASSERT_FALSE(classes.end() == std::find(
      classes.begin(), classes.end(),
      CreatableClass::GetApparentClass()));
  ASSERT_FALSE(classes.end() == std::find(
      classes.begin(), classes.end(),
      NonCreatableClass::GetApparentClass()));
}
}  // namespace o3d
