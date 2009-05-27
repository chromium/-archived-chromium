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

float4x4 worldViewProjection : WORLDVIEWPROJECTION;

// The texture sampler is used to access the texture bitmap in the fragment
// shader.
sampler texSampler0;

// input parameters for our vertex shader
struct PixelShaderInput {
  float4 position : POSITION;
  float2 texcoord : TEXCOORD0;  // Texture coordinates
};

// input parameters for our pixel shader
struct VertexShaderInput {
  float4 position : POSITION;
  float2 texcoord : TEXCOORD0;  // Texture coordinates
};

/**
 * Our vertex shader
 */
PixelShaderInput vertexShaderFunction(VertexShaderInput input) {
  PixelShaderInput output;
  output.position = mul(input.position, worldViewProjection);
  output.texcoord = input.texcoord;
  return output;
}

/* Given the texture coordinates, our pixel shader grabs the corresponding
 * color from the texture.
 */
float4 pixelShaderFunction(PixelShaderInput input): COLOR {
  return tex2D(texSampler0, input.texcoord);
}

// Here we tell our effect file *which* functions are
// our vertex and pixel shaders.
// #o3d VertexShaderEntryPoint vertexShaderFunction
// #o3d PixelShaderEntryPoint pixelShaderFunction
// #o3d MatrixLoadOrder RowMajor
