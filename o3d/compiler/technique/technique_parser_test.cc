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

//
// Test program to exercise the Technique Antlr grammar using both files and
// in-memory string buffers.

#include "tests/common/win/testing_common.h"
#include "compiler/technique/technique_parser.h"

namespace o3d {

// classes ---------------------------------------------------------------------

class TechniqueParserTest : public testing::Test {
 protected:
  virtual void SetUp();
  virtual void TearDown();
 private:
};

void TechniqueParserTest::SetUp() {
}

void TechniqueParserTest::TearDown() {
}

// globals ---------------------------------------------------------------------

char simple_fx_source[] =
"float4x4 worldViewProj : WORLDVIEWPROJECTION;  \
void vs(in float4 pos, out float4 opos) {       \
  opos = mul(pos, worldViewProj);               \
}                                               \
float4 fs(): COLOR {                            \
  return float3(0.33f, 0.57f, 0.10f);           \
}                                               \
technique t1 {                                  \
  pass p0 {                                     \
    VertexShader = compile vs_2_0 vs();         \
    PixelShader = compile ps_2_0 fs();          \
  }                                             \
}                                               \
";

char lambert_fx_source[] =
"struct a2v {                                           \
  float4 pos : POSITION;                                \
  float3 normal : NORMAL;                               \
};                                                      \
                                                        \
struct v2f {                                            \
  float4 pos : POSITION;                                \
  float3 n : TEXCOORD0;                                 \
  float3 l : TEXCOORD1;                                 \
};                                                      \
                                                        \
float4x4 worldViewProj : WorldViewProjection;           \
float4x4 world : World;                                 \
float4x4 worldIT : WorldInverseTranspose;               \
float3 lightWorldPos;                                   \
float4 lightColor;                                      \
                                                        \
v2f vsMain(a2v IN) {                                    \
  v2f OUT;                                              \
  OUT.pos = mul(IN.pos, worldViewProj);                 \
  OUT.n = mul(float4(IN.normal,0), worldIT).xyz;        \
  OUT.l = lightWorldPos-mul(IN.pos, world).xyz;         \
  return OUT;                                           \
}                                                       \
                                                        \
float4 fsMain(v2f IN): COLOR {                          \
  float3 l=normalize(IN.l);                             \
  float3 n=normalize(IN.n);                             \
  float4 litR=lit(dot(n,l),0,0);                        \
  return emissive+lightColor*(ambient+diffuse*litR.y);  \
}                                                       \
                                                        \
technique {                                             \
  pass p0 {                                             \
    VertexShader = compile vs_2_0 vsMain();             \
    PixelShader = compile ps_2_0 fsMain();              \
  }                                                     \
}                                                       \
";

// -----------------------------------------------------------------------------

TEST_F(TechniqueParserTest, ParseSimpleFXFromFile) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  String filepath = *g_program_path + "/unittest_data/simple.fx";
  EXPECT_TRUE(ParseFxFile(filepath,
                          &shader_source,
                          &sampler_list,
                          &technique_list,
                          &error_string));
  EXPECT_LT(0, static_cast<int>(technique_list.size()));
  ASSERT_EQ(1, technique_list.size());
  EXPECT_EQ("t1", technique_list[0].name);
  ASSERT_EQ(0, technique_list[0].annotation.size());
  ASSERT_EQ(1, technique_list[0].pass.size());
  EXPECT_EQ("p0", technique_list[0].pass[0].name);
  ASSERT_EQ(0, technique_list[0].pass[0].annotation.size());
  EXPECT_EQ("vs", technique_list[0].pass[0].vertex_shader_entry);
  EXPECT_EQ("vs_2_0", technique_list[0].pass[0].vertex_shader_profile);
  EXPECT_EQ("", technique_list[0].pass[0].vertex_shader_arguments);
  EXPECT_EQ("fs", technique_list[0].pass[0].fragment_shader_entry);
  EXPECT_EQ("ps_2_0", technique_list[0].pass[0].fragment_shader_profile);
  EXPECT_EQ("", technique_list[0].pass[0].fragment_shader_arguments);
  ASSERT_EQ(0, technique_list[0].pass[0].state_assignment.size());
  EXPECT_EQ(0, sampler_list.size());
}

