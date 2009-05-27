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


#include <algorithm>

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

using Vectormath::Aos::Vector4;
using Vectormath::Aos::Matrix4;

// Basic test fixture.  Simply creates a Client object
// before each test and deletes it after
class ClientBasic : public testing::Test {
 protected:

  ClientBasic()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();

  virtual void TearDown();

  Client* client() { return client_; }
  Pack* pack() { return pack_; }

  void DestroyPack();

  ServiceDependency<ObjectManager> object_manager_;

 private:
  Client *client_;
  Pack* pack_;
};

void ClientBasic::SetUp() {
  client_ = new Client(g_service_locator);
  client_->Init();
  pack_ = object_manager_->CreatePack();
}

void ClientBasic::TearDown() {
  DestroyPack();
  delete client_;
}

void ClientBasic::DestroyPack() {
  if (pack_) {
    object_manager_->DestroyPack(pack_);
    pack_ = NULL;
  }
}

// Tests that the Client always has a root
TEST_F(ClientBasic, GetRoot) {
  EXPECT_TRUE((client()->root() != NULL));
}

TEST_F(ClientBasic, CreatePack) {
  const String kPackName("CreatePack pack");
  Pack* pack = object_manager_->CreatePack();
  pack->set_name(kPackName);
  ASSERT_FALSE(pack == NULL);

  EXPECT_TRUE(object_manager_->GetById<Pack>(pack->id()) == pack);
  object_manager_->DestroyPack(pack);
}

// Tests Transform creation
TEST_F(ClientBasic, CreateTransform) {
  Transform* t = pack()->Create<Transform>();
  ASSERT_FALSE(t == NULL);

  EXPECT_TRUE(t->IsA(Transform::GetApparentClass()));

  EXPECT_TRUE(object_manager_->GetById<Transform>(t->id()) == t);

  // Make sure the node has no parent
  EXPECT_TRUE(t->parent() == NULL);
}

// Tests shape creation
TEST_F(ClientBasic, CreateShape) {
  Shape* s = pack()->Create<Shape>();
  ASSERT_FALSE(s == NULL);

  EXPECT_TRUE(s->IsA(Shape::GetApparentClass()));

  EXPECT_TRUE(object_manager_->GetById<Shape>(s->id()) == s);
}

// Buffer ----------------------------------------------------------------------

// Tests buffer creation
TEST_F(ClientBasic, CreateBuffer) {
  Buffer* b1(pack()->Create<IndexBuffer>());
  EXPECT_FALSE(b1 == NULL);
  Buffer* b2(pack()->Create<VertexBuffer>());
  EXPECT_FALSE(b2 == NULL);
}

// Tests GetBuffers
TEST_F(ClientBasic, GetBuffers) {
  Buffer* b1 = pack()->Create<IndexBuffer>();
  b1->set_name("buffer1");
  Buffer* b2 = pack()->Create<VertexBuffer>();
  b2->set_name("buffer1");
  Buffer* b3 = pack()->Create<VertexBuffer>();
  b3->set_name("buffer3");

  std::vector<Buffer*> buffers = object_manager_->Get<Buffer>("buffer1");
  EXPECT_EQ(2, buffers.size());
  std::vector<Buffer*>::const_iterator pos;
  EXPECT_TRUE((pos = find(buffers.begin(), buffers.end(), b1)) !=
      buffers.end());
  EXPECT_TRUE((pos = find(buffers.begin(), buffers.end(), b2)) !=
      buffers.end());
}

// Tests GetBufferById
TEST_F(ClientBasic, GetBufferById) {
  Buffer* b1 = pack()->Create<IndexBuffer>();
  Buffer* b2 = pack()->Create<VertexBuffer>();
  ASSERT_TRUE(b1 != NULL);
  ASSERT_TRUE(b2 != NULL);

  o3d::Id id1 = b1->id();
  o3d::Id id2 = b2->id();

  EXPECT_TRUE(object_manager_->GetById<Buffer>(id1) == b1);
  EXPECT_TRUE(object_manager_->GetById<Buffer>(id2) == b2);
}

// Effect ----------------------------------------------------------------------

// Tests Effect creation
TEST_F(ClientBasic, CreateEffect) {
  Effect* e1 = pack()->Create<Effect>();
  EXPECT_FALSE(e1 == NULL);
  Effect* e2 = pack()->Create<Effect>();
  EXPECT_FALSE(e2 == NULL);
}

