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


// This file contains the interface class for the low-level graphics API
// (GAPI).

#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_GAPI_INTERFACE_H__
#define O3D_COMMAND_BUFFER_COMMON_CROSS_GAPI_INTERFACE_H__

#include "command_buffer/common/cross/buffer_sync_api.h"
#include "command_buffer/common/cross/resource.h"

namespace o3d {
namespace command_buffer {

// RBGA color definition.
struct RGBA {
  float red;
  float green;
  float blue;
  float alpha;
};

// This class defines the low-level graphics API, as a pure interface class.
class GAPIInterface {
 public:
  typedef BufferSyncInterface::ParseError ParseError;

  GAPIInterface() {}
  virtual ~GAPIInterface() {}

  // Initializes the graphics context.
  // Returns:
  //   true if successful.
  virtual bool Initialize() = 0;

  // Destroys the graphics context.
  virtual void Destroy() = 0;

  // Bit definitions for buffers to clear.
  enum ClearBuffer {
    COLOR = 0x1,
    DEPTH = 0x2,
    STENCIL = 0x4,
    ALL_BUFFERS = COLOR | DEPTH | STENCIL
  };

  // Primitive type for Draw and DrawIndexed.
  enum PrimitiveType {
    POINTS,
    LINES,
    LINE_STRIPS,
    TRIANGLES,
    TRIANGLE_STRIPS,
    TRIANGLE_FANS,
    MAX_PRIMITIVE_TYPE
  };

  // Polygon mode for SetPolygonRaster
  enum PolygonMode {
    POLYGON_MODE_POINTS,
    POLYGON_MODE_LINES,
    POLYGON_MODE_FILL,
    NUM_POLYGON_MODE
  };

  // Face culling mode for SetPolygonRaster
  enum FaceCullMode {
    CULL_NONE,
    CULL_CW,
    CULL_CCW,
    NUM_FACE_CULL_MODE
  };

  // Comparison function for alpha or depth test
  enum Comparison {
    NEVER,
    LESS,
    EQUAL,
    LEQUAL,
    GREATER,
    NOT_EQUAL,
    GEQUAL,
    ALWAYS,
    NUM_COMPARISON
  };

  // Stencil operation
  enum StencilOp {
    KEEP,
    ZERO,
    REPLACE,
    INC_NO_WRAP,
    DEC_NO_WRAP,
    INVERT,
    INC_WRAP,
    DEC_WRAP,
    NUM_STENCIL_OP
  };

  // Blend Equation
  enum BlendEq {
    BLEND_EQ_ADD,
    BLEND_EQ_SUB,
    BLEND_EQ_REV_SUB,
    BLEND_EQ_MIN,
    BLEND_EQ_MAX,
    NUM_BLEND_EQ
  };

  // Blend Funtion
  enum BlendFunc {
    BLEND_FUNC_ZERO,
    BLEND_FUNC_ONE,
    BLEND_FUNC_SRC_COLOR,
    BLEND_FUNC_INV_SRC_COLOR,
    BLEND_FUNC_SRC_ALPHA,
    BLEND_FUNC_INV_SRC_ALPHA,
    BLEND_FUNC_DST_ALPHA,
    BLEND_FUNC_INV_DST_ALPHA,
    BLEND_FUNC_DST_COLOR,
    BLEND_FUNC_INV_DST_COLOR,
    BLEND_FUNC_SRC_ALPHA_SATUTRATE,
    BLEND_FUNC_BLEND_COLOR,
    BLEND_FUNC_INV_BLEND_COLOR,
    NUM_BLEND_FUNC
  };

  // Starts a frame. Rendering should occur between BeginFrame and EndFrame.
  virtual void BeginFrame() = 0;

  // Ends the frame, and bring the back buffer to front. Rendering should occur
  // between BeginFrame and EndFrame.
  virtual void EndFrame() = 0;

