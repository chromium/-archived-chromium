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

// This shader expects to be passed vertices that are part of an x-z plane mesh
// because it assumes the normal for each vertex before being offset by the
// animation is 0, 1, 0.

uniform float4x4 viewProjection : VIEWPROJECTION;
uniform float3 lightWorldPos;
uniform float4 lightIntensity;
uniform float4x4 world : WORLD;
uniform float4x4 viewInverse : VIEWINVERSE;
uniform float4x4 worldInverseTranspose : WORLDINVERSETRANSPOSE;
uniform float4 ambientIntensity;
uniform float4 emissive;
uniform float4 ambient;
uniform float4 diffuse;
uniform float4 specular;
uniform float shininess;
uniform float time;

struct VertexShaderInput {
  float4 position : POSITION;
};

struct PixelShaderInput {
  float4 position : POSITION;
  float3 normal : TEXCOORD1;
  float3 worldPosition : TEXCOORD4;
};

PixelShaderInput vertexShaderFunction(VertexShaderInput input) {
  PixelShaderInput output;
  float4 worldPosition = mul(input.position, world);
  float animValue = time + (worldPosition.x + worldPosition.z) * 0.4;
  float animSin = sin(animValue);
  float animCos = cos(animValue);
  float4 position = float4(
      worldPosition.x,
      worldPosition.y + animSin,
      worldPosition.z,
      1);
  output.position = mul(position, viewProjection);
  float3 normal = normalize(float3(animCos, abs(animSin), animCos));
  output.normal = mul(float4(normal, 0), worldInverseTranspose).xyz;
  output.worldPosition = position.xyz;

  return output;
}

float4 pixelShaderFunction(PixelShaderInput input) : COLOR {
  float3 surfaceToLight = normalize(lightWorldPos - input.worldPosition);
  float3 worldNormal = normalize(input.normal);
  float3 surfaceToView = normalize(viewInverse[3].xyz - input.worldPosition);
  float3 halfVector = normalize(surfaceToLight + surfaceToView);
  float4 litResult = lit(dot(worldNormal, surfaceToLight),
                         dot(worldNormal, halfVector), shininess);
  float4 outColor = ambientIntensity * ambient;
  outColor += lightIntensity * (diffuse * litResult.y +
      specular * litResult.z);
  outColor += emissive;
  return float4(outColor.rgb, diffuse.a);
}

// #o3d VertexShaderEntryPoint vertexShaderFunction
// #o3d PixelShaderEntryPoint pixelShaderFunction
// #o3d MatrixLoadOrder RowMajor
