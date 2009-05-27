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


// This file implements unit tests for class Material.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/material.h"

namespace o3d {

class MaterialTest : public testing::Test {
 protected:

  MaterialTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  TransformationContext* transformation_context_;
  SemanticManager* semantic_manager_;
  Pack *pack_;
};

void MaterialTest::SetUp() {
  transformation_context_ = new TransformationContext(g_service_locator);
  semantic_manager_ = new SemanticManager(g_service_locator);
  pack_ = object_manager_->CreatePack();
}

void MaterialTest::TearDown() {
  pack_->Destroy();
  delete semantic_manager_;
  delete transformation_context_;
}

// Test the basic operations of class Material.
TEST_F(MaterialTest, Basic) {
  Material* material = pack()->Create<Material>();
  // Check that material got created.
  EXPECT_TRUE(material != NULL);

  // Check that its state and effect are not set.
  EXPECT_TRUE(material->effect() == NULL);
  EXPECT_TRUE(material->state() == NULL);

  // Check that the default params got created.
  Param* effect_param = material->GetParam<ParamEffect>(
      Material::kEffectParamName);
  Param* state_param = material->GetParam<ParamState>(
      Material::kStateParamName);
  EXPECT_TRUE(effect_param != NULL);
  EXPECT_TRUE(state_param != NULL);

  // Check that if we set one by accessor the param is effected
  State* state = pack()->Create<State>();
  ASSERT_TRUE(state != NULL);
  ParamState* typed_state_param = down_cast<ParamState*>(state_param);
  material->set_state(state);
  EXPECT_EQ(state, typed_state_param->value());

  // Check that if we set one by param the accessor is effected
  Effect* effect = pack()->Create<Effect>();
  ASSERT_TRUE(effect != NULL);
  ParamEffect* typed_effect_param = down_cast<ParamEffect*>(effect_param);
  ASSERT_TRUE(typed_effect_param != NULL);
  typed_effect_param->set_value(effect);
  EXPECT_EQ(effect, material->effect());
}

}  // namespace o3d
