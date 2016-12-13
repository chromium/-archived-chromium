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


// This file implements unit tests for class Shape.

#include "core/cross/object_manager.h"
#include "core/cross/pack.h"
#include "core/cross/primitive.h"
#include "core/cross/service_dependency.h"
#include "core/cross/shape.h"
#include "utils/cross/json_writer.h"
#include "utils/cross/string_writer.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

class ShapeTest : public testing::Test {
 protected:

  ShapeTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  ServiceDependency<ObjectManager> object_manager_;
  Pack *pack_;
};

void ShapeTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void ShapeTest::TearDown() {
  pack_->Destroy();
}

TEST_F(ShapeTest, ElementsAccessorShouldReturnAllElements) {
  Shape* shape = pack_->Create<Shape>();

  Element* element = pack_->Create<Primitive>();
  shape->AddElement(element);
  ElementArray elements = shape->GetElements();

  EXPECT_EQ(1, elements.size());
  EXPECT_EQ(element, elements[0]);
}

TEST_F(ShapeTest, ElementsAccessorShouldReplaceAllElements) {
  Shape* shape = pack_->Create<Shape>();

  Element* element1 = pack_->Create<Primitive>();
  shape->AddElement(element1);

  Element* element2 = pack_->Create<Primitive>();
  ElementArray elements;
  elements.push_back(element2);
  shape->SetElements(elements);
  elements = shape->GetElements();

  EXPECT_EQ(1, elements.size());
  EXPECT_EQ(element2, elements[0]);
}
}  // namespace o3d
