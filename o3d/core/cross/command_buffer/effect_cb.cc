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


// This file implements the command-buffer version of the Effect class.

#include "core/cross/precompile.h"
#include "core/cross/error.h"
#include "core/cross/semantic_manager.h"
#include "core/cross/service_locator.h"
#include "core/cross/command_buffer/effect_cb.h"
#include "command_buffer/common/cross/buffer_sync_api.h"
#include "command_buffer/common/cross/cmd_buffer_format.h"
#include "command_buffer/client/cross/fenced_allocator.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"

namespace o3d {

using command_buffer::BufferSyncInterface;
using command_buffer::CommandBufferEntry;
using command_buffer::CommandBufferHelper;
using command_buffer::ResourceID;
namespace effect_param = command_buffer::effect_param;
namespace vertex_struct = command_buffer::vertex_struct;

EffectCB::EffectCB(ServiceLocator *service_locator, RendererCB *renderer)
    : Effect(service_locator),
      resource_id_(command_buffer::kInvalidResource),
      generation_(0),
      renderer_(renderer) {
}

EffectCB::~EffectCB() {
  Destroy();
}

bool EffectCB::LoadFromFXString(const String& source) {
  Destroy();
  // NOTE: Destroy() already increments the generation counter.
  String vp_main;
  String fp_main;
  MatrixLoadOrder load_order = ROW_MAJOR;
  // TODO: Check for failure once shader parser is in.
  if (!ValidateFX(source, &vp_main, &fp_main, &load_order)) {
    return false;
  }
  set_matrix_load_order(load_order);
  // buffer is vp_main \0 fp_main \0 effect_text
  // The source doesn't need to be 0-terminated, we are passing the size.
  size_t source_size = vp_main.size() + 1 + fp_main.size() + 1 + source.size();
  char *buffer_data = renderer_->allocator()->AllocTyped<char>(source_size);
  if (!buffer_data) {
    O3D_ERROR(service_locator()) << "Could not allocate " << source_size
                                 << " bytes to load the effect.";
    return false;
  }
  memcpy(buffer_data, vp_main.data(), vp_main.size());
  buffer_data[vp_main.size()] = 0;
  memcpy(buffer_data + vp_main.size() + 1, fp_main.data(), fp_main.size());
  buffer_data[vp_main.size() + 1 + fp_main.size()] = 0;
  memcpy(buffer_data + vp_main.size() + 1 + fp_main.size() + 1,
         source.data(), source.size());

  ResourceID resource_id = renderer_->effect_ids().AllocateID();

  CommandBufferHelper *helper = renderer_->helper();
  CommandBufferEntry args[4];
  args[0].value_uint32 = resource_id;
  args[1].value_uint32 = source_size;
  args[2].value_uint32 = renderer_->transfer_shm_id();
  args[3].value_uint32 = renderer_->allocator()->GetOffset(buffer_data);
  helper->AddCommand(command_buffer::CREATE_EFFECT, 4, args);
  renderer_->allocator()->FreePendingToken(buffer_data, helper->InsertToken());

  // NOTE: we're calling Finish to check the command result, to see if
  // the effect has succesfully compiled.
  helper->Finish();
  if (renderer_->sync_interface()->GetParseError() !=
      BufferSyncInterface::PARSE_NO_ERROR) {
    O3D_ERROR(service_locator()) << "Effect failed to compile.";
    renderer_->effect_ids().FreeID(resource_id);
    return false;
  }
  resource_id_ = resource_id;
  EffectHelper effect_helper(helper, renderer_->allocator(),
                             renderer_->transfer_shm_id(),
                             &renderer_->effect_param_ids());
  if (!effect_helper.CreateEffectParameters(resource_id, &param_descs_)) {
    O3D_ERROR(service_locator()) << "Failed to create effect parameters.";
    Destroy();
    return false;
  }
  for (unsigned int i = 0; i < param_descs_.size(); ++i) {
    if (!effect_helper.GetParamStrings(&(param_descs_[i]))) {
      O3D_ERROR(service_locator())
          << "Failed to create effect parameters strings.";
      Destroy();
      return false;
    }
  }
  if (!effect_helper.GetEffectStreams(resource_id, &stream_descs_)) {
    O3D_ERROR(service_locator()) << "Failed to get streams.";
    Destroy();
    return false;
  }
  set_source(source);
  return true;
}

void EffectCB::Destroy() {
  set_source("");
  ++generation_;
  if (resource_id_ != command_buffer::kInvalidResource) {
    CommandBufferHelper *helper = renderer_->helper();
    CommandBufferEntry args[1];
    args[0].value_uint32 = resource_id_;
    helper->AddCommand(command_buffer::DESTROY_EFFECT, 1, args);
    renderer_->effect_ids().FreeID(resource_id_);
    resource_id_ = command_buffer::kInvalidResource;
  }
  if (param_descs_.size() > 0) {
    EffectHelper effect_helper(renderer_->helper(), renderer_->allocator(),
                               renderer_->transfer_shm_id(),
                               &renderer_->effect_param_ids());
    effect_helper.DestroyEffectParameters(param_descs_);
    param_descs_.clear();
  }
}

static const ObjectBase::Class* CBTypeToParamType(
    effect_param::DataType type) {
  switch (type) {
    case effect_param::FLOAT1:
      return ParamFloat::GetApparentClass();
    case effect_param::FLOAT2:
      return ParamFloat2::GetApparentClass();
    case effect_param::FLOAT3:
      return ParamFloat3::GetApparentClass();
    case effect_param::FLOAT4:
      return ParamFloat4::GetApparentClass();
    case effect_param::INT:
      return ParamInteger::GetApparentClass();
    case effect_param::MATRIX4:
      return ParamMatrix4::GetApparentClass();
    case effect_param::SAMPLER:
      return ParamSampler::GetApparentClass();
    case effect_param::TEXTURE:
      return ParamTexture::GetApparentClass();
    default : {
      DLOG(ERROR) << "Cannot convert command buffer type "
                  << type
                  << " to a Param type.";
      return NULL;
    }
  }
}

void EffectCB::GetParameterInfo(EffectParameterInfoArray *array) {
  DCHECK(array);
  array->clear();
  SemanticManager *semantic_manager =
      service_locator()->GetService<SemanticManager>();
  for (unsigned int i = 0; i < param_descs_.size(); ++i) {
    EffectHelper::EffectParamDesc &desc = param_descs_[i];
    const ObjectBase::Class *param_class = CBTypeToParamType(desc.data_type);
    if (!param_class)
      continue;
    const ObjectBase::Class *sem_class = NULL;

    if (desc.semantic.size()) {
      sem_class = semantic_manager->LookupSemantic(desc.semantic);
    }
    array->push_back(EffectParameterInfo(desc.name,
                                         param_class,
                                         0,
                                         desc.semantic,
                                         sem_class));
  }
}


static bool CBSemanticToO3DSemantic(
    vertex_struct::Semantic semantic,
    unsigned int semantic_index,
    Stream::Semantic *out_semantic,
    unsigned int *out_semantic_index) {
  switch (semantic) {
    case vertex_struct::POSITION:
      if (semantic_index != 0) return false;
      *out_semantic = Stream::POSITION;
      *out_semantic_index = 0;
      return true;
    case vertex_struct::NORMAL:
      if (semantic_index != 0) return false;
      *out_semantic = Stream::NORMAL;
      *out_semantic_index = 0;
      return true;
    case vertex_struct::COLOR:
      if (semantic_index > 1) return false;
      *out_semantic = Stream::COLOR;
      *out_semantic_index = semantic_index;
      return true;
    case vertex_struct::TEX_COORD:
      if (semantic_index == 6) {
        *out_semantic = Stream::TANGENT;
        *out_semantic_index = 0;
        return true;
      } else if (semantic_index == 7) {
        *out_semantic = Stream::BINORMAL;
        *out_semantic_index = 0;
        return true;
      } else {
        *out_semantic = Stream::TEXCOORD;
        *out_semantic_index = semantic_index;
        return true;
      }
    default:
      return false;
  }
}
void EffectCB::GetStreamInfo(EffectStreamInfoArray *array) {
  DCHECK(array);
  array->clear();
  for (unsigned int i = 0; i < stream_descs_.size(); ++i) {
    Stream::Semantic semantic;
    unsigned int semantic_index;
    if (CBSemanticToO3DSemantic(stream_descs_[i].semantic,
                                stream_descs_[i].semantic_index,
                                &semantic,
                                &semantic_index)) {
      array->push_back(EffectStreamInfo(semantic, semantic_index));
    }
  }
}

}  // namespace o3d
