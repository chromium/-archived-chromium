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


// Basic O3D system test for constructing and rendering geometry.

#include <math.h>

#include "core/cross/client.h"
#include "core/cross/counter.h"
#include "core/cross/draw_context.h"
#include "core/cross/draw_pass.h"
#include "core/cross/draw_list.h"
#include "core/cross/primitive.h"
#include "core/cross/stream_bank.h"
#include "core/cross/tree_traversal.h"
#include "core/cross/clear_buffer.h"
#include "core/cross/math_utilities.h"
#include "import/cross/raw_data.h"
#include "tests/common/win/testing_common.h"
#include "tests/common/win/system_test.h"

namespace o3d {

float DegreesToRadians(float degrees) {
  return degrees * kPi / 180.f;
}

const char kShaderString[] =
"// World View Projection matrix that will transform the input vertices     \n"
"// to screen space.                                                        \n"
"float4x4 worldViewProjection : WorldViewProjection;                        \n"
"                                                                           \n"
"// input parameters for our vertex shader                                  \n"
"struct VertexShaderInput {                                                 \n"
"  float4 position : POSITION;                                              \n"
"};                                                                         \n"
"                                                                           \n"
"// input parameters for our pixel shader                                   \n"
"struct PixelShaderInput {                                                  \n"
"  float4 position : POSITION;                                              \n"
"};                                                                         \n"
"/**                                                                        \n"
"* The vertex shader simply transforms the input vertices to screen space.  \n"
"*/                                                                         \n"
"PixelShaderInput vertexShaderFunction(VertexShaderInput input) {           \n"
"  PixelShaderInput output;                                                 \n"
"                                                                           \n"
"  // Multiply the vertex positions by the worldViewProjection matrix to    \n"
"  // transform them to screen space.                                       \n"
"  output.position = mul(input.position, worldViewProjection);              \n"
"  return output;                                                           \n"
"}                                                                          \n"
"                                                                           \n"
"/**                                                                        \n"
"* This pixel shader just returns the color red.                            \n"
"*/                                                                         \n"
"float4 pixelShaderFunction(PixelShaderInput input): COLOR {                \n"
"  return float4(1, 0, 0, 1);  // Red.                                      \n"
"}                                                                          \n"
"                                                                           \n"
"// Here we tell our effect file *which* functions are                      \n"
"// our vertex and pixel shaders.                                           \n"
"                                                                           \n"
"// #o3d VertexShaderEntryPoint vertexShaderFunction                        \n"
"// #o3d PixelShaderEntryPoint pixelShaderFunction                          \n"
"// #o3d MatrixLoadOrder RowMajor                                           \n";

// System-test class for basic o3d geometry construction and render
// functionality.
class BasicSystemTest : public testing::Test {
 protected:
  BasicSystemTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Client* client() {
    return client_;
  }

  Pack* pack() {
    return pack_;
  }

  DrawContext* context() {
    return context_;
  }

  DrawList* opaque_draw_list() {
    return opaque_draw_list_;
  }

  DrawList* transparent_draw_list() {
    return transparent_draw_list_;
  }

  // Constructs a cube, and returns the transform node under which
  // the new cube shape resides.
  void CreateCube(Material::Ref material, Transform::Ref* transform);

  ServiceDependency<ObjectManager> object_manager_;