// Tests GetEffects
TEST_F(ClientBasic, GetEffects) {
  Effect* e1 = pack()->Create<Effect>();
  e1->set_name("effect1");
  Effect* e2 = pack()->Create<Effect>();
  e2->set_name("effect1");
  Effect* e3 = pack()->Create<Effect>();
  e3->set_name("effect3");

  EffectArray effects = object_manager_->Get<Effect>("effect1");
  EXPECT_EQ(2, effects.size());
  EffectArray::const_iterator pos;
  EXPECT_TRUE((pos = find(effects.begin(), effects.end(), e1)) !=
      effects.end());
  EXPECT_TRUE((pos = find(effects.begin(), effects.end(), e2)) !=
      effects.end());
}

// Tests GetEffectById
TEST_F(ClientBasic, GetEffectById) {
  Effect* e1 = pack()->Create<Effect>();
  Effect* e2 = pack()->Create<Effect>();
  ASSERT_TRUE(e1 != NULL);
  ASSERT_TRUE(e2 != NULL);

  o3d::Id id1 = e1->id();
  o3d::Id id2 = e2->id();

  EXPECT_TRUE(object_manager_->GetById<Effect>(id1) == e1);
  EXPECT_TRUE(object_manager_->GetById<Effect>(id2) == e2);
}

// Scenegraph tree -------------------------------------------------------------

// Scenegraph tree test fixture.  Creates a Client object and
// a simple hierarchy in it
class ClientTree : public testing::Test {
 protected:

  ClientTree()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();

  virtual void TearDown();

  Client* client() { return client_; }
  Pack* pack() { return pack_; }
  void DestroyPack();

  Transform *t1_, *t2_, *t3_, *t4_;
  Shape *s1_, *s2_, *s3_;
  o3d::Id id_t1_, id_t2_, id_t3_, id_t4_, id_s1_, id_s2_, id_s3_;

  ServiceDependency<ObjectManager> object_manager_;

 private:
  Client *client_;
  Pack* pack_;
};

void ClientTree::SetUp() {
  client_ = new Client(g_service_locator);
  client_->Init();

  pack_ = object_manager_->CreatePack();

  t1_ = pack_->Create<Transform>();
  t1_->set_name("t1");
  t2_ = pack_->Create<Transform>();
  t2_->set_name("t2");
  t3_ = pack_->Create<Transform>();
  t3_->set_name("t3");
  t4_ = pack_->Create<Transform>();
  t4_->set_name("t4");

  s1_ = pack_->Create<Shape>();
  ASSERT_TRUE(s1_ != NULL);
  s2_ = pack_->Create<Shape>();
  ASSERT_TRUE(s2_ != NULL);
  s3_ = pack_->Create<Shape>();
  ASSERT_TRUE(s3_ != NULL);

  Transform *root = client_->root();
  t1_->SetParent(root);
  t2_->SetParent(root);
  t3_->SetParent(t2_);
  t4_->SetParent(t2_);

  t2_->AddShape(s1_);
  t3_->AddShape(s2_);
  t4_->AddShape(s3_);

  id_t1_ = t1_->id();
  id_t2_ = t2_->id();
  id_t3_ = t3_->id();
  id_t4_ = t4_->id();

  id_s1_ = s1_->id();
  id_s2_ = s2_->id();
  id_s3_ = s3_->id();
}

void ClientTree::TearDown() {
  DestroyPack();
  delete client_;
}

void ClientTree::DestroyPack() {
  if (pack_) {
    object_manager_->DestroyPack(pack_);
    pack_ = NULL;
  }
}

// Tests GetById
TEST_F(ClientTree, GetById) {
  EXPECT_TRUE(object_manager_->GetById<Shape>(id_s2_) == s2_);
}

// Tests GetByName
TEST_F(ClientTree, GetByName) {
  TransformArray transforms = object_manager_->Get<Transform>("t3");
  EXPECT_EQ(1, transforms.size());
  EXPECT_TRUE(transforms[0] == t3_);

  transforms = object_manager_->Get<Transform>("t5");
  EXPECT_EQ(0, transforms.size());

  // Add one more transform with the name "t3"
  Transform* t3_new = pack()->Create<Transform>();
  t3_new->set_name("t3");
  transforms = object_manager_->Get<Transform>("t3");
  EXPECT_EQ(2, transforms.size());
  TransformArray::const_iterator pos;
  EXPECT_TRUE((pos = find(transforms.begin(),
                          transforms.end(),
                          t3_)) != transforms.end());
  EXPECT_TRUE((pos = find(transforms.begin(),
                          transforms.end(),
                          t3_new)) != transforms.end());
}

// NaCl interface --------------------------------------------------------------

#if 0  // TODO: Understand and fix this stuff
// Native client interface test fixture
class NaClInterface : public testing::Test {
 protected:
  virtual void SetUp();

