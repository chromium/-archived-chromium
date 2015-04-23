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
#include "core/cross/matrix4_scale.h"
#include "core/cross/service_dependency.h"

namespace o3d {

class Matrix4ScaleTest : public testing::Test {
 protected:
  Matrix4ScaleTest()
      : object_manager_(g_service_locator),
        pack_(NULL),
        scale_(NULL) {
  }

  virtual void SetUp() {
    pack_ = object_manager_->CreatePack();
    scale_ = pack_->Create<Matrix4Scale>();
  }

  virtual void TearDown() {
    pack_->Destroy();
  }

 protected:
  ServiceDependency<ObjectManager> object_manager_;
  Pack* pack_;
  Matrix4Scale* scale_;
};

TEST_F(Matrix4ScaleTest, OutputsInputScaleAsMatrixIfNoParent) {
  scale_->set_scale(Float3(1, 2, 3));
  Matrix4 output_matrix = scale_->output_matrix();
  Matrix4 expected(Vector4(1, 0, 0, 0), Vector4(0, 2, 0, 0),
                   Vector4(0, 0, 3, 0), Vector4(0, 0, 0, 1));
  ASSERT_NEAR(0.0f, FrobeniusNorm(expected - output_matrix), 0.001f);
}

TEST_F(Matrix4ScaleTest, ComposesInputScaleWithParentMatrix) {
  scale_->set_scale(Float3(1, 2, 3));
  Matrix4 parent = Matrix4::scale(Vector3(3, 4, 5));
  scale_->set_input_matrix(parent);
  Matrix4 output_matrix = scale_->output_matrix();
  Matrix4 expected = Matrix4::scale(Vector3(3, 8, 15));
  ASSERT_NEAR(0.0f, FrobeniusNorm(expected - output_matrix), 0.001f);
}
}  // namespace o3d