TEST_F(TechniqueParserTest, ParseSimpleFXFromString) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  EXPECT_TRUE(ParseFxString(simple_fx_source,
                            &shader_source,
                            &sampler_list,
                            &technique_list,
                            &error_string));
  EXPECT_LT(0, static_cast<int>(technique_list.size()));
  ASSERT_EQ(1, technique_list.size());
  EXPECT_EQ("t1", technique_list[0].name);
  ASSERT_EQ(0, technique_list[0].annotation.size());
  ASSERT_EQ(1, technique_list[0].pass.size());
  EXPECT_EQ("p0", technique_list[0].pass[0].name);
  ASSERT_EQ(0, technique_list[0].pass[0].annotation.size());
  EXPECT_EQ("vs", technique_list[0].pass[0].vertex_shader_entry);
  EXPECT_EQ("vs_2_0", technique_list[0].pass[0].vertex_shader_profile);
  EXPECT_EQ("", technique_list[0].pass[0].vertex_shader_arguments);
  EXPECT_EQ("fs", technique_list[0].pass[0].fragment_shader_entry);
  EXPECT_EQ("ps_2_0", technique_list[0].pass[0].fragment_shader_profile);
  EXPECT_EQ("", technique_list[0].pass[0].fragment_shader_arguments);
  ASSERT_EQ(0, technique_list[0].pass[0].state_assignment.size());
  EXPECT_EQ(0, sampler_list.size());
}

TEST_F(TechniqueParserTest, ParseLambertFXFromFile) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  String filepath = *g_program_path + "/unittest_data/lambert.fx";
  EXPECT_TRUE(ParseFxFile(filepath.c_str(),
                          &shader_source,
                          &sampler_list,
                          &technique_list,
                          &error_string));
  EXPECT_LT(0, static_cast<int>(technique_list.size()));
  ASSERT_EQ(1, technique_list.size());
  EXPECT_EQ("", technique_list[0].name);
  ASSERT_EQ(0, technique_list[0].annotation.size());
  ASSERT_EQ(1, technique_list[0].pass.size());
  EXPECT_EQ("p0", technique_list[0].pass[0].name);
  ASSERT_EQ(0, technique_list[0].pass[0].annotation.size());
  EXPECT_EQ("vsMain", technique_list[0].pass[0].vertex_shader_entry);
  EXPECT_EQ("vs_2_0", technique_list[0].pass[0].vertex_shader_profile);
  EXPECT_EQ("", technique_list[0].pass[0].vertex_shader_arguments);
  EXPECT_EQ("fsMain", technique_list[0].pass[0].fragment_shader_entry);
  EXPECT_EQ("ps_2_0", technique_list[0].pass[0].fragment_shader_profile);
  EXPECT_EQ("", technique_list[0].pass[0].fragment_shader_arguments);
  ASSERT_EQ(0, technique_list[0].pass[0].state_assignment.size());
  EXPECT_EQ(0, sampler_list.size());
}

TEST_F(TechniqueParserTest, ParseLambertFXFromString) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  EXPECT_TRUE(ParseFxString(lambert_fx_source,
                            &shader_source,
                            &sampler_list,
                            &technique_list,
                            &error_string));
  EXPECT_LT(0, static_cast<int>(technique_list.size()));
  ASSERT_EQ(1, technique_list.size());
  EXPECT_EQ("", technique_list[0].name);
  ASSERT_EQ(0, technique_list[0].annotation.size());
  ASSERT_EQ(1, technique_list[0].pass.size());
  EXPECT_EQ("p0", technique_list[0].pass[0].name);
  ASSERT_EQ(0, technique_list[0].pass[0].annotation.size());
  EXPECT_EQ("vsMain", technique_list[0].pass[0].vertex_shader_entry);
  EXPECT_EQ("vs_2_0", technique_list[0].pass[0].vertex_shader_profile);
  EXPECT_EQ("", technique_list[0].pass[0].vertex_shader_arguments);
  EXPECT_EQ("fsMain", technique_list[0].pass[0].fragment_shader_entry);
  EXPECT_EQ("ps_2_0", technique_list[0].pass[0].fragment_shader_profile);
  EXPECT_EQ("", technique_list[0].pass[0].fragment_shader_arguments);
  ASSERT_EQ(0, technique_list[0].pass[0].state_assignment.size());
  ASSERT_EQ(0, sampler_list.size());
}