  virtual void TearDown();

  Client* client() { return client_; }
  Pack* pack() { return pack_; }

  Transform *t1_, *t2_, *t3_, *t4_;
  Shape *s1_, *s2_, *s3_;
  Primitive *p1_, *p2_, *p3_;
  o3d::Id id_t1_, id_t2_, id_t3_, id_t4_, id_s1_, id_s2_, id_s3_;
  int vertex_buffer_size_;
  int index_buffer_size_;
  scoped_array<float> cube_vertices1_;
  scoped_array<float> cube_vertices2_;

 private:
  Client *client_;
  Pack* pack_;
};

void NaClInterface::SetUp() {
  client_ = new Client;
  client_->Init(g_renderer);

  pack_ = object_manager_->CreatePack();

  t1_ = pack_->Create<Transform>("t1");
  t2_ = pack_->Create<Transform>("t2");
  t3_ = pack_->Create<Transform>("t3");
  t4_ = pack_->Create<Transform>("t4");

  s1_ = pack_->Create<Shape>("s1");
  p1_ = pack_->Create<Primitive>("p1");
  ASSERT_TRUE(s1_ != NULL);
  ASSERT_TRUE(p1_ != NULL);
  s2_ = pack_->Create<Shape>("s2");
  p2_ = pack_->Create<Primitive>("p2");
  ASSERT_TRUE(s2_ != NULL);
  ASSERT_TRUE(p2_ != NULL);
  s3_ = pack_->Create<Shape>("s3");
  p3_ = pack_->Create<Primitive>("p3");
  ASSERT_TRUE(s3_ != NULL);
  ASSERT_TRUE(p3_ != NULL);

  Transform *root = client()->root();
  t1_->SetParent(root);
  t2_->SetParent(root);
  t3_->SetParent(t2_);
  t4_->SetParent(t2_);

  t2_->AddShape(s1_);
  t3_->AddShape(s2_);
  t4_->AddShape(s3_);

  s1_->AddPrimitive(p1_);
  s2_->AddPrimitive(p2_);
  s3_->AddPrimitive(p3_);

  const unsigned int kCubeIndicies[] =  {
    0, 1, 2, 1, 3, 2,  // Quad 0
    4, 5, 6, 5, 7, 6,  // Quad 1
    4, 6, 0, 6, 1, 0,  // Quad 2
    5, 2, 7, 2, 3, 7,  // Quad 3
    1, 6, 3, 6, 7, 3,  // Quad 4
    0, 2, 4, 2, 5, 4   // Quad 5
  };

  cube_vertices1_.reset(new float[8*3]);
  cube_vertices2_.reset(new float[8*3]);

  for (int ii = 0;ii < 8*3;ii++) {
    cube_vertices1_[ii] = static_cast<float>(ii+100);
    cube_vertices2_[ii] = static_cast<float>(ii+200);
  }


  // Create and fill the vertex buffer
  VertexBuffer* vert_buffer = pack()->Create<VertexBuffer>("CubeVerts");
  ASSERT_TRUE(vert_buffer != NULL);
  vertex_buffer_size_ = 8*3*sizeof(cube_vertices1_[0]);
  vert_buffer->Allocate(vertex_buffer_size_);
  void *vertices = NULL;
  vert_buffer->Lock(&vertices);
  memcpy(vertices, cube_vertices1_.get(), vertex_buffer_size_);
  vert_buffer->Unlock();

  // Create and fill the vertex buffer
  VertexBuffer* vert_buffer2 = pack()->Create<VertexBuffer>("CubeVerts2");
  ASSERT_TRUE(vert_buffer2 != NULL);
  vert_buffer2->Allocate(vertex_buffer_size_);
  void *vertices2 = NULL;
  vert_buffer2->Lock(&vertices2);
  memcpy(vertices2, cube_vertices2_.get(), vertex_buffer_size_);
  vert_buffer2->Unlock();

  // Create and fill the index buffer
  IndexBuffer* index_buffer = pack()->Create<IndexBuffer>("CubeIndices");
  ASSERT_TRUE(index_buffer != NULL);
  index_buffer->Allocate(6*6*sizeof(kCubeIndicies[0]));
  unsigned int *indices = NULL;
  void *indices_void = static_cast<void*>(indices);
  index_buffer->Lock(&indices_void);
  memcpy(indices_void, kCubeIndicies, sizeof(kCubeIndicies));
  index_buffer->Unlock();
  index_buffer_size_ = 6*6*sizeof(kCubeIndicies[0]);

  p1_->SetVertexStream(Stream::POSITION,
                       0,
                       Stream::FLOAT3,
                       vert_buffer,
                       0,
                       sizeof(cube_vertices1_[0]) * 3);
  p1_->SetIndexStream(index_buffer, 0);

  p2_->SetVertexStream(Stream::POSITION,
                       0,
                       Stream::FLOAT3,
                       vert_buffer2,
                       0,
                       sizeof(cube_vertices2_[0]) * 3);
  p2_->SetIndexStream(index_buffer, 0);

  id_t1_ = t1_->id();
  id_t2_ = t2_->id();
  id_t3_ = t3_->id();
  id_t4_ = t4_->id();

  id_s1_ = s1_->id();
  id_s2_ = s2_->id();
  id_s3_ = s3_->id();
}