  // Clear buffers, filling them with a constant value.
  // Parameters:
  //   buffers: which buffers to clear. Can be a combination (bitwise or) of
  //     values from ClearBuffer.
  //   color: the RGBA color to clear the color target with.
  //   depth: the depth to clear the depth buffer with.
  //   stencil: the stencil value to clear the stencil buffer with.
  virtual void Clear(unsigned int buffers,
                     const RGBA &color,
                     float depth,
                     unsigned int stencil) = 0;

  // Creates a vertex buffer.
  // Parameters:
  //   id: the resource ID for the new vertex buffer.
  //   size: the size of the vertex buffer, in bytes.
  //   flags: the vertex buffer flags, as a combination of vertex_buffer::Flags
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError CreateVertexBuffer(ResourceID id,
                                        unsigned int size,
                                        unsigned int flags) = 0;

  // Destroys a vertex buffer.
  // Parameters:
  //   id: the resource ID of the vertex buffer.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if an invalid vertex buffer
  //   ID was passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError DestroyVertexBuffer(ResourceID id) = 0;

  // Sets data into a vertex buffer.
  // Parameters:
  //   id: the resource ID of the vertex buffer.
  //   offset: the offset into the vertex buffer where to place the data.
  //   size: the size of the data.
  //   data: the source data.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments were
  //   passed: invalid resource ID, or offset or size out of range.
  //   BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError SetVertexBufferData(ResourceID id,
                                         unsigned int offset,
                                         unsigned int size,
                                         const void *data) = 0;

  // Gets data from a vertex buffer.
  // Parameters:
  //   id: the resource ID of the vertex buffer.
  //   offset: the offset into the vertex buffer where to get the data.
  //   size: the size of the data.
  //   data: the destination buffer.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments were
  //   passed: invalid resource ID, or offset or size out of range.
  //   BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError GetVertexBufferData(ResourceID id,
                                         unsigned int offset,
                                         unsigned int size,
                                         void *data) = 0;

