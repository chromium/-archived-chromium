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


#include "core/cross/client.h"
#include "core/cross/effect.h"
#include "core/cross/primitive.h"
#include "core/cross/standard_param.h"
#include "core/cross/param_array.h"
#include "core/cross/stream.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

class EffectTest : public testing::Test {
 protected:

  EffectTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  ObjectManager* object_manager() { return object_manager_.Get(); }
  Client* client() { return client_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Client *client_;
};

void EffectTest::SetUp() {
  client_ = new Client(g_service_locator);
  client_->Init();
}

void EffectTest::TearDown() {
  delete client_;
}

// ----------------------------------------------------------------------------

namespace {

char kLambertEffect[] =
    "struct a2v {                                            \n"
    "float4 pos : POSITION;                                  \n"
    "float3 normal : NORMAL;                                 \n"
    "float2 diffuseUV : TEXCOORD1;                           \n"
    "};                                                      \n"
    "struct v2f {                                            \n"
    "float4 pos : POSITION;                                  \n"
    "float3 n : TEXCOORD1;                                   \n"
    "float3 l : TEXCOORD2;                                   \n"
    "float2 diffuseUV : TEXCOORD0;                           \n"
    "};                                                      \n"
    "float4x4 worldViewProj : WorldViewProjection;           \n"
    "float4x4 world : World;                                 \n"
    "float4x4 worldIT : WorldInverseTranspose;               \n"
    "float3 lightWorldPos;                                   \n"
    "float4 lightColor;                                      \n"
    "uniform float4 emissive;                                \n"
    "uniform float4 ambient;                                 \n"
    "uniform float array[8];                                 \n"
    "uniform extern texture diffuseTexture;                  \n"
    "sampler2D diffuseSampler = sampler_state {              \n"
    "  Texture = <diffuseTexture>;                           \n"
    "};                                                      \n"
    "v2f vsMain(a2v IN) {                                    \n"
    "  v2f OUT;                                              \n"
    "  OUT.pos = mul(IN.pos, worldViewProj);                 \n"
    "  OUT.n = mul(float4(IN.normal, 0), worldIT).xyz;       \n"
    "  OUT.l = lightWorldPos - mul(IN.pos, world).xyz;       \n"
    "  OUT.diffuseUV = IN.diffuseUV;                         \n"
    "  return OUT;                                           \n"
    "}                                                       \n"
    "float4 fsMain(v2f IN): COLOR {                          \n"
    "  float4 diffuse = tex2D(diffuseSampler, IN.diffuseUV); \n"
    "  float3 l = normalize(IN.l);                           \n"
    "  float3 n = normalize(IN.n);                           \n"
    "  float4 litR = lit(dot(n,l),0,0);                      \n"
    "  return emissive+lightColor*(ambient+diffuse*litR.y);  \n"
    "}                                                       \n"
    "// #o3d VertexShaderEntryPoint vsMain                   \n"
    "// #o3d PixelShaderEntryPoint fsMain                    \n"
    "// #o3d MatrixLoadOrder RowMajor                        \n"
    "";

struct ParamInfo {
  const char* name;
  const ObjectBase::Class* type;
  int num_elements;
  const char* semantic;
  const ObjectBase::Class* sas_type;
};

ParamInfo expected_params[] = {
  { "lightWorldPos", ParamFloat3::GetApparentClass(), 0, "", NULL, },
  { "lightColor", ParamFloat4::GetApparentClass(), 0, "", NULL, },
  { "emissive", ParamFloat4::GetApparentClass(), 0, "", NULL, },
  { "ambient", ParamFloat4::GetApparentClass(), 0, "", NULL, },
  { "array", ParamFloat::GetApparentClass(), 8, "", NULL, },
  { "diffuseTexture", ParamTexture::GetApparentClass(), 0, "", NULL, },
  { "diffuseSampler", ParamSampler::GetApparentClass(), 0, "", NULL, },
  { "worldViewProj", ParamMatrix4::GetApparentClass(), 0, "WORLDVIEWPROJECTION",
    WorldViewProjectionParamMatrix4::GetApparentClass(), },
  { "world", ParamMatrix4::GetApparentClass(), 0, "WORLD",
    WorldParamMatrix4::GetApparentClass(), },
  { "worldIT", ParamMatrix4::GetApparentClass(), 0, "WORLDINVERSETRANSPOSE",
    WorldInverseTransposeParamMatrix4::GetApparentClass(), },
};

EffectStreamInfo expected_streams[] = {
  EffectStreamInfo(Stream::POSITION, 0),
  EffectStreamInfo(Stream::NORMAL, 0),
  EffectStreamInfo(Stream::TEXCOORD, 1)
};

float kVertexBlock[][4] = {
  { -1, 1, 0, 1, },
  { 1, 1, 0, 1, },
  { -1, -1, 0, 1, },
  { 1, -1, 0, 1, },
};

float kColorBlock[][3] = {
  { 1, 0, 0, },
  { 1, 1, 0, },
  { 0, 1, 0, },
  { 0, 0, 1, },
};

uint32 kIndexBlock[4] = {
  0, 1, 2, 3
};

bool IsExpectedParamInfo(const EffectParameterInfo& info) {
  for (unsigned ii = 0; ii < arraysize(expected_params); ++ii) {
    const ParamInfo& expected_info = expected_params[ii];
    if (info.name().compare(expected_info.name) == 0) {
      return info.class_type() == expected_info.type &&
          info.semantic().compare(expected_info.semantic) == 0 &&
          info.sas_class_type() == expected_info.sas_type;
    }
  }
  return false;
}

bool IsExpectedStream(const EffectStreamInfo& info) {
  for (unsigned ii = 0; ii < arraysize(expected_streams); ++ii) {
    const EffectStreamInfo& expected_info = expected_streams[ii];
    if (info.semantic() == expected_info.semantic() &&
        info.semantic_index() == expected_info.semantic_index()) {
      return true;
    }
  }
  return false;
}

}  // anonymous namespace.

TEST_F(EffectTest, LogOpenGLCalls) {
  // TODO(o3d): Find a way to implement a Mocklog object under
  // Googleclient. The code would be of the usual form:
  //
  //    ScopedMockLog log;
  //    EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());
  //    EXPECT_CALL(log, LOG(INFO, _, StartsWith("EffectGL created:"));
  //
  // In the mean time, work directly on the actual log file itself:

  Pack* pack = object_manager()->CreatePack();

  Shape* shape = pack->Create<Shape>();
  ASSERT_TRUE(shape != NULL);
  client()->root()->AddShape(shape);
  Primitive* primitive = pack->Create<Primitive>();
  ASSERT_TRUE(primitive != NULL);
  primitive->SetOwner(shape);

  // Load the vertex and fragment shaders.
  Effect *fx = pack->Create<Effect>();
  ASSERT_TRUE(fx != NULL);
  EXPECT_TRUE(fx->LoadFromFXString(String(kLambertEffect)));
  EXPECT_TRUE(!fx->source().compare(kLambertEffect));
  Material* material = pack->Create<Material>();
  ASSERT_TRUE(material);
  material->set_effect(fx);
  primitive->set_material(material);

  // TODO(o3d): Test the log file.

  // Clean up.
  object_manager()->DestroyPack(pack);
}

TEST_F(EffectTest, CreateAndDestroyEffect) {
  Pack* pack = object_manager()->CreatePack();

  Shape* shape = pack->Create<Shape>();
  ASSERT_TRUE(shape != NULL);
  client()->root()->AddShape(shape);
  Primitive* primitive = pack->Create<Primitive>();
  ASSERT_TRUE(primitive != NULL);
  primitive->SetOwner(shape);

  // load an effect
  Effect *fx = pack->Create<Effect>();
  ASSERT_TRUE(fx != NULL);
  EXPECT_TRUE(fx->LoadFromFXString(String(kLambertEffect)));
  Material* material = pack->Create<Material>();
  ASSERT_TRUE(material);
  material->set_effect(fx);
  primitive->set_material(material);
  StreamBank* stream_bank = pack->Create<StreamBank>();
  ASSERT_TRUE(stream_bank != NULL);
  primitive->set_stream_bank(stream_bank);

  VertexBuffer* verts = pack->Create<VertexBuffer>();
  VertexBuffer* color = pack->Create<VertexBuffer>();
  IndexBuffer* index = pack->Create<IndexBuffer>();

  ASSERT_TRUE(verts != NULL);
  ASSERT_TRUE(color != NULL);
  ASSERT_TRUE(index != NULL);

  void* vbuffer = NULL;
  Field* vertex_field = verts->CreateField(FloatField::GetApparentClass(),
                                           arraysize(kVertexBlock[0]));
  ASSERT_TRUE(vertex_field != NULL);
  ASSERT_TRUE(verts->AllocateElements(arraysize(kVertexBlock)));
  vertex_field->SetFromFloats(&kVertexBlock[0][0], arraysize(kVertexBlock[0]),
                              0, arraysize(kVertexBlock));

  Field* color_field = color->CreateField(FloatField::GetApparentClass(),
                                          arraysize(kColorBlock[0]));
  ASSERT_TRUE(color_field != NULL);
  ASSERT_TRUE(color->AllocateElements(arraysize(kColorBlock)));
  color_field->SetFromFloats(&kColorBlock[0][0], arraysize(kColorBlock[0]), 0,
                             arraysize(kColorBlock));

  EXPECT_TRUE(index->AllocateElements(arraysize(kIndexBlock)));
  index->index_field()->SetFromUInt32s(&kIndexBlock[0], 1, 0,
                                       arraysize(kIndexBlock));

  EXPECT_TRUE(stream_bank->SetVertexStream(Stream::POSITION,
                                           0,
                                           vertex_field,
                                           0));
  EXPECT_TRUE(stream_bank->SetVertexStream(Stream::COLOR,
                                           0,
                                           color_field,
                                           0));
  primitive->set_index_buffer(index);

  // Create effect parameters.

  ParamFloat3 *lightpos =
      shape->CreateParam<ParamFloat3>("lightworldPos");
  EXPECT_TRUE(lightpos != NULL);
  lightpos->set_value(Float3(0.2f, 10.5f, -3.14f));

  ParamFloat4 *lightcolor =
      shape->CreateParam<ParamFloat4>("lightColor");
  EXPECT_TRUE(lightcolor != NULL);
  lightcolor->set_value(Float4(0.8f, 0.2f, 0.655f, 1.0f));

  ParamFloat4 *emissive =
      shape->CreateParam<ParamFloat4>("emissive");
  EXPECT_TRUE(emissive != NULL);
  emissive->set_value(Float4(0.0f, 0.0f, 0.0f, 1.0f));

  ParamFloat4 *ambient =
      shape->CreateParam<ParamFloat4>("ambient");
  EXPECT_TRUE(ambient != NULL);
  ambient->set_value(Float4(0.25f, 0.25f, 0.35f, 1.0f));

  String filepath = *g_program_path + "/unittest_data/rock01.tga";
  Texture *texture = pack->CreateTextureFromFile(filepath,
                                                 filepath,
                                                 Bitmap::TGA,
                                                 true);
  EXPECT_TRUE(texture != NULL);

  ParamTexture * diffuse_texture =
      shape->CreateParam<ParamTexture>("diffuseTexture");
  EXPECT_TRUE(diffuse_texture != NULL);
  diffuse_texture->set_value(texture);

  // Clean up.
  object_manager()->DestroyPack(pack);
}

TEST_F(EffectTest, GetEffectParameters) {
  Pack* pack = object_manager()->CreatePack();
  ASSERT_TRUE(pack != NULL);

  // load an effect
  Effect *fx = pack->Create<Effect>();
  ASSERT_TRUE(fx != NULL);
  EXPECT_TRUE(fx->LoadFromFXString(String(kLambertEffect)));

  // Check that we get the correct params
  EffectParameterInfoArray info;
  fx->GetParameterInfo(&info);
  EXPECT_EQ(arraysize(expected_params), info.size());

  for (EffectParameterInfoArray::size_type ii = 0; ii < info.size(); ++ii) {
    EXPECT_TRUE(IsExpectedParamInfo(info[ii]));
  }

  // Clean up.
  object_manager()->DestroyPack(pack);
}

TEST_F(EffectTest, CreateUniformParameters) {
  Pack* pack = object_manager()->CreatePack();
  ASSERT_TRUE(pack != NULL);

  // load an effect
  Effect *fx = pack->Create<Effect>();
  ASSERT_TRUE(fx != NULL);
  EXPECT_TRUE(fx->LoadFromFXString(String(kLambertEffect)));

  ParamObject* param_object = pack->Create<ParamObject>();
  ASSERT_TRUE(param_object != NULL);

  // Check that we get the correct params
  fx->CreateUniformParameters(param_object);

  for (unsigned ii = 0; ii < arraysize(expected_params); ++ii) {
    const ParamInfo& expected_info = expected_params[ii];
    Param* param = param_object->GetUntypedParam(expected_info.name);
    if (expected_info.sas_type) {
      ASSERT_TRUE(param == NULL);
    } else {
      ASSERT_TRUE(param != NULL);
      if (expected_info.num_elements > 0) {
        ASSERT_TRUE(param->IsA(ParamParamArray::GetApparentClass()));
      } else {
        EXPECT_TRUE(param->IsA(expected_info.type));
      }
    }
  }

  // Clean up.
  object_manager()->DestroyPack(pack);
}

TEST_F(EffectTest, CreateSASParameters) {
  Pack* pack = object_manager()->CreatePack();
  ASSERT_TRUE(pack != NULL);

  // load an effect
  Effect *fx = pack->Create<Effect>();
  ASSERT_TRUE(fx != NULL);
  EXPECT_TRUE(fx->LoadFromFXString(String(kLambertEffect)));

  ParamObject* param_object = pack->Create<ParamObject>();
  ASSERT_TRUE(param_object != NULL);

  // Check that we get the correct params
  fx->CreateSASParameters(param_object);

  for (unsigned ii = 0; ii < arraysize(expected_params); ++ii) {
    const ParamInfo& expected_info = expected_params[ii];
    Param* param = param_object->GetUntypedParam(expected_info.name);
    if (expected_info.sas_type) {
      ASSERT_TRUE(param != NULL);
      if (expected_info.num_elements > 0) {
        ASSERT_TRUE(param->IsA(ParamParamArray::GetApparentClass()));
      } else {
        EXPECT_TRUE(param->IsA(expected_info.sas_type));
      }
    } else {
      ASSERT_TRUE(param == NULL);
    }
  }

  // Clean up.
  object_manager()->DestroyPack(pack);
}

TEST_F(EffectTest, GetEffectStreams) {
  Pack* pack = object_manager()->CreatePack();
  ASSERT_TRUE(pack != NULL);

  // load an effect
  Effect *fx = pack->Create<Effect>();
  ASSERT_TRUE(fx != NULL);
  EXPECT_TRUE(fx->LoadFromFXString(String(kLambertEffect)));

  // Check that we get the correct params
  EffectStreamInfoArray info;
  fx->GetStreamInfo(&info);
  EXPECT_EQ(arraysize(expected_streams), info.size());

  for (EffectStreamInfoArray::size_type ii = 0; ii < info.size(); ++ii) {
    EXPECT_TRUE(IsExpectedStream(info[ii]));
  }

  // Clean up.
  object_manager()->DestroyPack(pack);
}

}  // namespace o3d