// Test the longer shaders from files ------------------------------------------

TEST_F(TechniqueParserTest, ParseNoShaderFXFromFile) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  String filepath = *g_program_path + "/unittest_data/noshader.fx";
  EXPECT_TRUE(ParseFxFile(filepath,
                          &shader_source,
                          &sampler_list,
                          &technique_list,
                          &error_string));
  EXPECT_LT(0, static_cast<int>(technique_list.size()));
  ASSERT_EQ(1, technique_list.size());
  EXPECT_EQ("t1", technique_list[0].name);
  ASSERT_EQ(0, technique_list[0].annotation.size());
  ASSERT_EQ(1, technique_list[0].pass.size());
  EXPECT_EQ("p0", technique_list[0].pass[0].name);
  ASSERT_EQ(0, technique_list[0].pass[0].annotation.size());
  EXPECT_EQ("", technique_list[0].pass[0].vertex_shader_entry);
  EXPECT_EQ("", technique_list[0].pass[0].vertex_shader_profile);
  EXPECT_EQ("", technique_list[0].pass[0].vertex_shader_arguments);
  EXPECT_EQ("", technique_list[0].pass[0].fragment_shader_entry);
  EXPECT_EQ("", technique_list[0].pass[0].fragment_shader_profile);
  EXPECT_EQ("", technique_list[0].pass[0].fragment_shader_arguments);
  ASSERT_EQ(4, technique_list[0].pass[0].state_assignment.size());
  EXPECT_EQ("ZEnable", technique_list[0].pass[0].state_assignment[0].name);
  EXPECT_EQ("true", technique_list[0].pass[0].state_assignment[0].value);
  EXPECT_EQ("ZWriteEnable", technique_list[0].pass[0].state_assignment[1].name);
  EXPECT_EQ("true", technique_list[0].pass[0].state_assignment[1].value);
  EXPECT_EQ("ZFunc", technique_list[0].pass[0].state_assignment[2].name);
  EXPECT_EQ("LessEqual", technique_list[0].pass[0].state_assignment[2].value);
  EXPECT_EQ("CullMode", technique_list[0].pass[0].state_assignment[3].name);
  EXPECT_EQ("None", technique_list[0].pass[0].state_assignment[3].value);
  EXPECT_EQ(0, sampler_list.size());
}

TEST_F(TechniqueParserTest, ParseNoTechniqueFXFromFile) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  String filepath = *g_program_path + "/unittest_data/notechnique.fx";
  EXPECT_TRUE(ParseFxFile(filepath,
                          &shader_source,
                          &sampler_list,
                          &technique_list,
                          &error_string));
  EXPECT_EQ(0, technique_list.size());
  EXPECT_EQ(0, sampler_list.size());
}

TEST_F(TechniqueParserTest, ParseFurFXFromFile) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  String filepath = *g_program_path + "/unittest_data/fur.fx";
  EXPECT_TRUE(ParseFxFile(filepath,
                          &shader_source,
                          &sampler_list,
                          &technique_list,
                          &error_string));
  ASSERT_EQ(1, technique_list.size());
  EXPECT_EQ(1, sampler_list.size());
}

TEST_F(TechniqueParserTest, ParseShadowMapFXFromFile) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  String filepath = *g_program_path + "/unittest_data/shadow_map.fx";
  EXPECT_TRUE(ParseFxFile(filepath,
                          &shader_source,
                          &sampler_list,
                          &technique_list,
                          &error_string));
  ASSERT_EQ(2, technique_list.size());
  EXPECT_EQ(2, sampler_list.size());
}



// Tests of error cases --------------------------------------------------------

