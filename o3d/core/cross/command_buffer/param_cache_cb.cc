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


// This file contains the implementation of the ParamCacheCB class.

#include "core/cross/precompile.h"
#include "core/cross/draw_element.h"
#include "core/cross/element.h"
#include "core/cross/semantic_manager.h"
#include "core/cross/command_buffer/effect_cb.h"
#include "core/cross/command_buffer/param_cache_cb.h"
#include "core/cross/command_buffer/sampler_cb.h"
#include "command_buffer/common/cross/cmd_buffer_format.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"

namespace o3d {

using command_buffer::CommandBufferEntry;
using command_buffer::CommandBufferHelper;
using command_buffer::EffectHelper;
using command_buffer::ResourceID;
namespace effect_param = command_buffer::effect_param;

// Base class for ParamHandlers.
class ParamHandlerCB {
 public:
  virtual ~ParamHandlerCB() {}
  virtual void SetValue(CommandBufferHelper *helper) = 0;
};

// Template implementation of ParamHandlerCB.
template <typename T>
class TypedParamHandlerCB : public ParamHandlerCB {
 public:
  TypedParamHandlerCB(T* param, ResourceID id)
      : param_(param),
        id_(id) {
  }

  // Sends the param value to the service.
  // This template definition only works for value types (floatn, matrix, int,
  // ..., but not textures or samplers).
  virtual void SetValue(CommandBufferHelper *helper) {
    static const unsigned int kEntryCount =
        (sizeof(typename T::DataType) + 3) / 4;
    CommandBufferEntry args[2 + kEntryCount];
    typename T::DataType value = param_->value();
    args[0].value_uint32 = id_;
    args[1].value_uint32 = sizeof(value);
    memcpy(args + 2, &value, sizeof(value));
    helper->AddCommand(command_buffer::SET_PARAM_DATA_IMMEDIATE,
                       2 + kEntryCount, args);
  }
 private:
  T* param_;
  ResourceID id_;
};

// Matrices are expected in row major order in the command buffer, so
// TypedParamHandlerCB<ParamMatrix4> works for row major, and we make a new
// class for column major.
typedef TypedParamHandlerCB<ParamMatrix4> MatrixParamHandlerRowsCB;

class MatrixParamHandlerColumnsCB : public ParamHandlerCB {
 public:
  MatrixParamHandlerColumnsCB(ParamMatrix4* param, ResourceID id)
      : param_(param),
        id_(id) {
  }

  // Sends the param value to the service.
  virtual void SetValue(CommandBufferHelper *helper) {
    CommandBufferEntry args[18];
    Matrix4 value = transpose(param_->value());
    args[0].value_uint32 = id_;
    args[1].value_uint32 = sizeof(value);
    memcpy(args + 2, &value, sizeof(value));
    helper->AddCommand(command_buffer::SET_PARAM_DATA_IMMEDIATE, 18, args);
  }
 private:
  ParamMatrix4* param_;
  ResourceID id_;
};

class SamplerParamHandlerCB : public ParamHandlerCB {
 public:
  SamplerParamHandlerCB(ParamSampler* param, ResourceID id)
      : param_(param),
        id_(id) {
  }

