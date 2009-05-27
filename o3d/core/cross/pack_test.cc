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


// Tests for functionality in pack.cc/.h.

#include "core/cross/client.h"
#include "core/cross/pack.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

class PackTest : public testing::Test {
 public:

  PackTest()
      : object_manager_(g_service_locator) {}

  ObjectManager* object_manager() { return object_manager_.Get(); }

 private:
  ServiceDependency<ObjectManager> object_manager_;
};

// Test basic Pack creation and destruction.
TEST_F(PackTest, Basic) {
  Pack* pack = object_manager()->CreatePack();
  pack->set_name("PackTest Basic");
  ASSERT_FALSE(pack == NULL);

  EXPECT_EQ(pack->name(), "PackTest Basic");
  EXPECT_TRUE(pack->Destroy());
}

// Test Pack object look-up by name and Id.
TEST_F(PackTest, ClientLookUp) {
  Pack* pack = object_manager()->CreatePack();
  pack->set_name("PackTest Basic");
  EXPECT_TRUE(object_manager()->GetById<Pack>(pack->id()) == pack);
  EXPECT_TRUE(pack->Destroy());
}

// Validate the lifetime behaviour for obects in a single pack.
TEST_F(PackTest, BasicLifetimeScope) {
  Pack* pack = object_manager()->CreatePack();
  Transform* transform1 = pack->Create<Transform>();
  Transform* transform2 = pack->Create<Transform>();
  Id id1 = transform1->id();
  Id id2 = transform2->id();

  // Remove all references to the pack.
  ASSERT_TRUE(pack->Destroy());

  // Upon removal of all references, the transforms should be destroyed.
  EXPECT_TRUE(object_manager()->GetById<Transform>(id1) == NULL);
  EXPECT_TRUE(object_manager()->GetById<Transform>(id2) == NULL);
}

// Validate the Pack object look-up routines.
TEST_F(PackTest, PackLookup) {
  Pack* pack1 = object_manager()->CreatePack();
  Pack* pack2 = object_manager()->CreatePack();
  Transform* transform1 = pack1->Create<Transform>();
  transform1->set_name("Transform1");
  Transform* transform2 = pack2->Create<Transform>();
  transform2->set_name("Transform2");

  EXPECT_TRUE(pack1->Get<Transform>("Transform1")[0] == transform1);
  EXPECT_TRUE(pack2->Get<Transform>("Transform2")[0] == transform2);

  // Validate that Pack look-ups are confined to the contents of the Pack.
  EXPECT_TRUE(pack1->Get<Transform>("Transform2").empty());
  EXPECT_TRUE(pack2->Get<Transform>("Transform1").empty());

  EXPECT_TRUE(pack1->GetById<Transform>(transform1->id()) == transform1);
  EXPECT_TRUE(pack2->GetById<Transform>(transform2->id()) == transform2);

  EXPECT_TRUE(pack1->GetById<Transform>(transform2->id()) == NULL);
  EXPECT_TRUE(pack2->GetById<Transform>(transform1->id()) == NULL);

  EXPECT_TRUE(pack1->Destroy());
  EXPECT_TRUE(pack2->Destroy());
}

// Validate the semantics of removal of objects from a Pack.
TEST_F(PackTest, RemoveObject) {
  Pack* pack = object_manager()->CreatePack();
  Transform* transform = pack->Create<Transform>();
  transform->set_name("Transform");
  Transform* transform2 = pack->Create<Transform>();

  const String transform_name(transform->name());
  const Id id(transform->id());

  pack->RemoveObject(transform);

  // The removed transform should not be accessible through the pack.
  EXPECT_TRUE(pack->GetById<Transform>(id) == NULL);
  EXPECT_TRUE(pack->Get<Transform>(transform_name).empty());

  // Existing transforms should remain untouched.
  EXPECT_TRUE(pack->GetById<Transform>(
      transform2->id()) == transform2);
  EXPECT_TRUE(pack->Get<Transform>(
      transform2->name())[0] == transform2);

  EXPECT_TRUE(pack->Destroy());
}

}  // namespace o3d
