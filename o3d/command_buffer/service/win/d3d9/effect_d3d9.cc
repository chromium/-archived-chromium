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


// This file contains the implementation of the D3D9 versions of the
// Effect resource.
// This file also contains the related GAPID3D9 function implementations.

#include <algorithm>
#include "command_buffer/service/win/d3d9/d3d9_utils.h"
#include "command_buffer/service/win/d3d9/geometry_d3d9.h"
#include "command_buffer/service/win/d3d9/gapi_d3d9.h"
#include "command_buffer/service/win/d3d9/effect_d3d9.h"
#include "command_buffer/service/win/d3d9/sampler_d3d9.h"
#include "command_buffer/service/cross/effect_utils.h"

// TODO: remove link-dependency on D3DX.

namespace o3d {
namespace command_buffer {

// Logs the D3D effect error, from either the buffer, or GetLastError().
static void LogFXError(LPD3DXBUFFER error_buffer) {
  if (error_buffer) {
    LPVOID compile_errors = error_buffer->GetBufferPointer();
    LOG(ERROR) << "Failed to compile effect: "
               << static_cast<char *>(compile_errors);
  } else {
    HLOCAL hLocal = NULL;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ALLOCATE_BUFFER,
                  NULL,
                  GetLastError(),
                  0,
                  reinterpret_cast<wchar_t*>(&hLocal),
                  0,
                  NULL);
    wchar_t* msg = reinterpret_cast<wchar_t*>(LocalLock(hLocal));
    LOG(ERROR) << "Failed to compile effect: " << msg;
    LocalFree(hLocal);
  }
}

EffectD3D9::EffectD3D9(ID3DXEffect *d3d_effect,
                       ID3DXConstantTable *fs_constant_table,
                       IDirect3DVertexShader9 *d3d_vertex_shader)
    : d3d_effect_(d3d_effect),
      fs_constant_table_(fs_constant_table),
      d3d_vertex_shader_(d3d_vertex_shader),
      sync_parameters_(false) {
  for (unsigned int i = 0; i < kMaxSamplerUnits; ++i) {
    samplers_[i] = kInvalidResource;
  }
  SetStreams();
}
// Releases the D3D effect.
EffectD3D9::~EffectD3D9() {
  for (ParamList::iterator it = params_.begin(); it != params_.end(); ++it) {
    (*it)->ResetEffect();
  }
  DCHECK(d3d_effect_);
  d3d_effect_->Release();
  DCHECK(fs_constant_table_);
  fs_constant_table_->Release();
  DCHECK(d3d_vertex_shader_);
  d3d_vertex_shader_->Release();
}

// Compiles the effect, and checks that the effect conforms to what we expect
// (no extra technique or pass in the effect code, since one is implicitly added
// using the program entry points) and that it validates. If successful, wrap
// the D3D effect into a new EffectD3D9.
EffectD3D9 *EffectD3D9::Create(GAPID3D9 *gapi,
                               const String& effect_code,
                               const String& vertex_program_entry,
                               const String& fragment_program_entry) {
  String prepared_effect = effect_code +
      "technique Shaders { "
      "  pass p0 { "
      "    VertexShader = compile vs_2_0 " + vertex_program_entry + "();"
      "    PixelShader = compile ps_2_0 " + fragment_program_entry + "();"
      "  }"
      "};";
  ID3DXEffect *d3d_effect = NULL;
  LPD3DXBUFFER error_buffer;
  IDirect3DDevice9 *device = gapi->d3d_device();
  if (D3DXCreateEffect(device,
                       prepared_effect.c_str(),
                       prepared_effect.size(),
                       NULL,
                       NULL,
                       0,
                       NULL,
                       &d3d_effect,
                       &error_buffer) != D3D_OK) {
    LogFXError(error_buffer);
    return NULL;
  }
  // check that .
  D3DXEFFECT_DESC effect_desc;
  HR(d3d_effect->GetDesc(&effect_desc));
  if (effect_desc.Techniques != 1) {
    LOG(ERROR) << "Only 1 technique is allowed in an effect.";
    d3d_effect->Release();
    return NULL;
  }
  D3DXHANDLE technique = d3d_effect->GetTechnique(0);
  DCHECK(technique);
  if (d3d_effect->ValidateTechnique(technique) != D3D_OK) {
    LOG(ERROR) << "Technique doesn't validate.";
    d3d_effect->Release();
    return NULL;
  }
  D3DXTECHNIQUE_DESC technique_desc;
  HR(d3d_effect->GetTechniqueDesc(technique, &technique_desc));
  if (technique_desc.Passes != 1) {
    LOG(ERROR) << "Only 1 pass is allowed in an effect.";
    d3d_effect->Release();
    return NULL;
  }
  d3d_effect->SetTechnique(technique);
  D3DXHANDLE pass = d3d_effect->GetPass(technique, 0);
  D3DXPASS_DESC pass_desc;
  HR(d3d_effect->GetPassDesc(pass, &pass_desc));
  ID3DXConstantTable *table = NULL;
  HR(D3DXGetShaderConstantTable(pass_desc.pPixelShaderFunction,
                                &table));
  if (!table) {
    LOG(ERROR) << "Could not get the constant table.";
    d3d_effect->Release();
    return NULL;
  }
  IDirect3DVertexShader9 *d3d_vertex_shader = NULL;
  HR(device->CreateVertexShader(pass_desc.pVertexShaderFunction,
                                &d3d_vertex_shader));
  if (!d3d_vertex_shader) {
    d3d_effect->Release();
    table->Release();
    DLOG(ERROR) << "Failed to create vertex shader";
    return NULL;
  }

  return new EffectD3D9(d3d_effect, table, d3d_vertex_shader);
}

// Begins rendering with the effect, setting all the appropriate states.
bool EffectD3D9::Begin(GAPID3D9 *gapi) {
  UINT numpasses;
  HR(d3d_effect_->Begin(&numpasses, 0));
  HR(d3d_effect_->BeginPass(0));
  sync_parameters_ = false;
  return SetSamplers(gapi);
}

// Terminates rendering with the effect, resetting all the appropriate states.
void EffectD3D9::End(GAPID3D9 *gapi) {
  HR(d3d_effect_->EndPass());
  HR(d3d_effect_->End());
}

// Gets the parameter count from the D3D effect description.
unsigned int EffectD3D9::GetParamCount() {
  D3DXEFFECT_DESC effect_desc;
  HR(d3d_effect_->GetDesc(&effect_desc));
  return effect_desc.Parameters;
}

// Gets the number of input streams from the shader.
unsigned int EffectD3D9::GetStreamCount() {
  return streams_.size();
}

// Retrieves the matching DataType from a D3D parameter description.
static effect_param::DataType GetDataTypeFromD3D(
    const D3DXPARAMETER_DESC &desc) {
  switch (desc.Type) {
    case D3DXPT_FLOAT:
      switch (desc.Class) {
        case D3DXPC_SCALAR:
          return effect_param::FLOAT1;
        case D3DXPC_VECTOR:
          switch (desc.Columns) {
            case 2:
              return effect_param::FLOAT2;
            case 3:
              return effect_param::FLOAT3;
            case 4:
              return effect_param::FLOAT4;
            default:
              return effect_param::UNKNOWN;
          }
        case D3DXPC_MATRIX_ROWS:
        case D3DXPC_MATRIX_COLUMNS:
          if (desc.Columns == 4 && desc.Rows == 4) {
            return effect_param::MATRIX4;
          } else {
            return effect_param::UNKNOWN;
          }
        default:
          return effect_param::UNKNOWN;
      }
    case D3DXPT_INT:
      if (desc.Class == D3DXPC_SCALAR) {
        return effect_param::INT;
      } else {
        return effect_param::UNKNOWN;
      }
    case D3DXPT_BOOL:
      if (desc.Class == D3DXPC_SCALAR) {
        return effect_param::BOOL;
      } else {
        return effect_param::UNKNOWN;
      }
    case D3DXPT_SAMPLER:
    case D3DXPT_SAMPLER2D:
    case D3DXPT_SAMPLER3D:
    case D3DXPT_SAMPLERCUBE:
      if (desc.Class == D3DXPC_OBJECT) {
        return effect_param::SAMPLER;
      } else {
        return effect_param::UNKNOWN;
      }
    case D3DXPT_TEXTURE:
    case D3DXPT_TEXTURE1D:
    case D3DXPT_TEXTURE2D:
    case D3DXPT_TEXTURE3D:
    case D3DXPT_TEXTURECUBE:
      if (desc.Class == D3DXPC_OBJECT) {
        return effect_param::TEXTURE;
      } else {
        return effect_param::UNKNOWN;
      }
    default:
      return effect_param::UNKNOWN;
  }
}

// Gets a handle to the selected parameter, and wraps it into an
// EffectParamD3D9 if successful.
EffectParamD3D9 *EffectD3D9::CreateParam(unsigned int index) {
  D3DXHANDLE handle = d3d_effect_->GetParameter(NULL, index);
  if (!handle) return NULL;
  return EffectParamD3D9::Create(this, handle);
}

// Gets a handle to the selected parameter, and wraps it into an
// EffectParamD3D9 if successful.
EffectParamD3D9 *EffectD3D9::CreateParamByName(const char *name) {
  D3DXHANDLE handle = d3d_effect_->GetParameterByName(NULL, name);
  if (!handle) return NULL;
  return EffectParamD3D9::Create(this, handle);
}

bool EffectD3D9::CommitParameters(GAPID3D9 *gapi) {
  if (sync_parameters_) {
    sync_parameters_ = false;
    d3d_effect_->CommitChanges();
    return SetSamplers(gapi);
  } else {
    return true;
  }
}

bool EffectD3D9::SetSamplers(GAPID3D9 *gapi) {
  IDirect3DDevice9 *d3d_device = gapi->d3d_device();
  bool result = true;
  for (unsigned int i = 0; i < kMaxSamplerUnits; ++i) {
    SamplerD3D9 *sampler = gapi->GetSampler(samplers_[i]);
    if (sampler) {
      result &= sampler->ApplyStates(gapi, i);
    } else {
      HR(d3d_device->SetTexture(i, NULL));
    }
  }
  return result;
}

bool EffectD3D9::SetStreams() {
  if (!d3d_vertex_shader_) {
    return false;
  }
  UINT size;
  d3d_vertex_shader_->GetFunction(NULL, &size);
  scoped_array<DWORD> function(new DWORD[size]);
  d3d_vertex_shader_->GetFunction(function.get(), &size);

  UINT num_semantics;
  HR(D3DXGetShaderInputSemantics(function.get(),
                                 NULL,
                                 &num_semantics));
  scoped_array<D3DXSEMANTIC> semantics(new D3DXSEMANTIC[num_semantics]);
  HR(D3DXGetShaderInputSemantics(function.get(),
                                 semantics.get(),
                                 &num_semantics));

  streams_.resize(num_semantics);
  for (UINT i = 0; i < num_semantics; ++i) {
    vertex_struct::Semantic semantic;
    unsigned int semantic_index;
    if (D3DSemanticToCBSemantic(static_cast<D3DDECLUSAGE>(semantics[i].Usage),
                                static_cast<int>(semantics[i].UsageIndex),
                                &semantic, &semantic_index)) {
      streams_[i].semantic = semantic;
      streams_[i].semantic_index = semantic_index;
    }
  }
  return true;
}

void EffectD3D9::LinkParam(EffectParamD3D9 *param) {
  params_.push_back(param);
}

void EffectD3D9::UnlinkParam(EffectParamD3D9 *param) {
  std::remove(params_.begin(), params_.end(), param);
}

// Fills the Desc structure, appending name and semantic if any, and if enough
// room is available in the buffer.
bool EffectD3D9::GetStreamDesc(unsigned int index,
                               unsigned int size,
                               void *data) {
  using effect_stream::Desc;
  if (size < sizeof(Desc))  // NOLINT
    return false;

  Desc stream = streams_[index];
  Desc *desc = static_cast<Desc *>(data);
  memset(desc, 0, sizeof(*desc));
  desc->semantic = stream.semantic;
  desc->semantic_index = stream.semantic_index;
  return true;
}

EffectParamD3D9::EffectParamD3D9(effect_param::DataType data_type,
                                 EffectD3D9 *effect,
                                 D3DXHANDLE handle)
    : EffectParam(data_type),
      effect_(effect),
      handle_(handle),
      sampler_units_(NULL),
      sampler_unit_count_(0) {
  DCHECK(effect_);
  effect_->LinkParam(this);
}

EffectParamD3D9::~EffectParamD3D9() {
  if (effect_) effect_->UnlinkParam(this);
}

EffectParamD3D9 *EffectParamD3D9::Create(EffectD3D9 *effect,
                                         D3DXHANDLE handle) {
  DCHECK(effect);
  D3DXPARAMETER_DESC desc;
  HR(effect->d3d_effect_->GetParameterDesc(handle, &desc));
  effect_param::DataType data_type = GetDataTypeFromD3D(desc);
  EffectParamD3D9 *param = new EffectParamD3D9(data_type, effect, handle);
  if (data_type == effect_param::SAMPLER) {
    ID3DXConstantTable *table = effect->fs_constant_table_;
    DCHECK(table);
    D3DXHANDLE sampler_handle = table->GetConstantByName(NULL, desc.Name);
    if (sampler_handle) {
      D3DXCONSTANT_DESC desc_array[kMaxSamplerUnits];
      unsigned int num_desc = kMaxSamplerUnits;
      table->GetConstantDesc(sampler_handle, desc_array, &num_desc);
      // We have no good way of querying how many descriptions would really be
      // returned as we're capping the number to kMaxSamplerUnits (which should
      // be more than sufficient). If however we do end up with the max number
      // there's a chance that there were actually more so let's log it.
      if (num_desc == kMaxSamplerUnits) {
        DLOG(WARNING) << "Number of constant descriptions might have exceeded "
                      << "the maximum of " << kMaxSamplerUnits;
      }
      param->sampler_unit_count_ = 0;
      if (num_desc > 0) {
        param->sampler_units_.reset(new unsigned int[num_desc]);
        for (unsigned int desc_index = 0; desc_index < num_desc; desc_index++) {
          D3DXCONSTANT_DESC constant_desc = desc_array[desc_index];
          if (constant_desc.Class == D3DXPC_OBJECT &&
              (constant_desc.Type == D3DXPT_SAMPLER ||
               constant_desc.Type == D3DXPT_SAMPLER2D ||
               constant_desc.Type == D3DXPT_SAMPLER3D ||
               constant_desc.Type == D3DXPT_SAMPLERCUBE)) {
            param->sampler_units_[param->sampler_unit_count_++] =
                constant_desc.RegisterIndex;
          }
        }
      }
    }
    // if the sampler hasn't been found in the constant table, that means it
    // isn't referenced, hence it doesn't use any sampler unit.
  }
  return param;
}

// Fills the Desc structure, appending name and semantic if any, and if enough
// room is available in the buffer.
bool EffectParamD3D9::GetDesc(unsigned int size, void *data) {
  using effect_param::Desc;
  if (size < sizeof(Desc))  // NOLINT
    return false;
  if (!effect_)
    return false;
  ID3DXEffect *d3d_effect = effect_->d3d_effect_;
  D3DXPARAMETER_DESC d3d_desc;
  HR(d3d_effect->GetParameterDesc(handle_, &d3d_desc));
  unsigned int name_size =
      d3d_desc.Name ? static_cast<unsigned int>(strlen(d3d_desc.Name)) + 1 : 0;
  unsigned int semantic_size = d3d_desc.Semantic ?
      static_cast<unsigned int>(strlen(d3d_desc.Semantic)) + 1 : 0;
  unsigned int total_size = sizeof(Desc) + name_size + semantic_size;  // NOLINT

  Desc *desc = static_cast<Desc *>(data);
  memset(desc, 0, sizeof(*desc));
  desc->size = total_size;
  desc->data_type = data_type();
  desc->data_size = GetDataSize(data_type());
  desc->name_offset = 0;
  desc->name_size = name_size;
  desc->semantic_offset = 0;
  desc->semantic_size = semantic_size;
  unsigned int current_offset = sizeof(Desc);
  if (d3d_desc.Name && current_offset + name_size <= size) {
    desc->name_offset = current_offset;
    memcpy(static_cast<char *>(data) + current_offset,
           d3d_desc.Name, name_size);
    current_offset += name_size;
  }
  if (d3d_desc.Semantic && current_offset + semantic_size <= size) {
    desc->semantic_offset = current_offset;
    memcpy(static_cast<char *>(data) + current_offset,
           d3d_desc.Semantic, semantic_size);
    current_offset += semantic_size;
  }
  return true;
}

// Sets the data into the D3D effect parameter, using the appropriate D3D call.
bool EffectParamD3D9::SetData(GAPID3D9 *gapi,
                              unsigned int size,
                              const void * data) {
  if (!effect_)
    return false;
  ID3DXEffect *d3d_effect = effect_->d3d_effect_;
  effect_param::DataType type = data_type();
  if (size < effect_param::GetDataSize(type)) return false;
  switch (type) {
    case effect_param::FLOAT1:
      HR(d3d_effect->SetFloat(handle_, *static_cast<const float *>(data)));
      break;
    case effect_param::FLOAT2:
      HR(d3d_effect->SetFloatArray(handle_, static_cast<const float *>(data),
                                   2));
      break;
    case effect_param::FLOAT3:
      HR(d3d_effect->SetFloatArray(handle_, static_cast<const float *>(data),
                                   3));
      break;
    case effect_param::FLOAT4:
      HR(d3d_effect->SetFloatArray(handle_, static_cast<const float *>(data),
                                   4));
      break;
    case effect_param::MATRIX4:
      HR(d3d_effect->SetMatrix(handle_,
                               reinterpret_cast<const D3DXMATRIX *>(data)));
      break;
    case effect_param::INT:
      HR(d3d_effect->SetInt(handle_, *static_cast<const int *>(data)));
      break;
    case effect_param::BOOL:
      HR(d3d_effect->SetBool(handle_, *static_cast<const bool *>(data)?1:0));
      break;
    case effect_param::SAMPLER: {
      ResourceID id = *static_cast<const ResourceID *>(data);
      for (unsigned int i = 0; i < sampler_unit_count_; ++i) {
        effect_->samplers_[sampler_units_[i]] = id;
      }
      break;
    }
    case effect_param::TEXTURE: {
      // TODO(rlp): finish
      break;
    }
    default:
      DLOG(ERROR) << "Invalid parameter type.";
      return false;
  }
  if (effect_ == gapi->current_effect()) {
    effect_->sync_parameters_ = true;
  }
  return true;
}

// Calls EffectD3D9::Create, and assign the result to the resource ID.
// If changing the current effect, dirty it.
BufferSyncInterface::ParseError GAPID3D9::CreateEffect(
    ResourceID id,
    unsigned int size,
    const void *data) {
  if (id == current_effect_id_) DirtyEffect();
  // Even though Assign would Destroy the effect at id, we do it explicitly in
  // case the creation fails.
  effects_.Destroy(id);
  // Data is vp_main \0 fp_main \0 effect_text.
  String vertex_program_entry;
  String fragment_program_entry;
  String effect_code;
  if (!ParseEffectData(size, data,
                       &vertex_program_entry,
                       &fragment_program_entry,
                       &effect_code)) {
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  }
  EffectD3D9 * effect = EffectD3D9::Create(this, effect_code,
                                           vertex_program_entry,
                                           fragment_program_entry);
  if (!effect) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  effects_.Assign(id, effect);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

// Destroys the Effect resource.
// If destroying the current effect, dirty it.
BufferSyncInterface::ParseError GAPID3D9::DestroyEffect(ResourceID id) {
  if (id == current_effect_id_) DirtyEffect();
  return effects_.Destroy(id) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

// Sets the current effect ID, dirtying the current effect.
BufferSyncInterface::ParseError GAPID3D9::SetEffect(ResourceID id) {
  DirtyEffect();
  current_effect_id_ = id;
  return BufferSyncInterface::PARSE_NO_ERROR;
}

// Gets the param count from the effect and store it in the memory buffer.
BufferSyncInterface::ParseError GAPID3D9::GetParamCount(
    ResourceID id,
    unsigned int size,
    void *data) {
  EffectD3D9 *effect = effects_.Get(id);
  if (!effect || size < sizeof(Uint32))  // NOLINT
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  *static_cast<Uint32 *>(data) = effect->GetParamCount();
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPID3D9::CreateParam(
    ResourceID param_id,
    ResourceID effect_id,
    unsigned int index) {
  EffectD3D9 *effect = effects_.Get(effect_id);
  if (!effect) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  EffectParamD3D9 *param = effect->CreateParam(index);
  if (!param) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  effect_params_.Assign(param_id, param);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPID3D9::CreateParamByName(
    ResourceID param_id,
    ResourceID effect_id,
    unsigned int size,
    const void *name) {
  EffectD3D9 *effect = effects_.Get(effect_id);
  if (!effect) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  std::string string_name(static_cast<const char *>(name), size);
  EffectParamD3D9 *param = effect->CreateParamByName(string_name.c_str());
  if (!param) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  effect_params_.Assign(param_id, param);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPID3D9::DestroyParam(ResourceID id) {
  return effect_params_.Destroy(id) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

BufferSyncInterface::ParseError GAPID3D9::SetParamData(
    ResourceID id,
    unsigned int size,
    const void *data) {
  EffectParamD3D9 *param = effect_params_.Get(id);
  if (!param) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return param->SetData(this, size, data) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

BufferSyncInterface::ParseError GAPID3D9::GetParamDesc(
    ResourceID id,
    unsigned int size,
    void *data) {
  EffectParamD3D9 *param = effect_params_.Get(id);
  if (!param) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return param->GetDesc(size, data) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

// Gets the stream count from the effect and store it in the memory buffer.
BufferSyncInterface::ParseError GAPID3D9::GetStreamCount(
    ResourceID id,
    unsigned int size,
    void *data) {
  EffectD3D9 *effect = effects_.Get(id);
  if (!effect || size < sizeof(Uint32))  // NOLINT
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  *static_cast<Uint32 *>(data) = effect->GetStreamCount();
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPID3D9::GetStreamDesc(
    ResourceID id,
    unsigned int index,
    unsigned int size,
    void *data) {
  EffectD3D9 *effect = effects_.Get(id);
  if (!effect) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return effect->GetStreamDesc(index, size, data) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}


// If the current effect is valid, call End on it, and tag for revalidation.
void GAPID3D9::DirtyEffect() {
  if (validate_effect_) return;
  DCHECK(current_effect_);
  current_effect_->End(this);
  current_effect_ = NULL;
  validate_effect_ = true;
}

// Gets the current effect, and calls Begin on it (if successful).
// Should only be called if the current effect is not valid.
bool GAPID3D9::ValidateEffect() {
  DCHECK(validate_effect_);
  DCHECK(!current_effect_);
  current_effect_ = effects_.Get(current_effect_id_);
  if (!current_effect_) return false;
  validate_effect_ = false;
  return current_effect_->Begin(this);
}

}  // namespace command_buffer
}  // namespace o3d