void NaClInterface::TearDown() {
  object_manager_->DestroyPack(pack_);
  delete client_;
}

TEST_F(NaClInterface, GetPropertyBufferSize) {
  // Fail when an id is not found
  IdArray ids;
  ids.push_back(100);
  EXPECT_EQ(-1,
            client()->GetPropertyBufferSize(ids, Client::PROP_WORLD_MATRIX));

  // Fail when the property is not available in one of the nodes
  // (Shapes don't have a world matrix property)
  ids.clear();
  ids.push_back(id_s1_);
  ids.push_back(id_t1_);
  EXPECT_EQ(-1,
            client()->GetPropertyBufferSize(ids, Client::PROP_WORLD_MATRIX));

  ids.clear();
  ids.push_back(id_t1_);
  ids.push_back(id_t2_);
  ids.push_back(id_t3_);
  int expected_size = 3*4*sizeof(float)*3;  // NOLINT
  EXPECT_EQ(expected_size,
            client()->GetPropertyBufferSize(ids, Client::PROP_WORLD_MATRIX));

  ids.clear();
  ids.push_back(id_s1_);
  ids.push_back(id_s2_);
  expected_size = 2*vertex_buffer_size_;
  EXPECT_EQ(
      expected_size,
      client()->GetPropertyBufferSize(ids, Client::PROP_VERTEX_POSITIONS));
}

TEST_F(NaClInterface, GetPropertyToBuffer) {
  IdArray ids;
  ids.push_back(id_s1_);
  ids.push_back(id_s2_);
  int sz = client()->GetPropertyBufferSize(ids, Client::PROP_VERTEX_POSITIONS);
  float *buff = static_cast<float*>(malloc(sz));
  EXPECT_TRUE(client()->GetPropertyToBuffer(buff,
                                            sz,
                                            ids,
                                            Client::PROP_VERTEX_POSITIONS));
  EXPECT_TRUE(buff[0] == cube_vertices1_[0]);
  EXPECT_TRUE(buff[5] == cube_vertices1_[5]);
  EXPECT_TRUE(buff[10] == cube_vertices1_[10]);

  EXPECT_TRUE(buff[24+0] == cube_vertices2_[0]);
  EXPECT_TRUE(buff[24+4] == cube_vertices2_[4]);
  EXPECT_TRUE(buff[24+10] == cube_vertices2_[10]);
}

TEST_F(NaClInterface, SetPropertyFromBuffer) {
  IdArray ids;
  ids.push_back(id_s1_);
  ids.push_back(id_s2_);
  int sz = client()->GetPropertyBufferSize(ids, Client::PROP_VERTEX_POSITIONS);
  int float_count = sz/sizeof(float);  // NOLINT
  float *buff = new float[float_count];
  for (int ii = 0;ii < float_count;ii++)
    buff[ii] = static_cast<float>(ii);

  EXPECT_TRUE(client()->SetPropertyFromBuffer(buff,
                                              sz,
                                              ids,
                                              Client::PROP_VERTEX_POSITIONS));

  // Check that the values made it to the first shape
  Stream stream = p1_->GetVertexStream(Stream::POSITION, 0);
  Buffer *buffer = stream.buffer();
  void *buffer_data = NULL;
  buffer->Lock(&buffer_data);
  float *f_data = static_cast<float*>(buffer_data);
  int ii;
  for (ii = 0;ii < float_count/2;ii++)
    EXPECT_TRUE(f_data[ii] == buff[ii]);
  buffer->Unlock();

  // Check that the values made it to the second shape
  stream = p2_->GetVertexStream(Stream::POSITION, 0);
  buffer = stream.buffer();
  buffer_data = NULL;
  buffer->Lock(&buffer_data);
  f_data = static_cast<float*>(buffer_data);
  for (ii = 0;ii < float_count/2;ii++)
    EXPECT_TRUE(f_data[ii] == buff[ii + float_count/2]);
  buffer->Unlock();

  delete [] buff;
}
#endif

}  // namespace o3d
