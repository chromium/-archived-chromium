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


// Tests for ObjectBase.

#include "core/cross/object_base.h"
#include "core/cross/object_manager.h"
#include "core/cross/pack.h"
#include "core/cross/service_dependency.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

class ObjectBaseTest : public testing::Test {
 protected:
  ObjectBaseTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 protected:
  ServiceDependency<ObjectManager> object_manager_;
  Pack* pack_;
};

void ObjectBaseTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void ObjectBaseTest::TearDown() {
  object_manager_->DestroyPack(pack_);
}

TEST_F(ObjectBaseTest, ObjectIdOfNullIsZero) {
  ASSERT_EQ(0, GetObjectId(NULL));
}

TEST_F(ObjectBaseTest, ObjectIdOfObjectIsReturned) {
  ASSERT_EQ(pack_->id(), GetObjectId(pack_));
}

TEST_F(ObjectBaseTest, ObjectClassName) {
  ASSERT_EQ(0,
            strcmp(ObjectBase::GetApparentClass()->name(),
                   "o3d.ObjectBase"));
  ASSERT_EQ(0,
            strcmp(ObjectBase::GetApparentClass()->unqualified_name(),
                   "ObjectBase"));
}

}  // namespace o3d
