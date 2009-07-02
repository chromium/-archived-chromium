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


// This file contains the binary format definition of the command buffer.

#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_CMD_BUFFER_FORMAT_H_
#define O3D_COMMAND_BUFFER_COMMON_CROSS_CMD_BUFFER_FORMAT_H_

#include "base/basictypes.h"
#include "command_buffer/common/cross/types.h"
#include "command_buffer/common/cross/bitfield_helpers.h"

namespace o3d {
namespace command_buffer {

// Struct that defines the command header in the command buffer.
struct CommandHeader {
  Uint32 size:8;
  Uint32 command:24;
};
COMPILE_ASSERT(sizeof(CommandHeader) == 4, Sizeof_CommandHeader_is_not_4);

// Union that defines possible command buffer entries.
union CommandBufferEntry {
  CommandHeader value_header;
  Uint32 value_uint32;
  Int32 value_int32;
  float value_float;
};

COMPILE_ASSERT(sizeof(CommandBufferEntry) == 4,
               Sizeof_CommandBufferEntry_is_not_4);

// Bitfields for the SET_VERTEX_INPUT command.
namespace set_vertex_input_cmd {
// argument 4
typedef BitField<0, 4> SemanticIndex;
typedef BitField<4, 4> Semantic;
typedef BitField<8, 8> Type;
typedef BitField<16, 16> Stride;
}  // namespace set_vertex_input_cmd

// Bitfields for the CREATE_TEXTURE_2D command.
namespace create_texture_2d_cmd {
// argument 1
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
// argument 2
typedef BitField<0, 4> Levels;
typedef BitField<4, 4> Unused;
typedef BitField<8, 8> Format;
typedef BitField<16, 16> Flags;
}  // namespace create_texture_2d_cmd

// Bitfields for the CREATE_TEXTURE_3D command.
namespace create_texture_3d_cmd {
// argument 1
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
// argument 2
typedef BitField<0, 16> Depth;
typedef BitField<16, 16> Unused1;
// argument 3
typedef BitField<0, 4> Levels;
typedef BitField<4, 4> Unused2;
typedef BitField<8, 8> Format;
typedef BitField<16, 16> Flags;
}  // namespace create_texture_3d_cmd

// Bitfields for the CREATE_TEXTURE_CUBE command.
namespace create_texture_cube_cmd {
// argument 1
typedef BitField<0, 16> Side;
typedef BitField<16, 16> Unused1;
// argument 2
typedef BitField<0, 4> Levels;
typedef BitField<4, 4> Unused2;
typedef BitField<8, 8> Format;
typedef BitField<16, 16> Flags;
}  // namespace create_texture_cube_cmd

// Bitfields for the SET_TEXTURE_DATA command.
namespace set_texture_data_cmd {
// argument 1
typedef BitField<0, 16> X;
typedef BitField<16, 16> Y;
// argument 2
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
// argument 3
typedef BitField<0, 16> Z;
typedef BitField<16, 16> Depth;
// argument 4
typedef BitField<0, 4> Level;
typedef BitField<4, 3> Face;
typedef BitField<7, 25> Unused;
}  // namespace set_texture_data_cmd

// Bitfields for the SET_TEXTURE_DATA_IMMEDIATE command.
namespace set_texture_data_immediate_cmd {
// argument 1
typedef BitField<0, 16> X;
typedef BitField<16, 16> Y;
// argument 2
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
// argument 3
typedef BitField<0, 16> Z;
typedef BitField<16, 16> Depth;
// argument 4
typedef BitField<0, 4> Level;
typedef BitField<4, 3> Face;
typedef BitField<7, 25> Unused;
}  // namespace set_texture_data_immediate_cmd

// Bitfields for the GET_TEXTURE_DATA command.
namespace get_texture_data_cmd {
// argument 1
typedef BitField<0, 16> X;
typedef BitField<16, 16> Y;
// argument 2
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
// argument 3
typedef BitField<0, 16> Z;
typedef BitField<16, 16> Depth;
// argument 4
typedef BitField<0, 4> Level;
typedef BitField<4, 3> Face;
typedef BitField<7, 25> Unused;
}  // namespace get_texture_data_cmd

// Bitfields for the SET_SAMPLER_STATES command.
namespace set_sampler_states {
// argument 2
typedef BitField<0, 3> AddressingU;
typedef BitField<3, 3> AddressingV;
typedef BitField<6, 3> AddressingW;
typedef BitField<9, 3> MagFilter;
typedef BitField<12, 3> MinFilter;
typedef BitField<15, 3> MipFilter;
typedef BitField<18, 6> Unused;
typedef BitField<24, 8> MaxAnisotropy;
}  // namespace get_texture_data_cmd

namespace set_scissor {
// argument 0
typedef BitField<0, 15> X;
typedef BitField<15, 1> Unused;
typedef BitField<16, 15> Y;
typedef BitField<31, 1> Enable;
// argument 1
typedef BitField<0, 16> Width;
typedef BitField<16, 16> Height;
}  // namespace set_scissor

namespace set_point_line_raster {
// argument 0
typedef BitField<0, 1> LineSmoothEnable;
typedef BitField<1, 1> PointSpriteEnable;
typedef BitField<2, 30> Unused;
}  // namespace set_point_line_raster

namespace set_polygon_raster {
// argument 0
typedef BitField<0, 2> FillMode;
typedef BitField<2, 2> CullMode;
typedef BitField<4, 28> Unused;
}  // namespace set_polygon_raster

namespace set_alpha_test {
// argument 0
typedef BitField<0, 3> Func;
typedef BitField<3, 28> Unused;
typedef BitField<31, 1> Enable;
}  // namespace set_alpha_test

namespace set_depth_test {
// argument 0
typedef BitField<0, 3> Func;
typedef BitField<3, 27> Unused;
typedef BitField<30, 1> WriteEnable;
typedef BitField<31, 1> Enable;
}  // namespace set_depth_test

namespace set_stencil_test {
// argument 0
typedef BitField<0, 8> WriteMask;
typedef BitField<8, 8> CompareMask;
typedef BitField<16, 8> ReferenceValue;
typedef BitField<24, 6> Unused0;
typedef BitField<30, 1> SeparateCCW;
typedef BitField<31, 1> Enable;
// argument 1
typedef BitField<0, 3> CWFunc;
typedef BitField<3, 3> CWPassOp;
typedef BitField<6, 3> CWFailOp;
typedef BitField<9, 3> CWZFailOp;
typedef BitField<12, 4> Unused1;
typedef BitField<16, 3> CCWFunc;
typedef BitField<19, 3> CCWPassOp;
typedef BitField<22, 3> CCWFailOp;
typedef BitField<25, 3> CCWZFailOp;
typedef BitField<28, 4> Unused2;
}  // namespace set_stencil_test

namespace set_color_write {
// argument 0
typedef BitField<0, 1> RedMask;
typedef BitField<1, 1> GreenMask;
typedef BitField<2, 1> BlueMask;
typedef BitField<3, 1> AlphaMask;
typedef BitField<0, 4> AllColorsMask;  // alias for RGBA
typedef BitField<4, 27> Unused;
typedef BitField<31, 1> DitherEnable;
}  // namespace set_color_write

namespace set_blending {
// argument 0
typedef BitField<0, 4> ColorSrcFunc;
typedef BitField<4, 4> ColorDstFunc;
typedef BitField<8, 3> ColorEq;
typedef BitField<11, 5> Unused0;
typedef BitField<16, 4> AlphaSrcFunc;
typedef BitField<20, 4> AlphaDstFunc;
typedef BitField<24, 3> AlphaEq;
typedef BitField<27, 3> Unused1;
typedef BitField<30, 1> SeparateAlpha;
typedef BitField<31, 1> Enable;
}  // namespace set_blending

// GAPI commands.
enum CommandId {
  NOOP,                              // No operation. Arbitrary argument size.
  SET_TOKEN,                         // Sets token. 1 argument.
  BEGIN_FRAME,                       // BeginFrame. 0 argument.
  END_FRAME,                         // EndFrame. 0 argument.
  CLEAR,                             // Clear. 7 arguments.
  CREATE_VERTEX_BUFFER,              // CreateVertexBuffer, 3 arguments.
  DESTROY_VERTEX_BUFFER,             // DestroyVertexBuffer. 1 argument.
  SET_VERTEX_BUFFER_DATA,            // SetVertexBufferData, 5 args
  SET_VERTEX_BUFFER_DATA_IMMEDIATE,  // SetVertexBufferData, 2 args + data
  GET_VERTEX_BUFFER_DATA,            // GetVertexBufferData, 5 args
  CREATE_INDEX_BUFFER,               // CreateIndexBuffer, 3 arguments.
  DESTROY_INDEX_BUFFER,              // DestroyIndexBuffer. 1 argument.
  SET_INDEX_BUFFER_DATA,             // SetIndexBufferData, 5 args
  SET_INDEX_BUFFER_DATA_IMMEDIATE,   // SetIndexBufferData, 2 args + data
  GET_INDEX_BUFFER_DATA,             // GetIndexBufferData, 5 args
  CREATE_VERTEX_STRUCT,              // CreateVertexStruct, 2 args.
  DESTROY_VERTEX_STRUCT,             // DestroyVertexStruct. 1 argument.
  SET_VERTEX_INPUT,                  // SetVertexInput, 5 args.
  SET_VERTEX_STRUCT,                 // SetVertexStruct, 1 arg.
  DRAW,                              // Draw, 3 args.
  DRAW_INDEXED,                      // DrawIndexed, 6 args.
  CREATE_EFFECT,                     // CreateEffect, 4 args.
  CREATE_EFFECT_IMMEDIATE,           // CreateEffect, 2 args + data
  DESTROY_EFFECT,                    // DestroyEffect, 1 arg.
  SET_EFFECT,                        // SetEffect, 1 arg.
  GET_PARAM_COUNT,                   // GetParamCount, 4 args.
  CREATE_PARAM,                      // CreateParam, 3 args.
  CREATE_PARAM_BY_NAME,              // CreateParamByName, 5 args.
  CREATE_PARAM_BY_NAME_IMMEDIATE,    // CreateParamByName, 3 args + data
  DESTROY_PARAM,                     // DestroyParam, 1 arg
  SET_PARAM_DATA,                    // SetParamData, 4 args
  SET_PARAM_DATA_IMMEDIATE,          // SetParamData, 2 args + data
  GET_PARAM_DESC,                    // GetParamDesc, 4 args
  GET_STREAM_COUNT,                  // GetStreamCount, 4 args.
  GET_STREAM_DESC,                   // GetStreamDesc, 5 args
  DESTROY_TEXTURE,                   // DestroyTexture, 1 arg
  CREATE_TEXTURE_2D,                 // CreateTexture2D, 3 args
  CREATE_TEXTURE_3D,                 // CreateTexture3D, 4 args
  CREATE_TEXTURE_CUBE,               // CreateTextureCube, 3 args
  SET_TEXTURE_DATA,                  // SetTextureData, 10 args
  SET_TEXTURE_DATA_IMMEDIATE,        // SetTextureData, 8 args + data
  GET_TEXTURE_DATA,                  // GetTextureData, 10 args
  CREATE_SAMPLER,                    // CreateSampler, 1 arg
  DESTROY_SAMPLER,                   // DestroySampler, 1 arg
  SET_SAMPLER_STATES,                // SetSamplerStates, 2 arg
  SET_SAMPLER_BORDER_COLOR,          // SetSamplerBorderColor, 5 arg
  SET_SAMPLER_TEXTURE,               // SetSamplerTexture, 2 arg
  SET_VIEWPORT,                      // SetViewport. 6 arguments.
  SET_SCISSOR,                       // SetScissor, 2 args
  SET_POINT_LINE_RASTER,             // SetPointLineRaster, 2 args
  SET_POLYGON_RASTER,                // SetPolygonRaster, 1 args
  SET_POLYGON_OFFSET,                // SetPolygonOffest, 2 args
  SET_ALPHA_TEST,                    // SetAlphaTest, 2 args
  SET_DEPTH_TEST,                    // SetDepthTest, 1 args
  SET_STENCIL_TEST,                  // SetStencilTest, 2 args
  SET_BLENDING,                      // SetBlending, 1 args
  SET_BLENDING_COLOR,                // SetBlendingColor, 4 args
  SET_COLOR_WRITE,                   // SetColorWrite, 1 args
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_CMD_BUFFER_FORMAT_H_
