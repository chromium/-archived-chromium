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


#include "tests/common/win/testing_common.h"
#include "core/cross/math_utilities.h"
#include "core/cross/object_manager.h"
#include "core/cross/pack.h"
#include "core/cross/service_dependency.h"
#include "core/cross/matrix4_translation.h"

namespace o3d {

class TranslationTest : public testing::Test {
 protected:
  TranslationTest()
      : object_manager_(g_service_locator),
        pack_(NULL),
        translation_(NULL) {
  }

  virtual void SetUp() {
    pack_ = object_manager_->CreatePack();
    translation_ = pack_->Create<Matrix4Translation>();
  }

  virtual void TearDown() {
    pack_->Destroy();
  }

 protected:
  ServiceDependency<ObjectManager> object_manager_;
  Pack* pack_;
  Matrix4Translation* translation_;
};

TEST_F(TranslationTest, OutputsInputTranslationAsMatrixIfNoParent) {
  translation_->set_translation(Float3(1, 2, 3));
  Matrix4 output_matrix = translation_->output_matrix();
  Matrix4 expected(Vector4(1, 0, 0, 0), Vector4(0, 1, 0, 0),
                   Vector4(0, 0, 1, 0), Vector4(1, 2, 3, 1));
  ASSERT_NEAR(0.0f, FrobeniusNorm(expected - output_matrix), 0.001f);
}

TEST_F(TranslationTest, ComposesInputTranslationWithParentMatrix) {
  translation_->set_translation(Float3(1, 2, 3));
  Matrix4 parent = Matrix4::translation(Vector3(3, 4, 5));
  translation_->set_input_matrix(parent);
  Matrix4 output_matrix = translation_->output_matrix();
  Matrix4 expected = Matrix4::translation(Vector3(4, 6, 8));
  ASSERT_NEAR(0.0f, FrobeniusNorm(expected - output_matrix), 0.001f);
}
}  // namespace o3d
