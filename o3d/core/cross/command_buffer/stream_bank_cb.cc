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


// This file contains the implementation of the StreamBankCB class.

#include "core/cross/precompile.h"
#include "core/cross/command_buffer/param_cache_cb.h"
#include "core/cross/command_buffer/primitive_cb.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "core/cross/command_buffer/buffer_cb.h"
#include "core/cross/command_buffer/effect_cb.h"
#include "core/cross/command_buffer/stream_bank_cb.h"
#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"

// TODO: add unit tests.

namespace o3d {

using command_buffer::ResourceID;
using command_buffer::CommandBufferHelper;
using command_buffer::CommandBufferEntry;
using command_buffer::GAPIInterface;
using command_buffer::kInvalidResource;
namespace vertex_struct = command_buffer::vertex_struct;

StreamBankCB::StreamBankCB(ServiceLocator* service_locator,
                           RendererCB *renderer)
    : StreamBank(service_locator),
      renderer_(renderer),
      vertex_struct_id_(kInvalidResource) {
}

StreamBankCB::~StreamBankCB() {
  DestroyVertexStruct();
}

// Converts a semantic/index pair from the O3D conventions to the command
// buffer conventions.
static bool GetCBSemantic(
    Stream::Semantic semantic,
    unsigned int semantic_index,
    vertex_struct::Semantic *out_semantic,
    unsigned int *out_semantic_index) {
  // TODO: what meaning do we really want to put to our semantics ? How
  // do they match the semantics that are set in the effect ? What combination
  // of (semantic, index) are supposed to work ?
  switch (semantic) {
    case Stream::POSITION:
      if (semantic_index != 0) return false;
      *out_semantic = vertex_struct::POSITION;
      *out_semantic_index = 0;
      return true;
    case Stream::NORMAL:
      if (semantic_index != 0) return false;
      *out_semantic = vertex_struct::NORMAL;
      *out_semantic_index = 0;
      return true;
    case Stream::TANGENT:
      if (semantic_index != 0) return false;
      *out_semantic = vertex_struct::TEX_COORD;
      *out_semantic_index = 6;
      return true;
    case Stream::BINORMAL:
      if (semantic_index != 0) return false;
      *out_semantic = vertex_struct::TEX_COORD;
      *out_semantic_index = 7;
      return true;
    case Stream::COLOR:
      if (semantic_index > 1) return false;
      *out_semantic = vertex_struct::COLOR;
      *out_semantic_index = semantic_index;
      return true;
    case Stream::TEXCOORD:
      *out_semantic = vertex_struct::TEX_COORD;
      *out_semantic_index = semantic_index;
      return true;
    default:
      return false;
  }
}

// Converts a data type from O3D enum values to command-buffer enum values.
static vertex_struct::Type GetCBType(const Field& field) {
  if (field.IsA(FloatField::GetApparentClass())) {
    switch (field.num_components()) {
      case 1:
        return vertex_struct::FLOAT1;
      case 2:
        return vertex_struct::FLOAT2;
      case 3:
        return vertex_struct::FLOAT3;
      case 4:
        return vertex_struct::FLOAT4;
    }
  } else if (field.IsA(UByteNField::GetApparentClass())) {
    switch (field.num_components()) {
      case 4:
        return vertex_struct::UCHAR4N;
    }
  }
  DLOG(ERROR) << "Unknown Stream DataType";
  return vertex_struct::NUM_TYPES;
}

// This function is overridden so that we can invalidate the vertex struct any
// time the streams change.
void StreamBankCB::OnUpdateStreams() {
  DestroyVertexStruct();
}

// Creates the vertex struct resource on the service side. It will only set the
// vertex inputs if they represent semantics and types we know about. The
// command buffer API will not draw with an incomplete vertex struct.
// This function will get called on Draw, after any change to the vertex inputs
// has occurred.
void StreamBankCB::CreateVertexStruct() {
  DCHECK_EQ(kInvalidResource, vertex_struct_id_);
  vertex_struct_id_ = renderer_->vertex_structs_ids().AllocateID();
  CommandBufferHelper *helper = renderer_->helper();
  CommandBufferEntry args[5];
  args[0].value_uint32 = vertex_struct_id_;
  args[1].value_uint32 = vertex_stream_params_.size();
  helper->AddCommand(command_buffer::CREATE_VERTEX_STRUCT, 2, args);
  for (unsigned int i = 0; i < vertex_stream_params_.size(); ++i) {
    const Stream &stream = vertex_stream_params_[i]->stream();
    vertex_struct::Semantic cb_semantic;
    unsigned int cb_semantic_index;
    if (!GetCBSemantic(stream.semantic(), stream.semantic_index(), &cb_semantic,
                       &cb_semantic_index)) {
      DLOG(INFO) << "Unknown semantic (" << stream.semantic() << ", "
                 << stream.semantic_index() << ") - ignoring stream.";
      continue;
    }
    vertex_struct::Type cb_type = GetCBType(stream.field());
    if (cb_type == vertex_struct::NUM_TYPES) {
      DLOG(INFO) << "Invalid type (" << stream.field().num_components()
                 << ") - ignoring stream.";
      continue;
    }

    namespace cmd = command_buffer::set_vertex_input_cmd;
    VertexBufferCB *vertex_buffer =
        static_cast<VertexBufferCB *>(stream.field().buffer());
    args[0].value_uint32 = vertex_struct_id_;
    args[1].value_uint32 = i;
    args[2].value_uint32 = vertex_buffer->resource_id();
    args[3].value_uint32 = stream.field().offset();
    args[4].value_uint32 =
        cmd::SemanticIndex::MakeValue(cb_semantic_index) |
        cmd::Semantic::MakeValue(cb_semantic) |
        cmd::Type::MakeValue(cb_type) |
        cmd::Stride::MakeValue(vertex_buffer->stride());
    helper->AddCommand(command_buffer::SET_VERTEX_INPUT, 5, args);
  }
}

// Destroys the vertex struct resource on the service side.
void StreamBankCB::DestroyVertexStruct() {
  if (vertex_struct_id_ != kInvalidResource) {
    CommandBufferHelper *helper = renderer_->helper();
    CommandBufferEntry args[1];
    args[0].value_uint32 = vertex_struct_id_;
    helper->AddCommand(command_buffer::DESTROY_VERTEX_STRUCT, 1, args);
    renderer_->vertex_structs_ids().FreeID(vertex_struct_id_);
    vertex_struct_id_ = kInvalidResource;
  }
}

void StreamBankCB::BindStreamsForRendering() {
  if (vertex_struct_id_ == kInvalidResource)
    CreateVertexStruct();
  CommandBufferHelper *helper = renderer_->helper();
  CommandBufferEntry args[6];
  // Sets current vertex struct.
  args[0].value_uint32 = vertex_struct_id_;
  helper->AddCommand(command_buffer::SET_VERTEX_STRUCT, 1, args);
}

}  // namespace o3d
