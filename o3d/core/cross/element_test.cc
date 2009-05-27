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


// This file implements unit tests for class Element.

#include <algorithm>
#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/element.h"
#include "core/cross/primitive.h"

namespace o3d {

class ElementTest : public testing::Test {
 protected:

  ElementTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack *pack_;
};

void ElementTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void ElementTest::TearDown() {
  pack_->Destroy();
}

TEST_F(ElementTest, Basic) {
  Element* element = pack()->Create<Primitive>();

  // Check that element got created.
  ASSERT_TRUE(element != NULL);

  // Check that its material is not set.
  EXPECT_TRUE(element->material() == NULL);

  // Check that the default params got created.
  EXPECT_TRUE(
      element->GetParam<ParamMaterial>(Element::kMaterialParamName) != NULL);
  EXPECT_TRUE(
      element->GetParam<ParamBoundingBox>(
          Element::kBoundingBoxParamName) != NULL);
  EXPECT_TRUE(
      element->GetParam<ParamFloat3>(Element::kZSortPointParamName) != NULL);
  EXPECT_TRUE(
      element->GetParam<ParamFloat>(Element::kPriorityParamName) != NULL);
  EXPECT_TRUE(
      element->GetParam<ParamBoolean>(Element::kCullParamName) != NULL);

  // Check that it has no owner.
  EXPECT_TRUE(element->owner() == NULL);
}

TEST_F(ElementTest, Owner) {
  Shape* shape1 = pack()->Create<Shape>();
  Shape* shape2 = pack()->Create<Shape>();
  Element* element = pack()->Create<Primitive>();

  ASSERT_TRUE(shape1 != NULL);
  ASSERT_TRUE(shape2 != NULL);
  ASSERT_TRUE(element != NULL);

  element->SetOwner(shape1);

  // Check that we are owned by shape1
  EXPECT_TRUE(element->owner() == shape1);

  // Check that shape1 owns us.
  const ElementRefArray& elements1 = shape1->GetElementRefs();
  EXPECT_TRUE(std::find(elements1.begin(),
                        elements1.end(),
                        Element::Ref(element)) !=
              elements1.end());

  // Check switching owners
  element->SetOwner(shape2);

  // Check that we are owned by shape2
  EXPECT_TRUE(element->owner() == shape2);

  // Check that shape1 no longer owns us.
  EXPECT_TRUE(std::find(elements1.begin(),
                        elements1.end(),
                        Element::Ref(element)) ==
              elements1.end());

  // Check that shape2 owns us.
  const ElementRefArray& elements2 = shape2->GetElementRefs();
  EXPECT_TRUE(std::find(elements2.begin(),
                        elements2.end(),
                        Element::Ref(element)) !=
              elements2.end());
}

TEST_F(ElementTest, DrawElement) {
  Element* element = pack()->Create<Primitive>();
  DrawElement* draw_element1 = pack()->Create<DrawElement>();
  DrawElement* draw_element2 = pack()->Create<DrawElement>();

  ASSERT_TRUE(element != NULL);
  ASSERT_TRUE(draw_element1 != NULL);
  ASSERT_TRUE(draw_element2 != NULL);

  // Check there are no draw elements.
  EXPECT_EQ(element->GetDrawElementRefs().size(), 0);

  element->AddDrawElement(draw_element1);

  // Check it's been added.
  EXPECT_EQ(element->GetDrawElementRefs().size(), 1);
  EXPECT_EQ(element->GetDrawElements()[0], draw_element1);

  // Check removing something else fails
  EXPECT_FALSE(element->RemoveDrawElement(draw_element2));

  // Remove it.
  EXPECT_TRUE(element->RemoveDrawElement(draw_element1));

  // Check it's been removed.
  EXPECT_EQ(element->GetDrawElementRefs().size(), 0);

  // Check removing it twice fails.
  EXPECT_FALSE(element->RemoveDrawElement(draw_element1));
}

static void CreateCube(Pack* pack, Primitive** primitive_pointer) {
  static float cube_vertices[][3] = {
    { 1000,  1000,   1000  },  // dummy vertex
    { -1.0f, -1.0f,  1.0f, },  // vertex v0
    { +1.0f, -1.0f,  1.0f, },  // vertex v1
    { +1.0f, -1.0f, -1.0f, },  // vertex v2
    { -1.0f, -1.0f, -1.0f, },  // vertex v3
    { -1.0f,  1.0f,  1.0f, },  // vertex v4
    { +1.0f,  1.0f,  1.0f, },  // vertex v5
    { +1.0f,  1.0f, -1.0f, },  // vertex v6
    { -1.0f,  1.0f, -1.0f, },  // vertex v7
  };

  static uint32 cube_indices[] = {
    0,        // dummy index
    0, 1, 4,  // triangle v0,v1,v4
    1, 5, 4,  // triangle v1,v5,v4
    1, 2, 5,  // triangle v1,v2,v5
    2, 6, 5,  // triangle v2,v6,v5
    2, 3, 6,  // triangle v2,v3,v6
    3, 7, 6,  // triangle v3,v7,v6
    3, 0, 7,  // triangle v3,v0,v7
    0, 4, 7,  // triangle v0,v4,v7
    4, 5, 7,  // triangle v4,v5,v7
    5, 6, 7,  // triangle v5,v6,v7
    3, 2, 0,  // triangle v3,v2,v0
    2, 1, 0   // triangle v2,v1,v0
  };
  Primitive* primitive = pack->Create<Primitive>();
  StreamBank* stream_bank = pack->Create<StreamBank>();
  ASSERT_TRUE(primitive != NULL);
  ASSERT_TRUE(stream_bank != NULL);
  primitive->set_stream_bank(stream_bank);

  // Check Setting Vertex Streams.
  VertexBuffer* vertex_buffer = pack->Create<VertexBuffer>();
  ASSERT_TRUE(vertex_buffer != NULL);
  Field* position_field = vertex_buffer->CreateField(
      FloatField::GetApparentClass(), arraysize(cube_vertices[0]));
  ASSERT_TRUE(position_field != NULL);
  ASSERT_TRUE(vertex_buffer->AllocateElements(arraysize(cube_vertices)));
  position_field->SetFromFloats(&cube_vertices[0][0],
                                arraysize(cube_vertices[0]), 0,
                                arraysize(cube_vertices));
  EXPECT_TRUE(stream_bank->SetVertexStream(Stream::POSITION,
                                           0,
                                           position_field,
                                           1));
  // Check Setting Index Streams.
  IndexBuffer* index_buffer = pack->Create<IndexBuffer>();
  ASSERT_TRUE(index_buffer != NULL);
  ASSERT_TRUE(index_buffer->AllocateElements(arraysize(cube_indices)));
  index_buffer->index_field()->SetFromUInt32s(cube_indices, 1, 0,
                                              arraysize(cube_indices));

  primitive->set_index_buffer(index_buffer);

  primitive->set_primitive_type(o3d::Primitive::TRIANGLELIST);
  primitive->set_start_index(1);
  primitive->set_number_primitives(12);
  primitive->set_number_vertices(8);

  EXPECT_EQ(primitive->primitive_type(), o3d::Primitive::TRIANGLELIST);
  EXPECT_EQ(primitive->number_primitives(), 12);
  EXPECT_EQ(primitive->number_vertices(), 8);

  *primitive_pointer = primitive;
}

static const float kEpsilon = 0.00001f;

TEST_F(ElementTest, IntersectRay) {
  Element* element = pack()->Create<Primitive>();

  ASSERT_TRUE(element != NULL);

  RayIntersectionInfo info;
  element->IntersectRay(0,
                        State::CULL_NONE,
                        Point3(0, 0, 0),
                        Point3(1.0f, 1.0f, 1.0f),
                        &info);

  // Check that it's invalid (there are no streams)
  EXPECT_FALSE(info.valid());

  Primitive* primitive;
  CreateCube(pack(), &primitive);
  Element* element2 = primitive;

  element2->IntersectRay(0,
                         State::CULL_NONE,
                         Point3(-2.0f, -2.0f, -2.0f),
                         Point3(2.0f, 2.0f, 2.0f),
                         &info);

  // Check that it intersected.
  EXPECT_TRUE(info.valid());
  EXPECT_TRUE(info.intersected());
  EXPECT_TRUE(fabsf(info.position().getX() - -1.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(info.position().getY() - -1.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(info.position().getZ() - -1.0f) < kEpsilon);

  // Check when culling counter clockwise
  element2->IntersectRay(0,
                         State::CULL_CCW,
                         Point3(-2.0f, -2.0f, -2.0f),
                         Point3(0.0f, 0.0f, 0.0f),
                         &info);

  // Check that it intersected.
  EXPECT_TRUE(info.valid());
  EXPECT_TRUE(info.intersected());

  // Check when culling clockwise
  element2->IntersectRay(0,
                         State::CULL_CW,
                         Point3(-2.0f, -2.0f, -2.0f),
                         Point3(0.0f, 0.0f, 0.0f),
                         &info);

  // Check that it did NOT intersect.
  EXPECT_TRUE(info.valid());
  EXPECT_FALSE(info.intersected());

  element2->IntersectRay(0,
                         State::CULL_NONE,
                         Point3(2.0f, 2.0f, 2.0f),
                         Point3(3.0f, 3.0f, 3.0f),
                         &info);

  // Check that it didn't intersect
  EXPECT_TRUE(info.valid());
  EXPECT_FALSE(info.intersected());
}  // namespace o3d

TEST_F(ElementTest, GetBoundingBox) {
  Element* element = pack()->Create<Primitive>();

  ASSERT_TRUE(element != NULL);

  BoundingBox box;
  element->GetBoundingBox(0, &box);

  // Check that it's invalid (there are no streams)
  EXPECT_FALSE(box.valid());

  Primitive* primitive;
  CreateCube(pack(), &primitive);
  Element* element2 = primitive;
  element2->GetBoundingBox(0, &box);
  EXPECT_TRUE(box.valid());
  EXPECT_TRUE(fabsf(box.min_extent().getX() - -1.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(box.min_extent().getY() - -1.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(box.min_extent().getZ() - -1.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(box.max_extent().getX() - 1.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(box.max_extent().getY() - 1.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(box.max_extent().getZ() - 1.0f) < kEpsilon);
}

}  // namespace o3d
