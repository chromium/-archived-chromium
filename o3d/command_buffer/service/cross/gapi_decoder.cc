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


// This class contains the implementation of the GAPI decoder class, decoding
// GAPI commands into calls to a GAPIInterface class.

#include "base/cross/bits.h"
#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/service/cross/gapi_decoder.h"
#include "command_buffer/service/cross/cmd_buffer_engine.h"

namespace o3d {
namespace command_buffer {

// Decode command with its arguments, and call the corresponding GAPIInterface
// method.
// Note: args is a pointer to the command buffer. As such, it could be changed
// by a (malicious) client at any time, so if validation has to happen, it
// should operate on a copy of them.
BufferSyncInterface::ParseError GAPIDecoder::DoCommand(
    unsigned int command,
    unsigned int arg_count,
    CommandBufferEntry *args) {
  switch (command) {
    case NOOP:
      return BufferSyncInterface::PARSE_NO_ERROR;
    case SET_TOKEN:
      if (arg_count == 1) {
        engine_->set_token(args[0].value_uint32);
        return BufferSyncInterface::PARSE_NO_ERROR;
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case BEGIN_FRAME:
      if (arg_count == 0) {
        gapi_->BeginFrame();
        return BufferSyncInterface::PARSE_NO_ERROR;
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case END_FRAME:
      if (arg_count == 0) {
        gapi_->EndFrame();
        return BufferSyncInterface::PARSE_NO_ERROR;
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case CLEAR:
      if (arg_count == 7) {
        unsigned int buffers = args[0].value_uint32;
        if (buffers & ~GAPIInterface::ALL_BUFFERS)
          return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        RGBA rgba;
        rgba.red = args[1].value_float;
        rgba.green = args[2].value_float;
        rgba.blue = args[3].value_float;
        rgba.alpha = args[4].value_float;
        float depth = args[5].value_float;
        unsigned int stencil = args[6].value_uint32;
        gapi_->Clear(buffers, rgba, depth, stencil);
        return BufferSyncInterface::PARSE_NO_ERROR;
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_VIEWPORT:
      if (arg_count == 6) {
        gapi_->SetViewport(args[0].value_uint32,
                           args[1].value_uint32,
                           args[2].value_uint32,
                           args[3].value_uint32,
                           args[4].value_float,
                           args[5].value_float);
        return BufferSyncInterface::PARSE_NO_ERROR;
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case CREATE_VERTEX_BUFFER:
      if (arg_count == 3) {
        return gapi_->CreateVertexBuffer(args[0].value_uint32,
                                         args[1].value_uint32,
                                         args[2].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case DESTROY_VERTEX_BUFFER:
      if (arg_count == 1) {
        return gapi_->DestroyVertexBuffer(args[0].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_VERTEX_BUFFER_DATA_IMMEDIATE: {
      if (arg_count < 2) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      ResourceID id = args[0].value_uint32;
      unsigned int offset = args[1].value_uint32;
      unsigned int size = (arg_count - 2) * sizeof(args[0]);
      return gapi_->SetVertexBufferData(id, offset, size, args + 2);
    }
    case SET_VERTEX_BUFFER_DATA:
      if (arg_count == 5) {
        ResourceID id = args[0].value_uint32;
        unsigned int offset = args[1].value_uint32;
        unsigned int size = args[2].value_uint32;
        void *data = GetAddressAndCheckSize(args[3].value_uint32,
                                            args[4].value_uint32,
                                            size);
        if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->SetVertexBufferData(id, offset, size, data);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case GET_VERTEX_BUFFER_DATA:
      if (arg_count == 5) {
        ResourceID id = args[0].value_uint32;
        unsigned int offset = args[1].value_uint32;
        unsigned int size = args[2].value_uint32;
        void *data = GetAddressAndCheckSize(args[3].value_uint32,
                                            args[4].value_uint32,
                                            size);
        if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->GetVertexBufferData(id, offset, size, data);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case CREATE_INDEX_BUFFER:
      if (arg_count == 3) {
        return gapi_->CreateIndexBuffer(args[0].value_uint32,
                                        args[1].value_uint32,
                                        args[2].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case DESTROY_INDEX_BUFFER:
      if (arg_count == 1) {
        return gapi_->DestroyIndexBuffer(args[0].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_INDEX_BUFFER_DATA_IMMEDIATE: {
      if (arg_count < 2) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      ResourceID id = args[0].value_uint32;
      unsigned int offset = args[1].value_uint32;
      unsigned int size = (arg_count - 2) * sizeof(args[0]);
      return gapi_->SetIndexBufferData(id, offset, size, args + 2);
    }
    case SET_INDEX_BUFFER_DATA:
      if (arg_count == 5) {
        ResourceID id = args[0].value_uint32;
        unsigned int offset = args[1].value_uint32;
        unsigned int size = args[2].value_uint32;
        void *data = GetAddressAndCheckSize(args[3].value_uint32,
                                            args[4].value_uint32,
                                            size);
        if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->SetIndexBufferData(id, offset, size, data);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case GET_INDEX_BUFFER_DATA:
      if (arg_count == 5) {
        ResourceID id = args[0].value_uint32;
        unsigned int offset = args[1].value_uint32;
        unsigned int size = args[2].value_uint32;
        void *data = GetAddressAndCheckSize(args[3].value_uint32,
                                            args[4].value_uint32,
                                            size);
        if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->GetIndexBufferData(id, offset, size, data);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case CREATE_VERTEX_STRUCT:
      if (arg_count == 2) {
        return gapi_->CreateVertexStruct(args[0].value_uint32,
                                         args[1].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case DESTROY_VERTEX_STRUCT:
      if (arg_count == 1) {
        return gapi_->DestroyVertexStruct(args[0].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_VERTEX_INPUT:
      return DecodeSetVertexInput(arg_count, args);
    case SET_VERTEX_STRUCT:
      if (arg_count == 1) {
        return gapi_->SetVertexStruct(args[0].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case DRAW:
      if (arg_count == 3) {
        unsigned int primitive_type = args[0].value_uint32;
        if (primitive_type >= GAPIInterface::MAX_PRIMITIVE_TYPE)
          return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        unsigned int first = args[1].value_uint32;
        unsigned int count = args[2].value_uint32;
        return gapi_->Draw(
            static_cast<GAPIInterface::PrimitiveType>(primitive_type),
            first, count);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case DRAW_INDEXED:
      if (arg_count == 6) {
        unsigned int primitive_type = args[0].value_uint32;
        if (primitive_type >= GAPIInterface::MAX_PRIMITIVE_TYPE)
          return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        ResourceID index_buffer = args[1].value_uint32;
        unsigned int first = args[2].value_uint32;
        unsigned int count = args[3].value_uint32;
        unsigned int min_index = args[4].value_uint32;
        unsigned int max_index = args[5].value_uint32;
        return gapi_->DrawIndexed(
            static_cast<GAPIInterface::PrimitiveType>(primitive_type),
            index_buffer, first, count, min_index, max_index);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case CREATE_EFFECT:
      if (arg_count == 4) {
        ResourceID id = args[0].value_uint32;
        unsigned int size = args[1].value_uint32;
        void *data = GetAddressAndCheckSize(args[2].value_uint32,
                                            args[3].value_uint32,
                                            size);
        if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->CreateEffect(id, size, data);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case CREATE_EFFECT_IMMEDIATE:
      if (arg_count > 2) {
        ResourceID id = args[0].value_uint32;
        unsigned int size = args[1].value_uint32;
        if (size > (arg_count-2) * sizeof(args[0]))
          return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->CreateEffect(id, size, args + 2);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case DESTROY_EFFECT:
      if (arg_count == 1) {
        return gapi_->DestroyEffect(args[0].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_EFFECT:
      if (arg_count == 1) {
        return gapi_->SetEffect(args[0].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case GET_PARAM_COUNT:
      if (arg_count == 4) {
        ResourceID id = args[0].value_uint32;
        unsigned int size = args[1].value_uint32;
        void *data = GetAddressAndCheckSize(args[2].value_uint32,
                                            args[3].value_uint32,
                                            size);
        if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->GetParamCount(id, size, data);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case CREATE_PARAM:
      if (arg_count == 3) {
        ResourceID param_id = args[0].value_uint32;
        ResourceID effect_id = args[1].value_uint32;
        unsigned int index = args[2].value_uint32;
        return gapi_->CreateParam(param_id, effect_id, index);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case CREATE_PARAM_BY_NAME:
      if (arg_count == 5) {
        ResourceID param_id = args[0].value_uint32;
        ResourceID effect_id = args[1].value_uint32;
        unsigned int size = args[2].value_uint32;
        void *data = GetAddressAndCheckSize(args[3].value_uint32,
                                            args[4].value_uint32,
                                            size);
        if (!data)
          return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->CreateParamByName(param_id, effect_id, size, data);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case CREATE_PARAM_BY_NAME_IMMEDIATE:
      if (arg_count > 3) {
        ResourceID param_id = args[0].value_uint32;
        ResourceID effect_id = args[1].value_uint32;
        unsigned int size = args[2].value_uint32;
        if (size > (arg_count-1) * sizeof(args[0]))
          return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->CreateParamByName(param_id, effect_id, size, args + 3);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case DESTROY_PARAM:
      if (arg_count == 1) {
        return gapi_->DestroyParam(args[0].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_PARAM_DATA:
      if (arg_count == 4) {
        ResourceID id = args[0].value_uint32;
        unsigned int size = args[1].value_uint32;
        void *data = GetAddressAndCheckSize(args[2].value_uint32,
                                            args[3].value_uint32,
                                            size);
        if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->SetParamData(id, size, data);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_PARAM_DATA_IMMEDIATE:
      if (arg_count > 2) {
        ResourceID id = args[0].value_uint32;
        unsigned int size = args[1].value_uint32;
        if (size > (arg_count-2) * sizeof(args[0]))
          return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->SetParamData(id, size, args + 2);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case GET_PARAM_DESC:
      if (arg_count == 4) {
        ResourceID id = args[0].value_uint32;
        unsigned int size = args[1].value_uint32;
        void *data = GetAddressAndCheckSize(args[2].value_uint32,
                                            args[3].value_uint32,
                                            size);
        if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->GetParamDesc(id, size, data);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case GET_STREAM_COUNT:
      if (arg_count == 4) {
        ResourceID id = args[0].value_uint32;
        unsigned int size = args[1].value_uint32;
        void *data = GetAddressAndCheckSize(args[2].value_uint32,
                                            args[3].value_uint32,
                                            size);
        if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->GetStreamCount(id, size, data);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case GET_STREAM_DESC:
      if (arg_count == 5) {
        ResourceID id = args[0].value_uint32;
        unsigned int index = args[1].value_uint32;
        unsigned int size = args[2].value_uint32;
        void *data = GetAddressAndCheckSize(args[3].value_uint32,
                                            args[4].value_uint32,
                                            size);
        if (!data) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        return gapi_->GetStreamDesc(id, index, size, data);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case DESTROY_TEXTURE:
      if (arg_count == 1) {
        return gapi_->DestroyTexture(args[0].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case CREATE_TEXTURE_2D:
      return DecodeCreateTexture2D(arg_count, args);
    case CREATE_TEXTURE_3D:
      return DecodeCreateTexture3D(arg_count, args);
    case CREATE_TEXTURE_CUBE:
      return DecodeCreateTextureCube(arg_count, args);
    case SET_TEXTURE_DATA:
      return DecodeSetTextureData(arg_count, args);
    case SET_TEXTURE_DATA_IMMEDIATE:
      return DecodeSetTextureDataImmediate(arg_count, args);
    case GET_TEXTURE_DATA:
      return DecodeGetTextureData(arg_count, args);
    case CREATE_SAMPLER:
      if (arg_count == 1) {
        return gapi_->CreateSampler(args[0].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case DESTROY_SAMPLER:
      if (arg_count == 1) {
        return gapi_->DestroySampler(args[0].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_SAMPLER_STATES:
      return DecodeSetSamplerStates(arg_count, args);
    case SET_SAMPLER_BORDER_COLOR:
      if (arg_count == 5) {
        RGBA rgba;
        rgba.red = args[1].value_float;
        rgba.green = args[2].value_float;
        rgba.blue = args[3].value_float;
        rgba.alpha = args[4].value_float;
        return gapi_->SetSamplerBorderColor(args[0].value_uint32, rgba);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_SAMPLER_TEXTURE:
      if (arg_count == 2) {
        return gapi_->SetSamplerTexture(args[0].value_uint32,
                                        args[1].value_uint32);
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_SCISSOR:
      if (arg_count == 2) {
        namespace cmd = set_scissor;
        Uint32 x_y_enable = args[0].value_uint32;
        if (cmd::Unused::Get(x_y_enable) != 0)
          return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        unsigned int x = cmd::X::Get(x_y_enable);
        unsigned int y = cmd::Y::Get(x_y_enable);
        bool enable = cmd::Enable::Get(x_y_enable) != 0;
        Uint32 width_height = args[1].value_uint32;
        unsigned int width = cmd::Width::Get(width_height);
        unsigned int height = cmd::Height::Get(width_height);
        gapi_->SetScissor(enable, x, y, width, height);
        return BufferSyncInterface::PARSE_NO_ERROR;
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_POLYGON_OFFSET:
      if (arg_count == 2) {
        gapi_->SetPolygonOffset(args[0].value_float, args[1].value_float);
        return BufferSyncInterface::PARSE_NO_ERROR;
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_POINT_LINE_RASTER:
      if (arg_count == 2) {
        namespace cmd = set_point_line_raster;
        Uint32 enables = args[0].value_uint32;
        if (cmd::Unused::Get(enables) != 0)
          return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        bool line_smooth = cmd::LineSmoothEnable::Get(enables);
        bool point_sprite = cmd::PointSpriteEnable::Get(enables);
        float point_size = args[1].value_float;
        gapi_->SetPointLineRaster(line_smooth, point_sprite, point_size);
        return BufferSyncInterface::PARSE_NO_ERROR;
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_POLYGON_RASTER:
      if (arg_count == 1) {
        namespace cmd = set_polygon_raster;
        Uint32 fill_cull = args[0].value_uint32;
        unsigned int fill_value = cmd::FillMode::Get(fill_cull);
        unsigned int cull_value = cmd::CullMode::Get(fill_cull);
        if (cmd::Unused::Get(fill_cull) != 0 ||
            fill_value >= GAPIInterface::NUM_POLYGON_MODE ||
            cull_value >= GAPIInterface::NUM_FACE_CULL_MODE)
          return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        gapi_->SetPolygonRaster(
            static_cast<GAPIInterface::PolygonMode>(fill_value),
            static_cast<GAPIInterface::FaceCullMode>(cull_value));
        return BufferSyncInterface::PARSE_NO_ERROR;
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_ALPHA_TEST:
      if (arg_count == 2) {
        namespace cmd = set_alpha_test;
        Uint32 func_enable = args[0].value_uint32;
        if (cmd::Unused::Get(func_enable) != 0)
          return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        // Check that the bitmask get cannot generate values outside of the
        // allowed range.
        COMPILE_ASSERT(cmd::Func::kMask < GAPIInterface::NUM_COMPARISON,
                       set_alpha_test_Func_may_produce_invalid_values);
        GAPIInterface::Comparison comp =
            static_cast<GAPIInterface::Comparison>(cmd::Func::Get(func_enable));
        bool enable = cmd::Enable::Get(func_enable) != 0;
        gapi_->SetAlphaTest(enable, args[1].value_float, comp);
        return BufferSyncInterface::PARSE_NO_ERROR;
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_DEPTH_TEST:
      if (arg_count == 1) {
        namespace cmd = set_depth_test;
        Uint32 func_enable = args[0].value_uint32;
        if (cmd::Unused::Get(func_enable) != 0)
          return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        // Check that the bitmask get cannot generate values outside of the
        // allowed range.
        COMPILE_ASSERT(cmd::Func::kMask < GAPIInterface::NUM_COMPARISON,
                       set_alpha_test_Func_may_produce_invalid_values);
        GAPIInterface::Comparison comp =
            static_cast<GAPIInterface::Comparison>(cmd::Func::Get(func_enable));
        bool write_enable = cmd::WriteEnable::Get(func_enable) != 0;
        bool enable = cmd::Enable::Get(func_enable) != 0;
        gapi_->SetDepthTest(enable, write_enable, comp);
        return BufferSyncInterface::PARSE_NO_ERROR;
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_STENCIL_TEST:
      return DecodeSetStencilTest(arg_count, args);
    case SET_COLOR_WRITE:
      if (arg_count == 1) {
        namespace cmd = set_color_write;
        Uint32 enables = args[0].value_uint32;
        if (cmd::Unused::Get(enables) != 0)
          return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
        bool red = cmd::RedMask::Get(enables) != 0;
        bool green = cmd::GreenMask::Get(enables) != 0;
        bool blue = cmd::BlueMask::Get(enables) != 0;
        bool alpha = cmd::AlphaMask::Get(enables) != 0;
        bool dither = cmd::DitherEnable::Get(enables) != 0;
        gapi_->SetColorWrite(red, green, blue, alpha, dither);
        return BufferSyncInterface::PARSE_NO_ERROR;
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    case SET_BLENDING:
      return DecodeSetBlending(arg_count, args);
    case SET_BLENDING_COLOR:
      if (arg_count == 4) {
        RGBA rgba;
        rgba.red = args[0].value_float;
        rgba.green = args[1].value_float;
        rgba.blue = args[2].value_float;
        rgba.alpha = args[3].value_float;
        gapi_->SetBlendingColor(rgba);
        return BufferSyncInterface::PARSE_NO_ERROR;
      } else {
        return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
      }
    default:
      return BufferSyncInterface::PARSE_UNKNOWN_COMMAND;
  }
}

// Decodes the SET_VERTEX_INPUT command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeSetVertexInput(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  namespace cmd = set_vertex_input_cmd;
  if (arg_count != 5) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  ResourceID vertex_struct_id = args[0].value_uint32;
  unsigned int input_index = args[1].value_uint32;
  ResourceID vertex_buffer_id = args[2].value_uint32;
  unsigned int offset = args[3].value_uint32;
  unsigned int type_stride_semantic = args[4].value_uint32;
  unsigned int semantic_index = cmd::SemanticIndex::Get(type_stride_semantic);
  unsigned int semantic = cmd::Semantic::Get(type_stride_semantic);
  unsigned int type = cmd::Type::Get(type_stride_semantic);
  unsigned int stride = cmd::Stride::Get(type_stride_semantic);
  if (semantic >= vertex_struct::NUM_SEMANTICS ||
      type >= vertex_struct::NUM_TYPES || stride == 0)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return gapi_->SetVertexInput(vertex_struct_id, input_index, vertex_buffer_id,
                               offset, stride,
                               static_cast<vertex_struct::Type>(type),
                               static_cast<vertex_struct::Semantic>(semantic),
                               semantic_index);
}

// Decodes the CREATE_TEXTURE_2D command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeCreateTexture2D(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  if (arg_count == 3) {
    namespace cmd = create_texture_2d_cmd;
    unsigned int id = args[0].value_uint32;
    unsigned int width_height = args[1].value_uint32;
    unsigned int levels_format_flags = args[2].value_uint32;
    unsigned int width = cmd::Width::Get(width_height);
    unsigned int height = cmd::Height::Get(width_height);
    unsigned int levels = cmd::Levels::Get(levels_format_flags);
    unsigned int unused = cmd::Unused::Get(levels_format_flags);
    unsigned int format = cmd::Format::Get(levels_format_flags);
    unsigned int flags = cmd::Flags::Get(levels_format_flags);
    unsigned int max_levels =
        1 + base::bits::Log2Ceiling(std::max(width, height));
    if ((width == 0) || (height == 0) || (levels > max_levels) ||
        (unused != 0) || (format >= texture::NUM_FORMATS))
      return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
    if (levels == 0) levels = max_levels;
    return gapi_->CreateTexture2D(id, width, height, levels,
                                  static_cast<texture::Format>(format), flags);
  } else {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
}

// Decodes the CREATE_TEXTURE_3D command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeCreateTexture3D(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  if (arg_count == 4) {
    namespace cmd = create_texture_3d_cmd;
    unsigned int id = args[0].value_uint32;
    unsigned int width_height = args[1].value_uint32;
    unsigned int depth_unused = args[2].value_uint32;
    unsigned int levels_format_flags = args[3].value_uint32;
    unsigned int width = cmd::Width::Get(width_height);
    unsigned int height = cmd::Height::Get(width_height);
    unsigned int depth = cmd::Depth::Get(depth_unused);
    unsigned int unused1 = cmd::Unused1::Get(depth_unused);
    unsigned int levels = cmd::Levels::Get(levels_format_flags);
    unsigned int unused2 = cmd::Unused2::Get(levels_format_flags);
    unsigned int format = cmd::Format::Get(levels_format_flags);
    unsigned int flags = cmd::Flags::Get(levels_format_flags);
    unsigned int max_levels =
        1 + base::bits::Log2Ceiling(std::max(depth, std::max(width, height)));
    if ((width == 0) || (height == 0) || (depth == 0) ||
        (levels > max_levels) || (unused1 != 0) || (unused2 != 0) ||
        (format >= texture::NUM_FORMATS))
      return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
    if (levels == 0) levels = max_levels;
    return gapi_->CreateTexture3D(id, width, height, depth, levels,
                                  static_cast<texture::Format>(format), flags);
  } else {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
}

// Decodes the CREATE_TEXTURE_CUBE command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeCreateTextureCube(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  if (arg_count == 3) {
    namespace cmd = create_texture_cube_cmd;
    unsigned int id = args[0].value_uint32;
    unsigned int side_unused = args[1].value_uint32;
    unsigned int levels_format_flags = args[2].value_uint32;
    unsigned int side = cmd::Side::Get(side_unused);
    unsigned int unused1 = cmd::Unused1::Get(side_unused);
    unsigned int levels = cmd::Levels::Get(levels_format_flags);
    unsigned int unused2 = cmd::Unused2::Get(levels_format_flags);
    unsigned int format = cmd::Format::Get(levels_format_flags);
    unsigned int flags = cmd::Flags::Get(levels_format_flags);
    unsigned int max_levels = 1 + base::bits::Log2Ceiling(side);
    if ((side == 0) || (levels > max_levels) || (unused1 != 0) ||
        (unused2 != 0) || (format >= texture::NUM_FORMATS))
      return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
    if (levels == 0) levels = max_levels;
    return gapi_->CreateTextureCube(id, side, levels,
                                    static_cast<texture::Format>(format),
                                    flags);
  } else {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
}

// Decodes the SET_TEXTURE_DATA command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeSetTextureData(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  if (arg_count == 10) {
    namespace cmd = set_texture_data_cmd;
    unsigned int id = args[0].value_uint32;
    unsigned int x_y = args[1].value_uint32;
    unsigned int width_height = args[2].value_uint32;
    unsigned int z_depth = args[3].value_uint32;
    unsigned int level_face = args[4].value_uint32;
    unsigned int row_pitch = args[5].value_uint32;
    unsigned int slice_pitch = args[6].value_uint32;
    unsigned int size = args[7].value_uint32;
    unsigned int shm_id = args[8].value_uint32;
    unsigned int offset = args[9].value_uint32;
    unsigned int x = cmd::X::Get(x_y);
    unsigned int y = cmd::Y::Get(x_y);
    unsigned int width = cmd::Width::Get(width_height);
    unsigned int height = cmd::Height::Get(width_height);
    unsigned int z = cmd::Z::Get(z_depth);
    unsigned int depth = cmd::Depth::Get(z_depth);
    unsigned int level = cmd::Level::Get(level_face);
    unsigned int face = cmd::Face::Get(level_face);
    unsigned int unused = cmd::Unused::Get(level_face);
    const void *data = GetAddressAndCheckSize(shm_id, offset, size);
    if (face >= 6 || unused != 0 || !data)
      return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
    return gapi_->SetTextureData(id, x, y, z, width, height, depth, level,
                                 static_cast<texture::Face>(face), row_pitch,
                                 slice_pitch, size, data);

  } else {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
}

// Decodes the SET_TEXTURE_DATA_IMMEDIATE command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeSetTextureDataImmediate(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  if (arg_count > 8) {
    namespace cmd = set_texture_data_immediate_cmd;
    unsigned int id = args[0].value_uint32;
    unsigned int x_y = args[1].value_uint32;
    unsigned int width_height = args[2].value_uint32;
    unsigned int z_depth = args[3].value_uint32;
    unsigned int level_face = args[4].value_uint32;
    unsigned int row_pitch = args[5].value_uint32;
    unsigned int slice_pitch = args[6].value_uint32;
    unsigned int size = args[7].value_uint32;
    unsigned int x = cmd::X::Get(x_y);
    unsigned int y = cmd::Y::Get(x_y);
    unsigned int width = cmd::Width::Get(width_height);
    unsigned int height = cmd::Height::Get(width_height);
    unsigned int z = cmd::Z::Get(z_depth);
    unsigned int depth = cmd::Depth::Get(z_depth);
    unsigned int level = cmd::Level::Get(level_face);
    unsigned int face = cmd::Face::Get(level_face);
    unsigned int unused = cmd::Unused::Get(level_face);
    if (face >= 6 || unused != 0 ||
        size > (arg_count - 5) * sizeof(args[0]))
      return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
    return gapi_->SetTextureData(id, x, y, z, width, height, depth, level,
                                 static_cast<texture::Face>(face), row_pitch,
                                 slice_pitch, size, args + 8);
  } else {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
}

// Decodes the GET_TEXTURE_DATA command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeGetTextureData(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  if (arg_count == 10) {
    namespace cmd = get_texture_data_cmd;
    unsigned int id = args[0].value_uint32;
    unsigned int x_y = args[1].value_uint32;
    unsigned int width_height = args[2].value_uint32;
    unsigned int z_depth = args[3].value_uint32;
    unsigned int level_face = args[4].value_uint32;
    unsigned int row_pitch = args[5].value_uint32;
    unsigned int slice_pitch = args[6].value_uint32;
    unsigned int size = args[7].value_uint32;
    unsigned int shm_id = args[8].value_uint32;
    unsigned int offset = args[9].value_uint32;
    unsigned int x = cmd::X::Get(x_y);
    unsigned int y = cmd::Y::Get(x_y);
    unsigned int width = cmd::Width::Get(width_height);
    unsigned int height = cmd::Height::Get(width_height);
    unsigned int z = cmd::Z::Get(z_depth);
    unsigned int depth = cmd::Depth::Get(z_depth);
    unsigned int level = cmd::Level::Get(level_face);
    unsigned int face = cmd::Face::Get(level_face);
    unsigned int unused = cmd::Unused::Get(level_face);
    void *data = GetAddressAndCheckSize(shm_id, offset, size);
    if (face >= 6 || unused != 0 || !data)
      return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
    return gapi_->GetTextureData(id, x, y, z, width, height, depth, level,
                                 static_cast<texture::Face>(face), row_pitch,
                                 slice_pitch, size, data);

  } else {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
}

// Decodes the SET_SAMPLER_STATES command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeSetSamplerStates(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  namespace cmd = set_sampler_states;
  if (arg_count != 2)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  ResourceID id = args[0].value_uint32;
  Uint32 arg = args[1].value_uint32;
  if (cmd::Unused::Get(arg) != 0)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  unsigned int address_u_value = cmd::AddressingU::Get(arg);
  unsigned int address_v_value = cmd::AddressingV::Get(arg);
  unsigned int address_w_value = cmd::AddressingW::Get(arg);
  unsigned int mag_filter_value = cmd::MagFilter::Get(arg);
  unsigned int min_filter_value = cmd::MinFilter::Get(arg);
  unsigned int mip_filter_value = cmd::MipFilter::Get(arg);
  unsigned int max_anisotropy = cmd::MaxAnisotropy::Get(arg);
  if (address_u_value >= sampler::NUM_ADDRESSING_MODE ||
      address_v_value >= sampler::NUM_ADDRESSING_MODE ||
      address_w_value >= sampler::NUM_ADDRESSING_MODE ||
      mag_filter_value >= sampler::NUM_FILTERING_MODE ||
      min_filter_value >= sampler::NUM_FILTERING_MODE ||
      mip_filter_value >= sampler::NUM_FILTERING_MODE ||
      mag_filter_value == sampler::NONE ||
      min_filter_value == sampler::NONE ||
      max_anisotropy == 0) {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
  gapi_->SetSamplerStates(
      id,
      static_cast<sampler::AddressingMode>(address_u_value),
      static_cast<sampler::AddressingMode>(address_v_value),
      static_cast<sampler::AddressingMode>(address_w_value),
      static_cast<sampler::FilteringMode>(mag_filter_value),
      static_cast<sampler::FilteringMode>(min_filter_value),
      static_cast<sampler::FilteringMode>(mip_filter_value),
      max_anisotropy);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

// Decodes the SET_STENCIL_TEST command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeSetStencilTest(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  namespace cmd = set_stencil_test;
  if (arg_count != 2)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  Uint32 arg0 = args[0].value_uint32;
  Uint32 arg1 = args[1].value_uint32;
  if (cmd::Unused0::Get(arg0) != 0 ||
      cmd::Unused1::Get(arg1) != 0 ||
      cmd::Unused2::Get(arg1) != 0)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  unsigned int write_mask = cmd::WriteMask::Get(arg0);
  unsigned int compare_mask = cmd::CompareMask::Get(arg0);
  unsigned int ref = cmd::ReferenceValue::Get(arg0);
  bool enable = cmd::Enable::Get(arg0) != 0;
  bool separate_ccw = cmd::SeparateCCW::Get(arg0) != 0;
  gapi_->SetStencilTest(enable, separate_ccw, write_mask, compare_mask, ref,
                        arg1);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

// Decodes the SET_BLENDING command.
BufferSyncInterface::ParseError GAPIDecoder::DecodeSetBlending(
    unsigned int arg_count,
    CommandBufferEntry *args) {
  namespace cmd = set_blending;
  if (arg_count != 1)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  Uint32 arg = args[0].value_uint32;
  bool enable = cmd::Enable::Get(arg) != 0;
  bool separate_alpha = cmd::SeparateAlpha::Get(arg) != 0;
  unsigned int color_eq = cmd::ColorEq::Get(arg);
  unsigned int color_src = cmd::ColorSrcFunc::Get(arg);
  unsigned int color_dst = cmd::ColorDstFunc::Get(arg);
  unsigned int alpha_eq = cmd::AlphaEq::Get(arg);
  unsigned int alpha_src = cmd::AlphaSrcFunc::Get(arg);
  unsigned int alpha_dst = cmd::AlphaDstFunc::Get(arg);
  if (cmd::Unused0::Get(arg) != 0 ||
      cmd::Unused1::Get(arg) != 0 ||
      color_eq >= GAPIInterface::NUM_BLEND_EQ ||
      color_src >= GAPIInterface::NUM_BLEND_FUNC ||
      color_dst >= GAPIInterface::NUM_BLEND_FUNC ||
      alpha_eq >= GAPIInterface::NUM_BLEND_EQ ||
      alpha_src >= GAPIInterface::NUM_BLEND_FUNC ||
      alpha_dst >= GAPIInterface::NUM_BLEND_FUNC)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  gapi_->SetBlending(enable,
                     separate_alpha,
                     static_cast<GAPIInterface::BlendEq>(color_eq),
                     static_cast<GAPIInterface::BlendFunc>(color_src),
                     static_cast<GAPIInterface::BlendFunc>(color_dst),
                     static_cast<GAPIInterface::BlendEq>(alpha_eq),
                     static_cast<GAPIInterface::BlendFunc>(alpha_src),
                     static_cast<GAPIInterface::BlendFunc>(alpha_dst));
  return BufferSyncInterface::PARSE_NO_ERROR;
}

void *GAPIDecoder::GetAddressAndCheckSize(unsigned int shm_id,
                                          unsigned int offset,
                                          unsigned int size) {
  void * shm_addr = engine_->GetSharedMemoryAddress(shm_id);
  if (!shm_addr) return NULL;
  size_t shm_size = engine_->GetSharedMemorySize(shm_id);
  if (offset + size > shm_size) return NULL;
  return static_cast<char *>(shm_addr) + offset;
}

}  // namespace command_buffer
}  // namespace o3d
