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


// This shader takes a Y'UV420p image as a single greyscale plane, and
// converts it to RGB by sampling the correct parts of the image, and
// by converting the colorspace to RGB on the fly.

// Projection matrix for the camera.
float4x4 worldViewProjection : WorldViewProjection;

// These represent the image dimensions of the SOURCE IMAGE (not the
// Y'UV420p image).  This is the same as the dimensions of the Y'
// portion of the Y'UV420p image.  They are set from JavaScript.
float imageWidth;
float imageHeight;

// This is the texture sampler where the greyscale Y'UV420p image is
// accessed.
sampler textureSampler;

// These are the input/output parameters for our vertex shader
struct VertexShaderInput {
  float4 position : POSITION;
  float2 texcoord : TEXCOORD0;  // Texture coordinates
};

// These are the input/output parameters for our pixel shader.
struct PixelShaderInput {
  float4 position : POSITION;
  float2 texcoord : TEXCOORD0;  // Texture coordinates
};

/**
 * This fetches an individual Y pixel from the image, given the current
 * texture coordinates (which range from 0 to 1 on the source texture
 * image).  They are mapped to the portion of the image that contains
 * the Y component.
 *
 * @param position This is the position of the main image that we're
 *        trying to render, in parametric coordinates.
 */
float getYPixel(float2 position) {
  position.y = (position.y * 2.0 / 3.0) + (1.0 / 3.0);
  return tex2D(textureSampler, position).x;
}

/**
 * This does the crazy work of calculating the planar position (the
 * position in the byte stream of the image) of the U or V pixel, and
 * then converting that back to x and y coordinates, so that we can
 * account for the fact that V is appended to U in the image, and the
 * complications that causes (see below for a diagram).
 *
 * @param position This is the position of the main image that we're
 *        trying to render, in pixels.
 *
 * @param planarOffset This is an offset to add to the planar address
 *        we calculate so that we can find the U image after the V
 *        image.
 */
float2 mapCommon(float2 position, float planarOffset) {
  planarOffset += (imageWidth * floor(position.y / 2.0)) / 2.0 +
                  floor((imageWidth - 1.0 - position.x) / 2.0);
  float x = int(imageWidth - 1.0 - floor(fmod(planarOffset, imageWidth)));
  float y = int(floor(planarOffset / imageWidth));
  return float2((x + 0.5) / imageWidth, (y + 0.5) / (1.5 * imageHeight));
}

/**
 * This is a helper function for mapping pixel locations to a texture
 * coordinate for the U plane.
 *
 * @param position This is the position of the main image that we're
 *        trying to render, in pixels.
 */
float2 mapU(float2 position) {
  float planarOffset = (imageWidth * imageHeight) / 4.0;
  return mapCommon(position, planarOffset);
}

/**
 * This is a helper function for mapping pixel locations to a texture
 * coordinate for the V plane.
 *
 * @param position This is the position of the main image that we're
 *        trying to render, in pixels.
 */
float2 mapV(float2 position) {
  return mapCommon(position, 0.0);
}

/**
 * The vertex shader does nothing but returns the position of the
 * vertex using the world view projection matrix.
 */
PixelShaderInput vertexShaderMain(VertexShaderInput input) {
  PixelShaderInput output;
  output.position = mul(input.position, worldViewProjection);

  output.texcoord = input.texcoord;
  return output;
}