TEST_F(TechniqueParserTest, ParseEmptyString) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  String empty_fx_source = "";
  EXPECT_TRUE(ParseFxString(empty_fx_source,
                            &shader_source,
                            &sampler_list,
                            &technique_list,
                            &error_string));
  EXPECT_EQ(0, technique_list.size());
  EXPECT_EQ(String(""), shader_source);
  EXPECT_EQ(0, sampler_list.size());
}

TEST_F(TechniqueParserTest, ParseInvalidString) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  String invalid_fx_source = "$%^~ This is an invalid shader.";
  EXPECT_FALSE(ParseFxString(invalid_fx_source,
                             &shader_source,
                             &sampler_list,
                             &technique_list,
                             &error_string));
  EXPECT_EQ(0, technique_list.size());
  EXPECT_EQ(0, sampler_list.size());
  // TODO: make sure the string was rejected as an invalid HLSL
  // program and test the parser errors.
}

TEST_F(TechniqueParserTest, ParseInvalidFilename) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  String filepath = *g_program_path + "/unittest_data/invalid_filename.fx";
  EXPECT_FALSE(ParseFxFile(filepath,
                           &shader_source,
                           &sampler_list,
                           &technique_list,
                           &error_string));
  EXPECT_EQ(technique_list.size(), 0);
  EXPECT_EQ(shader_source, String(""));
  EXPECT_EQ(0, sampler_list.size());
}

TEST_F(TechniqueParserTest, ParseInvalidPassIdentifier) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  String invalid_pass_identifier_source = "technique { pass pass { } };";
  EXPECT_FALSE(ParseFxString(invalid_pass_identifier_source,
                             &shader_source,
                             &sampler_list,
                             &technique_list,
                             &error_string));
  EXPECT_EQ(1, technique_list.size());
  ASSERT_EQ(1, technique_list[0].pass.size());
  EXPECT_EQ("", technique_list[0].pass[0].name);
  EXPECT_EQ(shader_source, String(""));
  EXPECT_EQ(0, sampler_list.size());
}

TEST_F(TechniqueParserTest, ParseInvalidStateName) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  // NOTE: "FragmentShader" should read "FragmentProgram" or "PixelShader".
  String invalid_pass_identifier_source =
      "technique { pass { FragmentShader = compile ps_2_0 nothing(); } };";
  EXPECT_FALSE(ParseFxString(invalid_pass_identifier_source,
                             &shader_source,
                             &sampler_list,
                             &technique_list,
                             &error_string));
  EXPECT_EQ(1, technique_list.size());
  ASSERT_EQ(1, technique_list[0].pass.size());
  EXPECT_EQ("", technique_list[0].pass[0].name);
  EXPECT_EQ(shader_source, String(""));
  EXPECT_EQ(0, sampler_list.size());
}

TEST_F(TechniqueParserTest, ParseSampler) {
  String shader_source, error_string;
  TechniqueDeclarationList technique_list;
  SamplerStateList sampler_list;
  String filepath = *g_program_path + "/unittest_data/sampler_test.fx";
  EXPECT_TRUE(ParseFxFile(filepath,
                          &shader_source,
                          &sampler_list,
                          &technique_list,
                          &error_string));
  EXPECT_EQ(1, technique_list.size());
  ASSERT_EQ(1, technique_list[0].pass.size());
  ASSERT_EQ(1, sampler_list.size());
  EXPECT_EQ("Tex0", sampler_list[0].texture);
  EXPECT_EQ("Linear", sampler_list[0].min_filter);
  EXPECT_EQ("Point", sampler_list[0].mag_filter);
  EXPECT_EQ("None", sampler_list[0].mip_filter);
  EXPECT_EQ("Mirror", sampler_list[0].address_u);
  EXPECT_EQ("Wrap", sampler_list[0].address_v);
  EXPECT_EQ("Clamp", sampler_list[0].address_w);
  EXPECT_EQ("16", sampler_list[0].max_anisotropy);
  EXPECT_EQ("float4(1.0, 0.0, 0.0, 1.0)", sampler_list[0].border_color);
}

}  // namespace o3d
