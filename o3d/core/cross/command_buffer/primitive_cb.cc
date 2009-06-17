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


// This file contains the implementation of the PrimitiveCB class.

#include "core/cross/precompile.h"
#include "core/cross/command_buffer/param_cache_cb.h"
#include "core/cross/command_buffer/primitive_cb.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "core/cross/command_buffer/buffer_cb.h"
#include "core/cross/command_buffer/effect_cb.h"
#include "core/cross/command_buffer/stream_bank_cb.h"
#include "core/cross/error.h"
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

PrimitiveCB::PrimitiveCB(ServiceLocator* service_locator, RendererCB *renderer)
    : Primitive(service_locator),
      renderer_(renderer) {
}

PrimitiveCB::~PrimitiveCB() {
}

// Converts an O3D primitive type to a command-buffer one.
static GAPIInterface::PrimitiveType GetCBPrimitiveType(
    Primitive::PrimitiveType primitive_type) {
  switch (primitive_type) {
    case Primitive::LINELIST:
      return GAPIInterface::LINES;
    case Primitive::LINESTRIP:
      return GAPIInterface::LINE_STRIPS;
    case Primitive::TRIANGLELIST:
      return GAPIInterface::TRIANGLES;
    case Primitive::TRIANGLESTRIP:
      return GAPIInterface::TRIANGLE_STRIPS;
    case Primitive::TRIANGLEFAN:
      return GAPIInterface::TRIANGLE_FANS;
    default:
      // Note that POINTLIST falls into this case, for compatibility with D3D.
      return GAPIInterface::MAX_PRIMITIVE_TYPE;
  }
}

// Sends the draw commands to the command buffers.
void PrimitiveCB::Render(Renderer* renderer,
                         DrawElement* draw_element,
                         Material* material,
                         ParamObject* override,
                         ParamCache* param_cache) {
  DLOG_ASSERT(draw_element);
  DLOG_ASSERT(param_cache);
  if (!material) {
    O3D_ERROR(service_locator())
        << "No Material attached to Shape '"
        << draw_element->name() << "'";
    return;
  }
  EffectCB *effect_cb = down_cast<EffectCB*>(material->effect());
  // If there's no effect attached to this Material, draw nothing.
  if (!effect_cb ||
      effect_cb->resource_id() == command_buffer::kInvalidResource) {
    O3D_ERROR(service_locator())
        << "No Effect attached to Material '"
        << material->name() << "' in Shape '"
        << draw_element->name() << "'";
    return;
  }
  StreamBankCB* stream_bank_cb = down_cast<StreamBankCB*>(stream_bank());
  if (!stream_bank_cb) {
    O3D_ERROR(service_locator())
        << "No StreamBank attached to Shape '"
        << draw_element->name() << "'";
  }

  ParamCacheCB *param_cache_cb = down_cast<ParamCacheCB *>(param_cache);

  if (!param_cache_cb->ValidateAndCacheParams(effect_cb,
                                              draw_element,
                                              this,
                                              stream_bank_cb,
                                              material,
                                              override)) {
    // TODO: should we do this here, or on the service side ?
    // InsertMissingVertexStreams();
  }

  IndexBufferCB *index_buffer_cb =
      down_cast<IndexBufferCB *>(index_buffer());
  if (!index_buffer_cb) {
    // TODO: draw non-index in this case ? we don't do it currently on
    // other platforms, so keep compatibility.
    DLOG(INFO) << "Trying to draw with an empty index buffer.";
    return;
  }
  GAPIInterface::PrimitiveType cb_primitive_type =
      GetCBPrimitiveType(primitive_type_);
  if (cb_primitive_type == GAPIInterface::MAX_PRIMITIVE_TYPE) {
    DLOG(INFO) << "Invalid primitive type (" << primitive_type_ << ").";
    return;
  }

  // Make sure our streams are up to date (skinned, etc..)
  stream_bank_cb->UpdateStreams();

  stream_bank_cb->BindStreamsForRendering();

  CommandBufferHelper *helper = renderer_->helper();
  CommandBufferEntry args[6];

  // Sets current effect.
  // TODO: cache current effect ?
  args[0].value_uint32 = effect_cb->resource_id();
  helper->AddCommand(command_buffer::SET_EFFECT, 1, args);
  param_cache_cb->RunHandlers(helper);


  // Draws.
  args[0].value_uint32 = cb_primitive_type;
  args[1].value_uint32 = index_buffer_cb->resource_id();
  args[2].value_uint32 = 0;                     // first index.
  args[3].value_uint32 = number_primitives_;    // primitive count.
  args[4].value_uint32 = 0;                     // min index.
  args[5].value_uint32 = number_vertices_ - 1;  // max index.
  helper->AddCommand(command_buffer::DRAW_INDEXED, 6, args);
}

}  // namespace o3d