/**
 * Given the texture coordinates, our pixel shader grabs the right
 * value from each channel of the source image, converts it from Y'UV
 * to RGB, and returns the result.
 *
 * Each U and V pixel provides color information for a 2x2 block of Y
 * pixels.  The U and V planes are just appended to the Y image.
 *
 * For images that have a height divisible by 4, things work out nicely.
 * For images that are merely divisible by 2, it's not so nice
 * (and YUV420 doesn't work for image sizes not divisible by 2).
 *
 * Here is a 6x6 image, with the layout of the planes of U and V.
 * Notice that the V plane starts halfway through the last scanline
 * that has U on it.
 *
 * 1  +---+---+---+---+---+---+
 *    | Y | Y | Y | Y | Y | Y |
 *    +---+---+---+---+---+---+
 *    | Y | Y | Y | Y | Y | Y |
 *    +---+---+---+---+---+---+
 *    | Y | Y | Y | Y | Y | Y |
 *    +---+---+---+---+---+---+
 *    | Y | Y | Y | Y | Y | Y |
 *    +---+---+---+---+---+---+
 *    | Y | Y | Y | Y | Y | Y |
 *    +---+---+---+---+---+---+
 *    | Y | Y | Y | Y | Y | Y |
 * .3 +---+---+---+---+---+---+
 *    | U | U | U | U | U | U |
 *    +---+---+---+---+---+---+
 *    | U | U | U | V | V | V |
 *    +---+---+---+---+---+---+
 *    | V | V | V | V | V | V |
 * 0  +---+---+---+---+---+---+
 *    0                       1
 *
 * Here is a 4x4 image, where the U and V planes are nicely split into
 * separable blocks.
 *
 * 1  +---+---+---+---+
 *    | Y | Y | Y | Y |
 *    +---+---+---+---+
 *    | Y | Y | Y | Y |
 *    +---+---+---+---+
 *    | Y | Y | Y | Y |
 *    +---+---+---+---+
 *    | Y | Y | Y | Y |
 * .3 +---+---+---+---+
 *    | U | U | U | U |
 *    +---+---+---+---+
 *    | V | V | V | V |
 * 0  +---+---+---+---+
 *    0               1
 *
 */
float4 pixelShaderMain(PixelShaderInput input): COLOR {
  // Calculate what image pixel we're on, since we have to calculate
  // the location in the image stream, using floor in several places
  // which makes it hard to use parametric coordinates.
  float2 pixelPosition = float2(floor(imageWidth * input.texcoord.x),
                                floor(imageHeight * input.texcoord.y));

  // We can use the parametric coordinates to get the Y channel, since it's
  // a relatively normal image.
  float yChannel = getYPixel(input.texcoord);

  // As noted above, the U and V planes are smashed onto the end of
  // the image in an odd way (in our 2D texture mapping, at least), so
  // these mapping functions take care of that oddness.
  float uChannel = tex2D(textureSampler, mapU(pixelPosition)).x;
  float vChannel = tex2D(textureSampler, mapV(pixelPosition)).x;

  // This does the colorspace conversion from Y'UV to RGB as a matrix
  // multiply.  It also does the offset of the U and V channels from
  // [0,1] to [-.5,.5] as part of the transform.
  float4 channels = float4(yChannel, uChannel, vChannel, 1.0);
  float3x4 conversion = float3x4(1.0,  0.0,    1.402, -0.701,
                                 1.0, -0.344, -0.714,  0.529,
                                 1.0,  1.772,  0.0,   -0.886);
  float3 rgb = mul(conversion, channels);

  // This is another Y'UV transform that can be used, but it doesn't
  // accurately transform my source image.  Your images may well fare
  // better with it, however, considering they come from a different
  // source, and because I'm not sure that my original was converted
  // to Y'UV420p with the same RGB->YUV (or YCrCb) conversion as
  // yours.
  //
  // float4 channels = float4(yChannel, uChannel, vChannel, 1.0);
  // float3x4 conversion = float3x4(1.0,  0.0,      1.13983, -0.569915,
  //                                1.0, -0.39465, -0.58060,  0.487625,
  //                                1.0,  2.03211,  0.0,     -1.016055);
  // float3 rgb = mul(conversion, channels);

  return float4(rgb, 1.0);
}

// Here we tell our effect file *which* functions are
// our vertex and pixel shaders.
// #o3d VertexShaderEntryPoint vertexShaderMain
// #o3d PixelShaderEntryPoint pixelShaderMain
// #o3d MatrixLoadOrder RowMajor