  // Creates an index buffer.
  // Parameters:
  //   id: the resource ID for the new index buffer.
  //   size: the size of the index buffer, in bytes.
  //   flags: the index buffer flags, as a combination of index_buffer::Flags.
  //          Note that indices are 16 bits unless the index_buffer::INDEX_32BIT
  //          flag is specified.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError CreateIndexBuffer(ResourceID id,
                                       unsigned int size,
                                       unsigned int flags) = 0;

  // Destroys an index buffer.
  // Parameters:
  //   id: the resource ID of the index buffer.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if an invalid index buffer
  //   ID was passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError DestroyIndexBuffer(ResourceID id) = 0;

  // Sets data into an index buffer.
  // Parameters:
  //   id: the resource ID of the index buffer.
  //   offset: the offset into the index buffer where to place the data.
  //   size: the size of the data.
  //   data: the source data.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments were
  //   passed: invalid resource ID, or offset or size out of range.
  //   BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError SetIndexBufferData(ResourceID id,
                                        unsigned int offset,
                                        unsigned int size,
                                        const void *data) = 0;

  // Gets data from an index buffer.
  // Parameters:
  //   id: the resource ID of the index buffer.
  //   offset: the offset into the index buffer where to get the data.
  //   size: the size of the data.
  //   data: the destination buffer.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments were
  //   passed: invalid resource ID, or offset or size out of range.
  //   BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError GetIndexBufferData(ResourceID id,
                                        unsigned int offset,
                                        unsigned int size,
                                        void *data) = 0;

  // Creates a vertex struct. A vertex struct describes the input vertex
  // attribute streams.
  // Parameters:
  //   id: the resource ID of the vertex struct.
  //   input_count: the number of input vertex attributes.
  // returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError CreateVertexStruct(ResourceID id,
                                        unsigned int input_count) = 0;

  // Destroys a vertex struct.
  // Parameters:
  //   id: the resource ID of the vertex struct.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if an invalid vertex struct
  //   ID was passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError DestroyVertexStruct(ResourceID id) = 0;

  // Sets an input into a vertex struct.
  // Parameters:
  //   vertex_struct_id: the resource ID of the vertex struct.
  //   input_index: the index of the input being set.
  //   vertex_buffer_id: the resource ID of the vertex buffer containing the
  //                     data.
  //   offset: the offset into the vertex buffer of the input data, in bytes.
  //   stride: the stride of the input data, in bytes.
  //   type: the type of the input data.
  //   semantic: the semantic of the input.
  //   semantic_index: the semantic index of the input.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError SetVertexInput(ResourceID vertex_struct_id,
                                    unsigned int input_index,
                                    ResourceID vertex_buffer_id,
                                    unsigned int offset,
                                    unsigned int stride,
                                    vertex_struct::Type type,
                                    vertex_struct::Semantic semantic,
                                    unsigned int semantic_index) = 0;

  // Sets the current vertex struct for drawing.
  // Parameters:
  //   id: the resource ID of the vertex struct.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed (invalid vertex struct), BufferSyncInterface::PARSE_NO_ERROR
  //   otherwise.
  virtual ParseError SetVertexStruct(ResourceID id) = 0;

  // Draws primitives, using the current vertex struct and the current effect.
  // Parameters:
  //   primitive_type: the type of primitive to draw.
  //   first: the index of the first vertex.
  //   count: the number of primitives to draw.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError Draw(PrimitiveType primitive_type,
                          unsigned int first,
                          unsigned int count) = 0;

  // Draws primitives, using the current vertex struct and the current effect,
  // as well as an index buffer.
  // Parameters:
  //   primitive_type: the type of primitive to draw.
  //   index_buffer_id: the resource ID of the index buffer.
  //   first: the index into the index buffer of the first index to draw.
  //   count: the number of primitives to draw.
  //   min_index: the lowest index being drawn.
  //   max_index: the highest index being drawn.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError DrawIndexed(PrimitiveType primitive_type,
                                 ResourceID index_buffer_id,
                                 unsigned int first,
                                 unsigned int count,
                                 unsigned int min_index,
                                 unsigned int max_index) = 0;

  // Creates an effect, from source code.
  // Parameters:
  //   id: the resource ID of the effect.
  //   size: the size of data.
  //   data: the source code for the effect.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed or the effect failed to compile,
  //   BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError CreateEffect(ResourceID id,
                                  unsigned int size,
                                  const void *data) = 0;

  // Destroys an effect.
  // Parameters:
  //   id: the resource ID of the effect.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if an invalid effect ID
  //   was passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError DestroyEffect(ResourceID id) = 0;

  // Sets the active effect for drawing.
  // Parameters:
  //   id: the resource ID of the effect.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError SetEffect(ResourceID id) = 0;

  // Gets the number of parameters in an effect, returning it in a memory
  // buffer as a Uint32.
  // Parameters:
  //   id: the resource ID of the effect.
  //   size: the size of the data buffer. Must be at least 4 (the size of the
  //   Uint32).
  //   data: the buffer receiving the data.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError GetParamCount(ResourceID id,
                                   unsigned int size,
                                   void *data) = 0;

  // Creates an effect parameter by index.
  // Parameters:
  //   param_id: the resource ID of the parameter being created.
  //   effect_id: the resource ID of the effect containing the parameter.
  //   data_type: the data type for the parameter. Must match the data type in
  //     the effect source.
  //   index: the index of the parameter.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, such as invalid effect ID, unmatching data type or invalid
  //   index, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError CreateParam(ResourceID param_id,
                                 ResourceID effect_id,
                                 unsigned int index) = 0;

  // Creates an effect parameter by name.
  // Parameters:
  //   param_id: the resource ID of the parameter being created.
  //   effect_id: the resource ID of the effect containing the parameter.
  //   data_type: the data type for the parameter. Must match the data type in
  //     the effect source.
  //   size: the size of the parameter name.
  //   name: the parameter name, as an array of char. Doesn't have to be
  //     nul-terminated (though nul will terminate the string).
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, such as invalid effect ID, unmatching data type or no parameter
  //   was found with this name, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError CreateParamByName(ResourceID param_id,
                                       ResourceID effect_id,
                                       unsigned int size,
                                       const void *name) = 0;

  // Destroys an effect parameter.
  // Parameters:
  //   id: the resource ID of the parameter.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if an invalid parameter ID
  //   was passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError DestroyParam(ResourceID id) = 0;

  // Sets the effect parameter data.
  // Parameters:
  //   id: the resource ID of the parameter.
  //   size: the size of the data. Must be at least the size of the parameter
  //     as described by its type.
  //   data: the parameter data.
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, such as invalid parameter ID, or unmatching data size,
  //   BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError SetParamData(ResourceID id,
                                  unsigned int size,
                                  const void *data) = 0;

  // Gets the parameter description, storing it into a memory buffer. The
  // parameter is described by a effect_param::Desc structure. The size must be
  // at least the size of that structure. The name and semantic fields are only
  // filled if the character strings fit into the memory buffer. In any case,
  // the size field (in the effect_param::Desc) is filled with the size needed
  // to fill in the structure, the name and the semantic (if any). Thus to get
  // the complete information, GetParamDesc can be called twice, once to get
  // the size, and, after allocating a big enough buffer, again to fill in the
  // complete information including the text strings.
  // Parameters:
  //   id: the resource ID of the parameter.
  //   size: the size of the memory buffer that wil receive the parameter
  //     description. Must be at least sizeof(effect_param::Desc).
  //   data: the memory buffer.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, such as invalid parameter ID, or unsufficient data size,
  //   BufferSyncInterface::PARSE_NO_ERROR otherwise. Note that
  //   BufferSyncInterface::PARSE_NO_ERROR will be returned if the structure
  //   itself fits, not necessarily the names. To make sure all the information
  //   is available, the caller should compare the returned size member of the
  //   effect_param::Desc structure to the size parameter passed in.
  virtual ParseError GetParamDesc(ResourceID id,
                                  unsigned int size,
                                  void *data) = 0;

  // Gets the number of input streams for an effect, returning it in a memory
  // buffer as a Uint32.
  // Parameters:
  //   id: the resource ID of the effect.
  //   size: the size of the data buffer. Must be at least 4 (the size of the
  //   Uint32).
  //   data: the buffer receiving the data.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError GetStreamCount(ResourceID id,
                                    unsigned int size,
                                    void *data) = 0;

  // Gets the stream semantics, storing them in the data buffer. The stream
  // is described by an effect_stream::Desc structure which contains a
  // semantic type and a semantic index.
  // Parameters:
  //   id: the resource ID of the effect.
  //   index: which stream semantic to get
  //   size: the size of the data buffer. Must be at least 8 (the size of two
  //   Uint32).
  //   data: the buffer receiving the data.
  virtual ParseError GetStreamDesc(ResourceID id,
                                   unsigned int index,
                                   unsigned int size,
                                   void *data) = 0;

  // Creates a 2D texture resource.
  // Parameters:
  //   id: the resource ID of the texture.
  //   width: the texture width. Must be positive.
  //   height: the texture height. Must be positive.
  //   levels: the number of mipmap levels in the texture, or 0 to use the
  //     maximum.
  //   format: the format of the texels in the texture.
  //   flags: the texture flags, as a combination of texture::Flags.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError CreateTexture2D(ResourceID id,
                                     unsigned int width,
                                     unsigned int height,
                                     unsigned int levels,
                                     texture::Format format,
                                     unsigned int flags) = 0;

  // Creates a 3D texture resource.
  // Parameters:
  //   id: the resource ID of the texture.
  //   width: the texture width. Must be positive.
  //   height: the texture height. Must be positive.
  //   depth: the texture depth. Must be positive.
  //   levels: the number of mipmap levels in the texture, or 0 to use the
  //     maximum.
  //   format: the format of the pixels in the texture.
  //   flags: the texture flags, as a combination of texture::Flags.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError CreateTexture3D(ResourceID id,
                                     unsigned int width,
                                     unsigned int height,
                                     unsigned int depth,
                                     unsigned int levels,
                                     texture::Format format,
                                     unsigned int flags) = 0;

  // Creates a cube map texture resource.
  // Parameters:
  //   id: the resource ID of the texture.
  //   side: the texture side length. Must be positive.
  //   levels: the number of mipmap levels in the texture, or 0 to use the
  //     maximum.
  //   format: the format of the pixels in the texture.
  //   flags: the texture flags, as a combination of texture::Flags.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError CreateTextureCube(ResourceID id,
                                       unsigned int side,
                                       unsigned int levels,
                                       texture::Format format,
                                       unsigned int flags) = 0;

  // Sets texel data into a texture resource. This is a common function for
  // each of the texture types, but some restrictions exist based on the
  // texture type. The specified rectangle or volume of data, defined by x, y,
  // width, height and possibly z and depth must fit into the selected mimmap
  // level. Data is encoded by rows of 2D blocks, whose size depends on the
  // texel format, usually 1x1 texel, but can be 4x4 for DXT* formats. See
  // texture::GetBytesPerBlock, texture::GetBlockSizeX and
  // texture::GetBlockSizeY.
  // Parameters:
  //   id: the resource ID of the texture.
  //   x: the x position of the texel corresponding to the first byte of data.
  //   y: the y position of the texel corresponding to the first byte of data.
  //   z: the z position of the texel corresponding to the first byte of data.
  //     Must be 0 for non-3D textures.
  //   width: the width of the data rectangle/volume.
  //   height: the height of the data rectangle/volume.
  //   depth: the depth of the data volume. Must be 1 for non-3D textures.
  //   level: the mipmap level to put the data into.
  //   face: which face of the cube to put the data into. Is ignored for
  //     non-cube map textures.
  //   row_pitch: the number of bytes between two consecutive rows of blocks,
  //     in the source data.
  //   slice_pitch: the number of bytes between two consecutive slices of
  //     blocks, in the source data. Is ignored for non-3D textures.
  //   size: the size of the data.
  //   data: the texel data.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, for example invalid size, or out-of-bounds rectangle/volume,
  //   BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError SetTextureData(ResourceID id,
                                    unsigned int x,
                                    unsigned int y,
                                    unsigned int z,
                                    unsigned int width,
                                    unsigned int height,
                                    unsigned int depth,
                                    unsigned int level,
                                    texture::Face face,
                                    unsigned int pitch,
                                    unsigned int slice_pitch,
                                    unsigned int size,
                                    const void *data) = 0;

  // Gets texel data from a texture resource. This is a common function for
  // each of the texture types, but some restrictions exist based on the
  // texture type. The specified rectangle or volume of data, defined by x, y,
  // width, height and possibly z and depth must fit into the selected mimmap
  // level. Data is encoded by rows of 2D blocks, whose size depends on the
  // texel format, usually 1x1 texel, but can be 4x4 for DXT* formats. See
  // texture::GetBytesPerBlock, texture::GetBlockSizeX and
  // texture::GetBlockSizeY.
  // Parameters:
  //   id: the resource ID of the texture.
  //   x: the x position of the texel corresponding to the first byte of data.
  //   y: the y position of the texel corresponding to the first byte of data.
  //   z: the z position of the texel corresponding to the first byte of data.
  //     Must be 0 for non-3D textures.
  //   width: the width of the data rectangle/volume.
  //   height: the height of the data rectangle/volume.
  //   depth: the depth of the data volume. Must be 1 for non-3D textures.
  //   level: the mipmap level to put the data into.
  //   face: which face of the cube to put the data into. Is ignored for
  //     non-cube map textures.
  //   row_pitch: the number of bytes between two consecutive rows of blocks,
  //     in the destination buffer.
  //   slice_pitch: the number of bytes between two consecutive slices of
  //     blocks, in the destination buffer. Is ignored for non-3D textures.
  //   size: the size of the data.
  //   data: the destination buffer.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if invalid arguments are
  //   passed, for example invalid size, or out-of-bounds rectangle/volume,
  //   BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError GetTextureData(ResourceID id,
                                    unsigned int x,
                                    unsigned int y,
                                    unsigned int z,
                                    unsigned int width,
                                    unsigned int height,
                                    unsigned int depth,
                                    unsigned int level,
                                    texture::Face face,
                                    unsigned int pitch,
                                    unsigned int slice_pitch,
                                    unsigned int size,
                                    void *data) = 0;

  // Destroys a texture resource.
  // Parameters:
  //   id: the resource ID of the texture.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if an invalid texture
  //   resource ID was passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError DestroyTexture(ResourceID id) = 0;

  // Creates a sampler resource.
  // Parameters:
  //   id: the resource ID of the sampler.
  // Returns:
  //   BufferSyncInterface::PARSE_NO_ERROR.
  virtual ParseError CreateSampler(ResourceID id) = 0;

  // Destroys a sampler resource.
  // Parameters:
  //   id: the resource ID of the sampler.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if an invalid sampler
  //   resource ID was passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError DestroySampler(ResourceID id) = 0;

  // Sets the states in a sampler resource.
  // Parameters:
  //   id: the resource ID of the sampler.
  //   addressing_u: the addressing mode for the U coordinate.
  //   addressing_v: the addressing mode for the V coordinate.
  //   addressing_w: the addressing mode for the W coordinate.
  //   mag_filter: the filtering mode when magnifying textures.
  //   min_filter: the filtering mode when minifying textures.
  //   mip_filter: the filtering mode for mip-map interpolation textures.
  //   max_anisotropy: the maximum anisotropy.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if an invalid sampler
  //   resource ID was passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError SetSamplerStates(ResourceID id,
                                      sampler::AddressingMode addressing_u,
                                      sampler::AddressingMode addressing_v,
                                      sampler::AddressingMode addressing_w,
                                      sampler::FilteringMode mag_filter,
                                      sampler::FilteringMode min_filter,
                                      sampler::FilteringMode mip_filter,
                                      unsigned int max_anisotropy) = 0;

  // Sets the color of border pixels.
  // Parameters:
  //   id: the resource ID of the sampler.
  //   color: the border color.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if an invalid sampler
  //   resource ID was passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError SetSamplerBorderColor(ResourceID id,
                                           const RGBA &color) = 0;

  // Sets the texture resource used by a sampler resource.
  // Parameters:
  //   id: the resource ID of the sampler.
  //   texture_id: the resource id of the texture.
  // Returns:
  //   BufferSyncInterface::PARSE_INVALID_ARGUMENTS if an invalid sampler
  //   resource ID was passed, BufferSyncInterface::PARSE_NO_ERROR otherwise.
  virtual ParseError SetSamplerTexture(ResourceID id,
                                       ResourceID texture_id) = 0;

  // Sets the viewport, and depth range.
  // Parameters:
  //   x, y: upper left corner of the viewport.
  //   width, height: dimensions of the viewport.
  //   z_min, z_max: depth range.
  virtual void SetViewport(unsigned int x,
                           unsigned int y,
                           unsigned int width,
                           unsigned int height,
                           float z_min,
                           float z_max) = 0;

  // Sets the scissor test enable flag and rectangle.
  // Parameters:
  //   enable: whether or not scissor test is enabled.
  //   x, y: upper left corner of the scissor rectangle.
  //   width, height: dimensions of the scissor rectangle.
  virtual void SetScissor(bool enable,
                          unsigned int x,
                          unsigned int y,
                          unsigned int width,
                          unsigned int height) = 0;

  // Sets the point and line rasterization state.
  // Parameters:
  //   line_smooth: Whether or not line anti-aliasing is enabled.
  //   point_sprite: Whether or not point sprites are enabled.
  //   point_size: The point size.
  virtual void SetPointLineRaster(bool line_smooth,
                                  bool point_sprite,
                                  float point_size) = 0;

  // Sets the polygon rasterization state.
  // Parameters:
  //   fill_mode: The polygon filling mode.
  //   cull_mode: The polygon face culling mode.
  virtual void SetPolygonRaster(PolygonMode fill_mode,
                                FaceCullMode cull_mode) = 0;

  // Sets the polygon offset state. Polygon offset is enabled if slope_factor
  // or units is not 0.
  // The applied offset (in window coordinates) is:
  // o = max_slope * slope_factor + r * units
  // Where max_slope is the maximum slope of the polygon (in window
  // coordinates again), and r is the minimum resolvable z unit.
  // Parameters:
  //   slope_factor: slope factor for the offset.
  //   units: constant factor for the offset.
  virtual void SetPolygonOffset(float slope_factor, float units) = 0;

  // Sets the alpha test states.
  // Parameters:
  //   enable: alpha test enable state.
  //   reference: reference value for comparison.
  //   comp: alpha comparison function.
  virtual void SetAlphaTest(bool enable,
                            float reference,
                            Comparison comp) = 0;

  // Sets the depth test states.
  // Note: if the depth test is disabled, z values are not written to the z
  // buffer (i.e enable/ALWAYS is different from disable/*).
  // Parameters:
  //   enable: depth test enable state.
  //   write_enable: depth write enable state.
  //   comp: depth comparison function.
  virtual void SetDepthTest(bool enable,
                            bool write_enable,
                            Comparison comp) = 0;

  // Sets the stencil test states.
  // Parameters:
  //   enable: stencil test enable state.
  //   separate_ccw: whether or not counter-clockwise faces use separate
  //     functions/operations (2-sided stencil).
  //   write_mask: stencil write mask.
  //   compare_mask: stencil compare mask.
  //   ref: stencil reference value.
  //   func_ops: stencil test function and operations for both clockwise and
  //     counter-clockwise faces. This is a bitfield following the following
  //     description (little-endian addressing):
  //     bits 0  - 11: clockwise functions/operations
  //     bits 12 - 15: must be 0.
  //     bits 16 - 28: counter-clockwise functions/operations
  //     bits 29 - 32: must be 0.
  virtual void SetStencilTest(bool enable,
                              bool separate_ccw,
                              unsigned int write_mask,
                              unsigned int compare_mask,
                              unsigned int ref,
                              Uint32 func_ops) = 0;

  // Sets the color write paramters.
  // Parameters:
  //   red: enable red write.
  //   green: enable green write.
  //   blue: enable blue write.
  //   alpha: enable alpha write.
  //   dither: enable dithering.
  virtual void SetColorWrite(bool red,
                             bool green,
                             bool blue,
                             bool alpha,
                             bool dither) = 0;

  // Sets the blending mode.
  // Parameters:
  //   enable: whether or not to enable blending.
  //   separate_alpha: whether or not alpha uses separate Equation/Functions
  //     (if false, it uses the color ones).
  //   color_eq: the equation for blending of color values.
  //   color_src_func: the source function for blending of color values.
  //   color_dst_func: the destination function for blending of color values.
  //   alpha_eq: the equation for blending of alpha values.
  //   alpha_src_func: the source function for blending of alpha values.
  //   alpha_dst_func: the destination function for blending of alpha values.
  virtual void SetBlending(bool enable,
                           bool separate_alpha,
                           BlendEq color_eq,
                           BlendFunc color_src_func,
                           BlendFunc color_dst_func,
                           BlendEq alpha_eq,
                           BlendFunc alpha_src_func,
                           BlendFunc alpha_dst_func) = 0;

  // Sets the blending color.
  virtual void SetBlendingColor(const RGBA &color) = 0;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_GAPI_INTERFACE_H__
