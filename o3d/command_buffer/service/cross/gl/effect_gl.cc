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


// This file contains the implementation of the EffectParamGL and EffectGL
// classes, as well as the effect-related GAPI functions on GL.

#include <map>

#include "base/cross/std_functional.h"
#include "command_buffer/service/cross/gl/gapi_gl.h"
#include "command_buffer/service/cross/effect_utils.h"

namespace o3d {
namespace command_buffer {

EffectParamGL::EffectParamGL(effect_param::DataType data_type,
                             EffectGL *effect,
                             unsigned int param_index)
    : EffectParam(data_type),
      effect_(effect),
      low_level_param_index_(param_index) {
  DCHECK(effect_);
  effect_->LinkParam(this);
}

EffectParamGL::~EffectParamGL() {
  if (effect_)
    effect_->UnlinkParam(this);
}

static effect_param::DataType CgTypeToCBType(CGtype cg_type) {
  switch (cg_type) {
    case CG_FLOAT:
    case CG_FLOAT1:
      return effect_param::FLOAT1;
    case CG_FLOAT2:
      return effect_param::FLOAT2;
    case CG_FLOAT3:
      return effect_param::FLOAT3;
    case CG_FLOAT4:
      return effect_param::FLOAT4;
    case CG_INT:
    case CG_INT1:
      return effect_param::INT;
    case CG_BOOL:
    case CG_BOOL1:
      return effect_param::BOOL;
    case CG_FLOAT4x4:
      return effect_param::MATRIX4;
    case CG_SAMPLER:
    case CG_SAMPLER1D:
    case CG_SAMPLER2D:
    case CG_SAMPLER3D:
    case CG_SAMPLERCUBE:
      return effect_param::SAMPLER;
    default : {
      DLOG(INFO) << "Cannot convert CGtype "
                 << cgGetTypeString(cg_type)
                 << " to a Param type.";
      return effect_param::UNKNOWN;
    }
  }
}

EffectParamGL *EffectParamGL::Create(EffectGL *effect,
                                     unsigned int index) {
  DCHECK(effect);
  const EffectGL::LowLevelParam &low_level_param =
      effect->low_level_params_[index];
  CGtype cg_type =
      cgGetParameterType(EffectGL::GetEitherCgParameter(low_level_param));
  effect_param::DataType type = CgTypeToCBType(cg_type);
  if (type == effect_param::UNKNOWN) return NULL;
  return new EffectParamGL(type, effect, index);
}

// Fills the Desc structure, appending name and semantic if any, and if enough
// room is available in the buffer.
bool EffectParamGL::GetDesc(unsigned int size, void *data) {
  using effect_param::Desc;
  if (size < sizeof(Desc))  // NOLINT
    return false;
  if (!effect_)
    return false;
  const EffectGL::LowLevelParam &low_level_param =
      effect_->low_level_params_[low_level_param_index_];
  CGparameter cg_param = EffectGL::GetEitherCgParameter(low_level_param);
  const char *name = low_level_param.name;
  const char* semantic = cgGetParameterSemantic(cg_param);
  unsigned int name_size =
      name ? static_cast<unsigned int>(strlen(name)) + 1 : 0;
  unsigned int semantic_size = semantic ?
      static_cast<unsigned int>(strlen(semantic)) + 1 : 0;
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
  if (name && current_offset + name_size <= size) {
    desc->name_offset = current_offset;
    memcpy(static_cast<char *>(data) + current_offset, name, name_size);
    current_offset += name_size;
  }
  if (semantic && current_offset + semantic_size <= size) {
    desc->semantic_offset = current_offset;
    memcpy(static_cast<char *>(data) + current_offset, semantic, semantic_size);
    current_offset += semantic_size;
  }
  return true;
}

// Sets the data into the Cg effect parameter, using the appropriate Cg call.
bool EffectParamGL::SetData(GAPIGL *gapi,
                            unsigned int size,
                            const void * data) {
  if (!effect_)
    return false;

  EffectGL::LowLevelParam &low_level_param =
      effect_->low_level_params_[low_level_param_index_];
  CGparameter vp_param = low_level_param.vp_param;
  CGparameter fp_param = low_level_param.fp_param;
  effect_param::DataType type = data_type();
  if (size < effect_param::GetDataSize(type))
    return false;

  switch (type) {
    case effect_param::FLOAT1:
      if (vp_param)
        cgSetParameter1f(vp_param, *static_cast<const float *>(data));
      if (fp_param)
        cgSetParameter1f(fp_param, *static_cast<const float *>(data));
      break;
    case effect_param::FLOAT2:
      if (vp_param)
        cgSetParameter2fv(vp_param, static_cast<const float *>(data));
      if (fp_param)
        cgSetParameter2fv(fp_param, static_cast<const float *>(data));
      break;
    case effect_param::FLOAT3:
      if (vp_param)
        cgSetParameter3fv(vp_param, static_cast<const float *>(data));
      if (fp_param)
        cgSetParameter3fv(fp_param, static_cast<const float *>(data));
      break;
    case effect_param::FLOAT4:
      if (vp_param)
        cgSetParameter4fv(vp_param, static_cast<const float *>(data));
      if (fp_param)
        cgSetParameter4fv(fp_param, static_cast<const float *>(data));
      break;
    case effect_param::MATRIX4:
      if (vp_param)
        cgSetMatrixParameterfr(vp_param, static_cast<const float *>(data));
      if (fp_param)
        cgSetMatrixParameterfr(fp_param, static_cast<const float *>(data));
      break;
    case effect_param::INT:
      if (vp_param) cgSetParameter1i(vp_param, *static_cast<const int *>(data));
      if (fp_param) cgSetParameter1i(fp_param, *static_cast<const int *>(data));
      break;
    case effect_param::BOOL: {
      int bool_value = *static_cast<const bool *>(data)?1:0;
      if (vp_param) cgSetParameter1i(vp_param, bool_value);
      if (fp_param) cgSetParameter1i(fp_param, bool_value);
      break;
    }
    case effect_param::SAMPLER: {
      low_level_param.sampler_id = *static_cast<const ResourceID *>(data);
      if (effect_ == gapi->current_effect()) {
        gapi->DirtyEffect();
      }
      break;
    }
    default:
      DLOG(ERROR) << "Invalid parameter type.";
      return false;
  }
  return true;
}
EffectGL::EffectGL(CGprogram vertex_program,
                   CGprogram fragment_program)
    : vertex_program_(vertex_program),
      fragment_program_(fragment_program),
      update_samplers_(true) {
}

EffectGL::~EffectGL() {
  for (ParamResourceList::iterator it = resource_params_.begin();
       it != resource_params_.end(); ++it) {
    (*it)->ResetEffect();
  }
}

void EffectGL::LinkParam(EffectParamGL *param) {
  resource_params_.push_back(param);
}

void EffectGL::UnlinkParam(EffectParamGL *param) {
  std::remove(resource_params_.begin(), resource_params_.end(), param);
}

// Rewrites vertex program assembly code to match GL semantics for clipping.
// This parses the source, breaking it down into pieces:
// - declaration ("!!ARBvp1.0")
// - comments (that contain the parameter information)
// - instructions
// - "END" token.
// Then it rewrites the instructions so that 'result.position' doesn't get
// written directly, instead it is written to a temporary variable. Then a
// transformation is done on that variable before outputing to
// 'result.position':
// - offset x an y by half a pixel (times w).
// - remap z from [0..w] to [-w..w].
//
// Note that for the 1/2 pixel offset, we need a parameter that depends on the
// current viewport. This is done through 'program.env[0]' which is shared
// across all programs (so we only have to update it once when we change the
// viewport), because Cg won't use them currently (it uses 'program.local'
// instead).
static bool RewriteVertexProgramSource(String *source) {
  String::size_type pos = source->find('\n');
  if (pos == String::npos) {
    DLOG(ERROR) << "could not find program declaration";
    return false;
  }
  String decl(*source, 0, pos + 1);
  String::size_type start_comments = pos + 1;
  // skip the comments that contain the parameters etc.
  for (; pos < source->size(); pos = source->find('\n', pos)) {
    ++pos;
    if (pos >= source->size())
      break;
    if ((*source)[pos] != '#')
      break;
  }
  if (pos >= source->size()) {
    // we only found comments.
    return false;
  }
  String comments(*source, start_comments, pos - start_comments);

  String::size_type end_token = source->find("\nEND", pos + 1);
  if (end_token == String::npos) {
    DLOG(ERROR) << "Compiled shader doesn't have an END token";
    return false;
  }
  String instructions(*source, pos, end_token + 1 - pos);

  // Replace accesses to 'result.position' by accesses to our temp variable
  // '$O3D_HPOS'.
  // '$' is a valid symbol for identifiers, but Cg doesn't seem to be using
  // it, so we can use it to ensure we don't have name conflicts.
  static const char kOutPositionRegister[] = "result.position";
  for (String::size_type i = instructions.find(kOutPositionRegister);
       i < String::npos; i = instructions.find(kOutPositionRegister, i)) {
    instructions.replace(i, strlen(kOutPositionRegister), "$O3D_HPOS");
  }

  *source = decl +
            comments +
            // .x = 1/viewport.width; .y = 1/viewport.height; .z = 2.0;
            "PARAM $O3D_HELPER = program.env[0];\n"
            "TEMP $O3D_HPOS;\n" +
            instructions +
            // hpos.x <- hpos.x + hpos.w / viewport.width;
            // hpos.y <- hpos.y - hpos.w / viewport.height;
            "MAD $O3D_HPOS.xy, $O3D_HELPER.xyyy, $O3D_HPOS.w, $O3D_HPOS.xyyy;\n"
            // hpos.z <- hpos.z * 2 - hpos.w
            "MAD $O3D_HPOS.z, $O3D_HPOS.z, $O3D_HELPER.z, -$O3D_HPOS.w;\n"
            "MOV result.position, $O3D_HPOS;\n"
            "END\n";
  return true;
}

EffectGL *EffectGL::Create(GAPIGL *gapi,
                           const String& effect_code,
                           const String& vertex_program_entry,
                           const String& fragment_program_entry) {
  CGcontext context = gapi->cg_context();
  // Compile the original vertex program once, to get the ARBVP1 assembly code.
  CGprogram original_vp = cgCreateProgram(
      context, CG_SOURCE, effect_code.c_str(), CG_PROFILE_ARBVP1,
      vertex_program_entry.c_str(), NULL);
  const char* listing = cgGetLastListing(context);
  if (original_vp == NULL) {
    DLOG(ERROR) << "Effect Compile Error: " << cgGetErrorString(cgGetError())
                << " : " << listing;
    return NULL;
  }

  if (listing && listing[0] != 0) {
    DLOG(WARNING) << "Effect Compile Warnings: " << listing;
  }

  String vp_assembly = cgGetProgramString(original_vp, CG_COMPILED_PROGRAM);
  cgDestroyProgram(original_vp);
  if (!RewriteVertexProgramSource(&vp_assembly)) {
    return NULL;
  }
  CGprogram vertex_program = cgCreateProgram(
      context, CG_OBJECT, vp_assembly.c_str(), CG_PROFILE_ARBVP1,
      vertex_program_entry.c_str(), NULL);
  listing = cgGetLastListing(context);
  if (vertex_program == NULL) {
    DLOG(ERROR) << "Effect post-rewrite Compile Error: "
                << cgGetErrorString(cgGetError()) << " : " << listing;
    return false;
  }

  if (listing && listing[0] != 0) {
    DLOG(WARNING) << "Effect post-rewrite compile warnings: " << listing;
  }

  CHECK_GL_ERROR();

  // If the program rewrite introduced some syntax or semantic errors, we won't
  // know it until we load the program (through a GL error).
  // So flush all GL errors first...
  do {} while (glGetError() != GL_NO_ERROR);

  // ... Then load the program ...
  cgGLLoadProgram(vertex_program);

  // ... And check for GL errors.
  if (glGetError() != GL_NO_ERROR) {
    DLOG(ERROR) << "Effect post-rewrite GL Error: "
                << glGetString(GL_PROGRAM_ERROR_STRING_ARB)
                << "\nSource: \n"
                << vp_assembly;
    return NULL;
  }

  CGprogram fragment_program = cgCreateProgram(
      context, CG_SOURCE, effect_code.c_str(), CG_PROFILE_ARBFP1,
      fragment_program_entry.c_str(), NULL);
  listing = cgGetLastListing(context);
  if (fragment_program == NULL) {
    DLOG(ERROR) << "Effect Compile Error: "
                << cgGetErrorString(cgGetError()) << " : "
                << listing;
    return NULL;
  }

  if (listing && listing[0] != 0) {
    DLOG(WARNING) << "Effect Compile Warnings: " << listing;
  }

  cgGLLoadProgram(fragment_program);

  // Also check for GL errors, in case Cg managed to compile, but generated a
  // bad program.
  if (glGetError() != GL_NO_ERROR) {
    DLOG(ERROR) << "Effect GL Error: "
                << glGetString(GL_PROGRAM_ERROR_STRING_ARB);
    return false;
  }
  EffectGL *effect = new EffectGL(vertex_program, fragment_program);
  effect->Initialize();
  return effect;
}

int EffectGL::GetLowLevelParamIndexByName(const char *name) {
  DCHECK(name);
  for (unsigned int index = 0; index < low_level_params_.size(); ++index) {
    if (!strcmp(name, low_level_params_[index].name)) {
      return index;
    }
  }
  return -1;
}

void EffectGL::AddLowLevelParams(CGparameter cg_param,
                                 bool vp) {
  // Loop over all *leaf* parameters, visiting only CGparameters that have
  // had storage allocated to them.
  for (; cg_param != NULL; cg_param = cgGetNextLeafParameter(cg_param)) {
    CGenum variability = cgGetParameterVariability(cg_param);
    if (variability != CG_UNIFORM)
      continue;
    CGenum direction = cgGetParameterDirection(cg_param);
    if (direction != CG_IN)
      continue;
    const char *name = cgGetParameterName(cg_param);
    if (!name)
      continue;

    int index = GetLowLevelParamIndexByName(name);
    if (index < 0) {
      LowLevelParam param = {name, NULL, NULL, kInvalidResource};
      index = low_level_params_.size();
      low_level_params_.push_back(param);
      CGtype cg_type = cgGetParameterType(cg_param);
      if (cg_type == CG_SAMPLER ||
          cg_type == CG_SAMPLER1D ||
          cg_type == CG_SAMPLER2D ||
          cg_type == CG_SAMPLER3D ||
          cg_type == CG_SAMPLERCUBE) {
        sampler_params_.push_back(index);
      }
    }
    if (vp) {
      low_level_params_[index].vp_param = cg_param;
    } else {
      low_level_params_[index].fp_param = cg_param;
    }
  }
}

void EffectGL::Initialize() {
  AddLowLevelParams(cgGetFirstLeafParameter(vertex_program_, CG_PROGRAM), true);
  AddLowLevelParams(cgGetFirstLeafParameter(vertex_program_, CG_GLOBAL), true);
  AddLowLevelParams(cgGetFirstLeafParameter(fragment_program_, CG_PROGRAM),
                    false);
  AddLowLevelParams(cgGetFirstLeafParameter(fragment_program_, CG_GLOBAL),
                    false);
}

// Begins rendering with the effect, setting all the appropriate states.
bool EffectGL::Begin(GAPIGL *gapi) {
  cgGLBindProgram(vertex_program_);
  cgGLBindProgram(fragment_program_);
  // sampler->ApplyStates will mess with the texture binding on unit 0, so we
  // do 2 passes.
  // First to set the sampler states on the texture
  for (unsigned int i = 0; i < sampler_params_.size(); ++i) {
    unsigned int param_index = sampler_params_[i];
    ResourceID id = low_level_params_[param_index].sampler_id;
    if (id != kInvalidResource) {
      SamplerGL *sampler = gapi->GetSampler(id);
      if (!sampler->ApplyStates(gapi)) {
        return false;
      }
    }
  }
  // Second to enable/disable the sampler params.
  for (unsigned int i = 0; i < sampler_params_.size(); ++i) {
    unsigned int param_index = sampler_params_[i];
    const LowLevelParam &ll_param = low_level_params_[param_index];
    ResourceID id = ll_param.sampler_id;
    if (id != kInvalidResource) {
      SamplerGL *sampler = gapi->GetSampler(id);
      GLuint gl_texture = sampler->gl_texture();
      cgGLSetTextureParameter(ll_param.fp_param, gl_texture);
      cgGLEnableTextureParameter(ll_param.fp_param);
    } else {
      cgGLSetTextureParameter(ll_param.fp_param, 0);
      cgGLDisableTextureParameter(ll_param.fp_param);
    }
  }
  return true;
}

// Terminates rendering with the effect, resetting all the appropriate states.
void EffectGL::End(GAPIGL *gapi) {
}

// Gets the parameter count from the list.
unsigned int EffectGL::GetParamCount() const {
  return low_level_params_.size();
}

// Gets a handle to the selected parameter, and wraps it into an
// EffectParamGL if successful.
EffectParamGL *EffectGL::CreateParam(unsigned int index) {
  if (index < low_level_params_.size()) return NULL;
  return EffectParamGL::Create(this, index);
}

// Gets a handle to the selected parameter, and wraps it into an
// EffectParamGL if successful.
EffectParamGL *EffectGL::CreateParamByName(const char *name) {
  int index = GetLowLevelParamIndexByName(name);
  if (index < 0) return NULL;
  EffectParamGL::Create(this, index);
}

BufferSyncInterface::ParseError GAPIGL::CreateEffect(ResourceID id,
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
  EffectGL * effect = EffectGL::Create(this, effect_code,
                                       vertex_program_entry,
                                       fragment_program_entry);
  if (!effect) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  effects_.Assign(id, effect);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIGL::DestroyEffect(ResourceID id) {
  if (id == current_effect_id_) DirtyEffect();
  return effects_.Destroy(id) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

BufferSyncInterface::ParseError GAPIGL::SetEffect(ResourceID id) {
  DirtyEffect();
  current_effect_id_ = id;
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIGL::GetParamCount(ResourceID id,
                                                      unsigned int size,
                                                      void *data) {
  EffectGL *effect = effects_.Get(id);
  if (!effect || size < sizeof(Uint32))  // NOLINT
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  *static_cast<Uint32 *>(data) = effect->GetParamCount();
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIGL::CreateParam(ResourceID param_id,
                                                    ResourceID effect_id,
                                                    unsigned int index) {
  EffectGL *effect = effects_.Get(effect_id);
  if (!effect) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  EffectParamGL *param = effect->CreateParam(index);
  if (!param) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  effect_params_.Assign(param_id, param);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIGL::CreateParamByName(ResourceID param_id,
                                                          ResourceID effect_id,
                                                          unsigned int size,
                                                          const void *name) {
  EffectGL *effect = effects_.Get(effect_id);
  if (!effect) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  std::string string_name(static_cast<const char *>(name), size);
  EffectParamGL *param = effect->CreateParamByName(string_name.c_str());
  if (!param) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  effect_params_.Assign(param_id, param);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIGL::DestroyParam(ResourceID id) {
  return effect_params_.Destroy(id) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

BufferSyncInterface::ParseError GAPIGL::SetParamData(ResourceID id,
                                                     unsigned int size,
                                                     const void *data) {
  EffectParamGL *param = effect_params_.Get(id);
  if (!param) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return param->SetData(this, size, data) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

BufferSyncInterface::ParseError GAPIGL::GetParamDesc(ResourceID id,
                                                     unsigned int size,
                                                     void *data) {
  EffectParamGL *param = effect_params_.Get(id);
  if (!param) return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  return param->GetDesc(size, data) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

// If the current effect is valid, call End on it, and tag for revalidation.
void GAPIGL::DirtyEffect() {
  if (validate_effect_) return;
  DCHECK(current_effect_);
  current_effect_->End(this);
  current_effect_ = NULL;
  validate_effect_ = true;
}

// Gets the current effect, and calls Begin on it (if successful).
// Should only be called if the current effect is not valid.
bool GAPIGL::ValidateEffect() {
  DCHECK(validate_effect_);
  DCHECK(!current_effect_);
  current_effect_ = effects_.Get(current_effect_id_);
  if (!current_effect_) return false;
  validate_effect_ = false;
  return current_effect_->Begin(this);
}

}  // namespace command_buffer
}  // namespace o3d
