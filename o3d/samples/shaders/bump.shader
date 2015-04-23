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

// The 4x4 world view projection matrix.
float4x4 worldViewProjection : WorldViewProjection;
float4x4 worldInverseTranspose : WorldInverseTranspose;
float4x4 world : World;
float4x4 viewInverse : ViewInverse;

// whether to use texture
float useTexture;

float3 lightWorldPos;
float4 lightIntensity;
float4 ambientIntensity;
float4 emissive;
float4 ambient;
float4 diffuse;
float4 specular;
float shininess;

sampler2D AmbientSampler;
sampler2D DiffuseSampler;
sampler2D BumpSampler;

// input parameters for our vertex shader
struct VertexShaderInput {
  float4 position : POSITION;
  float4 normal : NORMAL;
  float4 tangent : TANGENT;
  float4 binormal : BINORMAL;
  float2 texcoord0 : TEXCOORD0;
};

// input parameters for our pixel shader
// also the output parameters for our vertex shader
struct PixelShaderInput {
  float4 position : POSITION;
  float2 texcoord0 : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float3 binormal : TEXCOORD2;
  float3 tangent : TEXCOORD3;
  float3 worldPosition : TEXCOORD4;
};

PixelShaderInput vertexShaderFunction(VertexShaderInput input) {
  PixelShaderInput output;

  // Transform position into clip space.
  output.position = mul(input.position, worldViewProjection);

  // Transform the tangent frame into world space.
  output.tangent = mul(input.tangent, worldInverseTranspose).xyz;
  output.binormal = mul(input.binormal, worldInverseTranspose).xyz;
  output.normal = mul(input.normal, worldInverseTranspose).xyz;

  // Pass through the texture coordinates.
  output.texcoord0 = input.texcoord0;

  // Calculate surface position in world space. Used for lighting.
  output.worldPosition = mul(input.position, world).xyz;

  return output;
}

float4 pixelShaderFunction(PixelShaderInput input): COLOR {
  // Construct a transform from tangent space into world space.
  float3x3 tangentToWorld = float3x3(input.tangent,
                                     input.binormal,
                                     input.normal);

  // Read the tangent space normal from the normal map and remove the bias so
  // they are in the range [-0.5,0.5]. There is no need to scale by 2 because
  // the vector will soon be normalized.
  float3 tangentNormal = tex2D(BumpSampler, input.texcoord0.xy).xyz -
      float3(0.5, 0.5, 0.5);

  // Transform the normal into world space.
  float3 worldNormal = mul(tangentNormal, tangentToWorld);
  worldNormal = normalize(worldNormal);

  // Read the diffuse and ambient colors.
  float4 textureAmbient = float4(1, 1, 1, 1);
  float4 textureDiffuse = float4(1, 1, 1, 1);
  if (useTexture == 1) {
    textureAmbient = tex2D(AmbientSampler, input.texcoord0.xy);
    textureDiffuse = tex2D(DiffuseSampler, input.texcoord0.xy);
  }

  // Apply lighting in world space in case the world transform contains scaling.
  float3 surfaceToLight = normalize(lightWorldPos.xyz -
      input.worldPosition.xyz);
  float3 surfaceToView = normalize(viewInverse[3].xyz - input.worldPosition);
  float3 halfVector = normalize(surfaceToLight + surfaceToView);
  float4 litResult = lit(dot(worldNormal, surfaceToLight),
                         dot(worldNormal, halfVector), shininess);
  float4 outColor = ambientIntensity * ambient * textureAmbient;
  outColor += lightIntensity * (diffuse * textureDiffuse * litResult.y +
      specular * litResult.z);
  outColor += emissive;
  return float4(outColor.rgb, diffuse.a * textureDiffuse.a);
}

// Here we tell our effect file *which* functions are
// our vertex and pixel shaders.

// #o3d VertexShaderEntryPoint vertexShaderFunction
// #o3d PixelShaderEntryPoint pixelShaderFunction
// #o3d MatrixLoadOrder RowMajor