 private:
  Client *client_;
  Pack* pack_;
  DrawContext* context_;
  DrawList* opaque_draw_list_;
  DrawList* transparent_draw_list_;
};

void BasicSystemTest::SetUp() {
  client_ = new o3d::Client(g_service_locator);
  client_->Init();

  pack_ = object_manager_->CreatePack();

  Transform* root = client_->root();

  // Creates a clear buffer render node and parents it to the root.
  ClearBuffer* clear_buffer = pack_->Create<ClearBuffer>();
  clear_buffer->set_priority(0);
  clear_buffer->set_clear_color(Float4(0.5f, 0.5f, 0.5f, 1.0f));
  clear_buffer->SetParent(client_->render_graph_root());

  // Create DrawLists
  opaque_draw_list_ = pack_->Create<DrawList>();
  transparent_draw_list_ = pack_->Create<DrawList>();

  // Create DrawContext.
  context_ = pack_->Create<DrawContext>();

  // Creates a TreeTraversal and parents it to the root.
  TreeTraversal* tree_traversal = pack_->Create<TreeTraversal>();
  tree_traversal->set_priority(1);
  tree_traversal->SetParent(client_->render_graph_root());

  // Creates a DrawPass for opaque shapes.
  DrawPass* opaque_draw_pass = pack_->Create<DrawPass>();
  opaque_draw_pass->set_priority(2);
  opaque_draw_pass->set_draw_list(opaque_draw_list_);
  opaque_draw_pass->SetParent(client_->render_graph_root());

  // Creates a DrawPass for transparent shapes.
  DrawPass* transparent_draw_pass = pack_->Create<DrawPass>();
  transparent_draw_pass->set_priority(3);
  transparent_draw_pass->set_draw_list(transparent_draw_list_);
  transparent_draw_pass->SetParent(client_->render_graph_root());

  // Register the passlists and drawcontext with the TreeTraversal
  tree_traversal->RegisterDrawList(opaque_draw_list_, context_, true);
  tree_traversal->RegisterDrawList(transparent_draw_list_, context_, true);
  tree_traversal->set_transform(root);

  const Point3 eye(0.0f, 1.0f, 5.0f);
  const Point3 target(0.0f, 0.0f, 0.0f);
  const Vector3 up(0.0f, 1.0f, 0.0f);

  const Matrix4 perspective_matrix = Vectormath::Aos::CreatePerspectiveMatrix(
      DegreesToRadians(60.0f),
      1.0f,
      1.0f,
      1000.0f);
  const Matrix4 view_matrix = Matrix4::lookAt(eye, target, up);

  context_->set_view(view_matrix);
  context_->set_projection(perspective_matrix);
}

void BasicSystemTest::TearDown() {
  // Force another render to make the stream capture end.
  client()->RenderClient();

  pack_->Destroy();
  delete client_;
}

void BasicSystemTest::CreateCube(Material::Ref material,
                                 Transform::Ref* transform) {
  Shape::Ref cube_shape(pack()->Create<Shape>());
  ASSERT_FALSE(cube_shape.IsNull());

  Transform::Ref cube_xform(pack()->Create<Transform>());
  ASSERT_FALSE(cube_xform.IsNull());

  // Create the Primitive that will contain the geometry data for
  // the cube.
  Primitive::Ref cube_primitive(pack()->Create<Primitive>());
  ASSERT_FALSE(cube_primitive.IsNull());

  // Create a StreamBank to hold the streams of vertex data.
  StreamBank::Ref stream_bank(pack()->Create<StreamBank>());
  ASSERT_FALSE(stream_bank.IsNull());

  // Assign the material that was passed in to the primitive.
  cube_primitive->set_material(material);

  // Assign the Primitive to the Shape.
  cube_primitive->SetOwner(cube_shape);

  // Assign the StreamBank to the Primitive.
  cube_primitive->set_stream_bank(stream_bank);

  cube_primitive->set_primitive_type(Primitive::TRIANGLELIST);
  cube_primitive->set_number_primitives(12); // 12 triangles
  cube_primitive->set_number_vertices(8);    // 8 vertices in total

  cube_primitive->CreateDrawElement(pack(), NULL);

  static const float kPositionArray[][3] = {
    {-0.5, -0.5,  0.5},
    {0.5, -0.5,  0.5},
    {-0.5,  0.5,  0.5},
    {0.5,  0.5,  0.5},
    {-0.5,  0.5, -0.5},
    {0.5,  0.5, -0.5},
    {-0.5, -0.5, -0.5},
    {0.5, -0.5, -0.5}
  };

  static const unsigned int kIndicesArray[] = {
    0, 1, 2,  // face 1
    2, 1, 3,
    2, 3, 4,  // face 2
    4, 3, 5,
    4, 5, 6,  // face 3
    6, 5, 7,
    6, 7, 0,  // face 4
    0, 7, 1,
    1, 7, 3,  // face 5
    3, 7, 5,
    6, 0, 4,  // face 6
    4, 0, 2
  };

  static const unsigned int kNumComponents = arraysize(kPositionArray[0]);
  static const unsigned int kNumElements = arraysize(kPositionArray);
  static const unsigned int kStride = kNumComponents;

  VertexBuffer::Ref positions_buffer(pack()->Create<VertexBuffer>());

  FloatField::Ref positions_field(positions_buffer->CreateField(
      FloatField::GetApparentClass(),
      3));
  ASSERT_FALSE(positions_field.IsNull());

  ASSERT_TRUE(positions_buffer->AllocateElements(kNumElements));
  positions_field->SetFromFloats(&kPositionArray[0][0], kStride, 0,
                                 kNumElements);

  IndexBuffer::Ref index_buffer(pack()->Create<IndexBuffer>());
  ASSERT_FALSE(index_buffer.IsNull());

  static const unsigned int kNumIndices = arraysize(kIndicesArray);
  ASSERT_TRUE(index_buffer->AllocateElements(kNumIndices));
  index_buffer->index_field()->SetFromUInt32s(&kIndicesArray[0], 1,
                                              0, kNumIndices);

  // Associate the positions Buffer with the StreamBank.
  stream_bank->SetVertexStream(
      Stream::POSITION,      // semantic: This stream stores vertex positions
      0,                     // semantic index: First (and only) position stream
      positions_field,       // field: the field this stream uses.
      0);                    // start_index: How many elements to skip in the
                             //     field.

  // Associate the triangle indices Buffer with the primitive.
  cube_primitive->set_index_buffer(index_buffer);

  cube_xform->AddShape(cube_shape);
  *transform = cube_xform;
}

TEST_F(BasicSystemTest, BasicSystemTestCase) {
  ASSERT_FALSE(client()->root() == NULL);

  Transform *spin_transform = pack()->Create<Transform>();
  ASSERT_FALSE(spin_transform == NULL);

  Transform* root = client()->root();
  spin_transform->SetParent(root);

  Material::Ref cube_material(pack()->Create<Material>());
  ASSERT_FALSE(cube_material.IsNull());
  cube_material->set_draw_list(opaque_draw_list());

  Effect::Ref effect(pack()->Create<Effect>());
  effect->LoadFromFXString(kShaderString);
  cube_material->set_effect(effect);

  Transform::Ref cube_xform;
  CreateCube(cube_material, &cube_xform);
  ASSERT_FALSE(cube_xform.IsNull());
  cube_xform->SetParent(spin_transform);

  ASSERT_FALSE(spin_transform->GetChildren()[0] == NULL);

  // Assert that 5 rendered frames generate both the correct command streams,
  // and framebuffer contents.
  BEGIN_ASSERT_STREAM_CAPTURE();
  for (int frame_count = 0; frame_count < 5; ++frame_count) {
    client()->RenderClient();
    ASSERT_FRAMEBUFFER();
    Matrix4 mat(Matrix4::rotationY(static_cast<float>(frame_count) * 2 *
                                   static_cast<float>(M_PI) / 5.0f));
    spin_transform->set_local_matrix(mat);
  }
  END_ASSERT_STREAM_CAPTURE();
}

}  // namespace o3d