  // Sends the param value to the service.
  virtual void SetValue(CommandBufferHelper *helper) {
    SamplerCB *sampler = down_cast<SamplerCB *>(param_->value());
    CommandBufferEntry args[3];
    args[0].value_uint32 = id_;
    args[1].value_uint32 = sizeof(ResourceID);
    if (!sampler) {
      // TODO: use error sampler
      args[2].value_uint32 = command_buffer::kInvalidResource;
    } else {
      sampler->SetTextureAndStates();
      args[2].value_uint32 = sampler->resource_id();
    }
    helper->AddCommand(command_buffer::SET_PARAM_DATA_IMMEDIATE, 3, args);
  }
 private:
  ParamSampler* param_;
  ResourceID id_;
};

static ParamHandlerCB *GetHandlerFromParamAndDesc(
    Param *param,
    const EffectHelper::EffectParamDesc &desc,
    Effect::MatrixLoadOrder matrix_load_order) {
  switch (desc.data_type) {
    case effect_param::MATRIX4:
      if (param->IsA(ParamMatrix4::GetApparentClass())) {
        DCHECK_EQ(sizeof(ParamMatrix4::DataType), desc.data_size);
        ParamMatrix4 *matrix_param = down_cast<ParamMatrix4*>(param);
        if (matrix_load_order == Effect::ROW_MAJOR) {
          return new MatrixParamHandlerRowsCB(matrix_param, desc.id);
        } else {
          return new MatrixParamHandlerColumnsCB(matrix_param, desc.id);
        }
      }
      break;
    case effect_param::FLOAT1:
      if (param->IsA(ParamFloat::GetApparentClass())) {
        DCHECK_EQ(sizeof(ParamFloat::DataType), desc.data_size);
        return new TypedParamHandlerCB<ParamFloat>(
            down_cast<ParamFloat*>(param), desc.id);
      }
      break;
    case effect_param::FLOAT2:
      if (param->IsA(ParamFloat2::GetApparentClass())) {
        DCHECK_EQ(sizeof(ParamFloat2::DataType), desc.data_size);
        return new TypedParamHandlerCB<ParamFloat2>(
            down_cast<ParamFloat2*>(param), desc.id);
      }
      break;
    case effect_param::FLOAT3:
      if (param->IsA(ParamFloat3::GetApparentClass())) {
        DCHECK_EQ(sizeof(ParamFloat3::DataType), desc.data_size);
        return new TypedParamHandlerCB<ParamFloat3>(
            down_cast<ParamFloat3*>(param), desc.id);
      }
      break;
    case effect_param::FLOAT4:
      if (param->IsA(ParamFloat4::GetApparentClass())) {
        DCHECK_EQ(sizeof(ParamFloat4::DataType), desc.data_size);
        return new TypedParamHandlerCB<ParamFloat4>(
            down_cast<ParamFloat4*>(param), desc.id);
      }
      break;
    case effect_param::INT:
      if (param->IsA(ParamInteger::GetApparentClass())) {
        DCHECK_EQ(sizeof(ParamInteger::DataType), desc.data_size);
        return new TypedParamHandlerCB<ParamInteger>(
            down_cast<ParamInteger*>(param), desc.id);
      }
      break;
    case effect_param::BOOL:
      if (param->IsA(ParamBoolean::GetApparentClass())) {
        DCHECK_EQ(sizeof(ParamBoolean::DataType), desc.data_size);
        return new TypedParamHandlerCB<ParamBoolean>(
            down_cast<ParamBoolean*>(param), desc.id);
      }
      break;
    case effect_param::SAMPLER:
      if (param->IsA(ParamSampler::GetApparentClass())) {
        DCHECK_EQ(sizeof(ResourceID), desc.data_size);
        return new SamplerParamHandlerCB(down_cast<ParamSampler*>(param),
                                         desc.id);
      }
      break;
    default:
      break;
  }
  // Do not report an error, it may be valid that the Param didn't match the
  // desc, we may still be looking for another match in other nodes.
  return NULL;
}

ParamCacheCB::ParamCacheCB()
    : last_effect_generation_(UINT_MAX),
      last_matrix_load_order_(Effect::ROW_MAJOR) {
}

ParamCacheCB::~ParamCacheCB() {
  ClearHandlers();
}

bool ParamCacheCB::ValidateEffect(Effect* effect) {
  DCHECK(effect);

  EffectCB* effect_cb = down_cast<EffectCB*>(effect);
  return (effect_cb->generation_ == last_effect_generation_) &&
      (effect_cb->matrix_load_order() == last_matrix_load_order_);
}

void ParamCacheCB::UpdateCache(Effect* effect,
                               DrawElement* draw_element,
                               Element* element,
                               Material* material,
                               ParamObject* override) {
  DLOG_ASSERT(effect);
  EffectCB* effect_cb = down_cast<EffectCB*>(effect);
  ClearHandlers();
  SemanticManager *semantic_manager =
      effect->service_locator()->GetService<SemanticManager>();

  Effect::MatrixLoadOrder load_order = effect_cb->matrix_load_order();

  ParamObject *param_objects[] = {
    override,
    draw_element,
    element,
    material,
    effect,
    semantic_manager->sas_param_object()
  };
  const unsigned int kParamObjectCount = arraysize(param_objects);
  for (unsigned int i = 0; i < effect_cb->param_descs_.size(); ++i) {
    EffectHelper::EffectParamDesc &desc = effect_cb->param_descs_[i];
    const ObjectBase::Class *sem_class = NULL;
    if (desc.semantic.size()) {
      sem_class = semantic_manager->LookupSemantic(desc.semantic);
    }
    ParamHandlerCB *handler = NULL;

    // find a valid param on one of the ParamObjects, and create a handler for
    // it.
    for (unsigned int j = 0; j < kParamObjectCount; ++j) {
      ParamObject *param_object = param_objects[j];
      Param *param = param_object->GetUntypedParam(desc.name);
      if (!param && sem_class) {
        param = param_object->GetUntypedParam(sem_class->name());
      }
      if (param) {
        handler = GetHandlerFromParamAndDesc(param, desc, load_order);
        if (handler)
          break;
      }
    }
    if (!handler) {
      DLOG(ERROR) << "Did not find a param matching " << desc.name;
      continue;
    }
    handlers_.push_back(handler);
  }

  last_matrix_load_order_ = load_order;
  last_effect_generation_ = effect_cb->generation_;
}

void ParamCacheCB::ClearHandlers() {
  for (unsigned int i = 0; i < handlers_.size(); ++i) {
    delete handlers_[i];
  }
  handlers_.clear();
}

void ParamCacheCB::RunHandlers(CommandBufferHelper *helper) {
  for (unsigned int i = 0; i < handlers_.size(); ++i) {
    handlers_[i]->SetValue(helper);
  }
}

}  // namespace o3d
