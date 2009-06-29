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


// This file contains the definition of EffectD3D9.

// TODO(gman): Most of the D3DXHANDLE lookup could be cached.

#include "core/cross/precompile.h"

#include "core/win/d3d9/effect_d3d9.h"

#include "base/scoped_ptr.h"
#include "core/cross/core_metrics.h"
#include "core/cross/error.h"
#include "core/cross/material.h"
#include "core/cross/renderer_platform.h"
#include "core/cross/semantic_manager.h"
#include "core/cross/standard_param.h"
#include "core/cross/transform.h"
#include "core/win/d3d9/d3d_entry_points.h"
#include "core/win/d3d9/primitive_d3d9.h"
#include "core/win/d3d9/sampler_d3d9.h"
#include "core/win/d3d9/texture_d3d9.h"
#include "core/win/d3d9/utils_d3d9.h"
#include "core/cross/param_array.h"

namespace o3d {

namespace {

inline bool IsSamplerType(D3DXPARAMETER_TYPE type) {
  return type == D3DXPT_SAMPLER ||
         type == D3DXPT_SAMPLER1D ||
         type == D3DXPT_SAMPLER2D ||
         type == D3DXPT_SAMPLER3D ||
         type == D3DXPT_SAMPLERCUBE;
}

}  // anonymous namespace

// A 'mostly' typesafe class to set an effect parameter from an O3D
// Param. The phandle must match the type of Param to be typesafe. That is
// handled when these are created.
template<typename ParamType, D3DXPARAMETER_CLASS d3d_parameter_class>
class TypedEffectParamHandlerD3D9 : public EffectParamHandlerD3D9 {
 public:
  TypedEffectParamHandlerD3D9(ParamType* param, D3DXHANDLE phandle)
      : param_(param),
        phandle_(phandle) {
  }
  virtual void SetEffectParam(RendererD3D9* renderer, ID3DXEffect* d3d_effect);
 private:
  ParamType* param_;
  D3DXHANDLE phandle_;
};

template <typename T>
class EffectParamArrayHandlerD3D9 : public EffectParamHandlerD3D9 {
 public:
  EffectParamArrayHandlerD3D9(ParamParamArray* param,
                              D3DXHANDLE phandle,
                              unsigned num_elements)
      : param_(param),
        phandle_(phandle),
        num_elements_(num_elements) {
  }
  virtual void SetEffectParam(RendererD3D9* renderer, ID3DXEffect* d3d_effect) {
    ParamArray* param = param_->value();
    if (param) {
      int size = param->size();
      if (size != num_elements_) {
        O3D_ERROR(param->service_locator())
            << "number of params in ParamArray does not match number of params "
            << "needed by shader array";
      } else {
        for (int i = 0; i < size; ++i) {
          Param* untyped_element = param->GetUntypedParam(i);
          // TODO(gman): Make this check happen when building the param cache.
          //    To do that would require that ParamParamArray mark it's owner
          //    as changed if a Param in it's ParamArray changes.
          if (untyped_element->IsA(T::GetApparentClass())) {
            D3DXHANDLE dx_element =
                d3d_effect->GetParameterElement(phandle_, i);
            SetElement(d3d_effect, dx_element, down_cast<T*>(untyped_element));
          } else {
            O3D_ERROR(param->service_locator())
                << "Param in ParamArray at index " << i << " is not a "
                << T::GetApparentClassName();
          }
        }
      }
    }
  }
  void SetElement(ID3DXEffect* d3dx_effect,
                  D3DXHANDLE dx_element,
                  T* element);

 private:
  ParamParamArray* param_;
  D3DXHANDLE phandle_;
  unsigned num_elements_;
};

// Number of h/w sampler units in the same shader using a single sampler.
// Eight should be enough!
static const int kMaxUnitsPerSampler = 8;

template <bool column_major>
class EffectParamMatrix4ArrayHandlerD3D9 : public EffectParamHandlerD3D9 {
 public:
  EffectParamMatrix4ArrayHandlerD3D9(ParamParamArray* param,
                                     D3DXHANDLE phandle,
                                     unsigned num_elements)
      : param_(param),
        phandle_(phandle),
        num_elements_(num_elements) {
  }
  virtual void SetEffectParam(RendererD3D9* renderer, ID3DXEffect* d3d_effect) {
    ParamArray* param = param_->value();
    if (param) {
      int size = param->size();
      if (size != num_elements_) {
        O3D_ERROR(param->service_locator())
            << "number of params in ParamArray does not match number of params "
            << "needed by shader array";
      } else {
        for (int i = 0; i < size; ++i) {
          Param* untyped_element = param->GetUntypedParam(i);
          // TODO(gman): Make this check happen when building the param cache.
          //    To do that would require that ParamParamArray mark it's owner
          //    as changed if a Param in it's ParamArray changes.
          if (untyped_element->IsA(ParamMatrix4::GetApparentClass())) {
            D3DXHANDLE dx_element =
                d3d_effect->GetParameterElement(phandle_, i);
            SetElement(d3d_effect,
                       dx_element,
                       down_cast<ParamMatrix4*>(untyped_element));
          } else {
            O3D_ERROR(param->service_locator())
                << "Param in ParamArray at index " << i << " is not a "
                << ParamMatrix4::GetApparentClassName();
          }
        }
      }
    }
  }
  void SetElement(ID3DXEffect* d3dx_effect,
                  D3DXHANDLE dx_element,
                  ParamMatrix4* element);

 private:
  ParamParamArray* param_;
  D3DXHANDLE phandle_;
  unsigned num_elements_;
};

// A class for setting the the appropriate d3d sampler states from an array of
// o3d Sampler object.
class EffectParamSamplerArrayHandlerD3D9 : public EffectParamHandlerD3D9 {
 public:
  EffectParamSamplerArrayHandlerD3D9(ParamParamArray* param,
                                     D3DXHANDLE phandle,
                                     const D3DXPARAMETER_DESC& pdesc,
                                     LPD3DXCONSTANTTABLE fs_constant_table,
                                     LPDIRECT3DDEVICE9 d3d_device)
      : param_(param),
        phandle_(phandle),
        sampler_unit_index_arrays_(pdesc.Elements) {
    if (!fs_constant_table) {
      DLOG(ERROR) << "Fragment shader constant table is NULL";
      return;
    }
    D3DXHANDLE sampler_array_handle = fs_constant_table->GetConstantByName(
        NULL,
        pdesc.Name);
    if (!sampler_array_handle) {
      DLOG(ERROR) << "Sampler " << pdesc.Name <<
          " not found in fragment shader";
      return;
    }
    for (unsigned ii = 0; ii < pdesc.Elements; ++ii) {
      D3DXHANDLE sampler_handle = fs_constant_table->GetConstantElement(
          sampler_array_handle,
          ii);
      if (!sampler_handle) {
        DLOG(ERROR) << "Sampler " << pdesc.Name << " index " << ii
                    << " not found in fragment shader";
      } else {
        D3DXCONSTANT_DESC desc_array[kMaxUnitsPerSampler];
        UINT num_desc = kMaxUnitsPerSampler;
        fs_constant_table->GetConstantDesc(
            sampler_handle, desc_array, &num_desc);
        // We have no good way of querying how many descriptions would really be
        // returned as we're capping the number to kMaxUnitsPerSampler (which
        // should be more than sufficient).  If however we do end up with the
        // max number there's a chance that there were actually more so let's
        // log it.
        if (num_desc == kMaxUnitsPerSampler) {
          DLOG(WARNING) << "Number of constant descriptions might have "
                        << "exceeded the maximum of " << kMaxUnitsPerSampler;
        }
        SamplerUnitIndexArray& index_array = sampler_unit_index_arrays_[ii];

        for (UINT desc_index = 0; desc_index < num_desc; desc_index++) {
          D3DXCONSTANT_DESC constant_desc = desc_array[desc_index];
          if (constant_desc.Class == D3DXPC_OBJECT &&
              IsSamplerType(constant_desc.Type)) {
            index_array.push_back(constant_desc.RegisterIndex);
          }
        }
        if (index_array.empty()) {
          DLOG(ERROR) << "No matching sampler units found for " <<
              pdesc.Name;
        }
      }
    }
  }

  virtual void SetEffectParam(RendererD3D9* renderer, ID3DXEffect* d3d_effect) {
    ParamArray* param = param_->value();
    if (param) {
      unsigned size = param->size();
      if (size != sampler_unit_index_arrays_.size()) {
        O3D_ERROR(param->service_locator())
            << "number of params in ParamArray does not match number of params "
            << "needed by shader array";
      } else {
        for (int i = 0; i < size; ++i) {
          SamplerUnitIndexArray& index_array = sampler_unit_index_arrays_[i];
          Param* untyped_element = param->GetUntypedParam(i);
          // TODO(gman): Make this check happen when building the param cache.
          //    To do that would require that ParamParamArray mark it's owner
          //    as changed if a Param in it's ParamArray changes.
          if (untyped_element->IsA(ParamSampler::GetApparentClass())) {
            D3DXHANDLE dx_element =
                d3d_effect->GetParameterElement(phandle_,  i);
            // Find the texture associated with the sampler first.
            Sampler* sampler =
                down_cast<ParamSampler*>(untyped_element)->value();
            if (!sampler) {
              sampler = renderer->error_sampler();
              if (!renderer->error_texture()) {
                O3D_ERROR(param->service_locator())
                    << "Missing Sampler for ParamSampler "
                    << param->name();
              }
            }

            SamplerD3D9* d3d_sampler = down_cast<SamplerD3D9*>(sampler);
            for (unsigned stage = 0; stage < index_array.size(); stage++) {
              d3d_sampler->SetTextureAndStates(index_array[stage]);
            }
          } else {
            O3D_ERROR(param->service_locator())
                << "Param in ParamArray at index " << i << " is not a "
                << ParamSampler::GetApparentClassName();
          }
        }
      }
    }
  }

  // Resets the value of the parameter to default.  Currently this is used
  // to unbind textures contained in Sampler params.
  virtual void ResetEffectParam(RendererD3D9* renderer,
                                ID3DXEffect* d3d_effect) {
    ParamArray* param = param_->value();
    if (param) {
      unsigned size = param->size();
      if (size == sampler_unit_index_arrays_.size()) {
        for (int i = 0; i < size; ++i) {
          SamplerUnitIndexArray& index_array = sampler_unit_index_arrays_[i];
          Param* untyped_element = param->GetUntypedParam(i);
          // TODO(gman): Make this check happen when building the param cache.
          //    To do that would require that ParamParamArray mark it's owner
          //    as changed if a Param in it's ParamArray changes.
          if (untyped_element->IsA(ParamSampler::GetApparentClass())) {
            D3DXHANDLE dx_element =
                d3d_effect->GetParameterElement(phandle_,  i);
            // Find the texture associated with the sampler first.
            Sampler* sampler =
                down_cast<ParamSampler*>(untyped_element)->value();
            if (!sampler) {
              sampler = renderer->error_sampler();
            }

            SamplerD3D9* d3d_sampler = down_cast<SamplerD3D9*>(sampler);
            for (unsigned stage = 0; stage < index_array.size(); stage++) {
              d3d_sampler->ResetTexture(index_array[stage]);
            }
          }
        }
      }
    }
  }

 private:
  typedef std::vector<int> SamplerUnitIndexArray;
  typedef std::vector<SamplerUnitIndexArray> SamplerIndexArrayArray;

  ParamParamArray* param_;
  D3DXHANDLE phandle_;
  // An array of arrays of sampler unit indices.
  SamplerIndexArrayArray sampler_unit_index_arrays_;
};

template<>
void EffectParamArrayHandlerD3D9<ParamFloat>::SetElement(
    ID3DXEffect* d3dx_effect,
    D3DXHANDLE dx_element,
    ParamFloat* element) {
  d3dx_effect->SetFloat(dx_element, element->value());
}

template<>
void EffectParamArrayHandlerD3D9<ParamFloat2>::SetElement(
    ID3DXEffect* d3dx_effect,
    D3DXHANDLE dx_element,
    ParamFloat2* element) {
  Float2 float2 = element->value();
  HR(d3dx_effect->SetFloatArray(dx_element, float2.GetFloatArray(), 2));
}

template<>
void EffectParamArrayHandlerD3D9<ParamFloat3>::SetElement(
    ID3DXEffect* d3dx_effect,
    D3DXHANDLE dx_element,
    ParamFloat3* element) {
  Float3 float3 = element->value();
  HR(d3dx_effect->SetFloatArray(dx_element, float3.GetFloatArray(), 3));
}

template<>
void EffectParamArrayHandlerD3D9<ParamFloat4>::SetElement(
    ID3DXEffect* d3dx_effect,
    D3DXHANDLE dx_element,
    ParamFloat4* element) {
  Float4 float4 = element->value();
  HR(d3dx_effect->SetFloatArray(dx_element, float4.GetFloatArray(), 4));
}

template<>
void EffectParamArrayHandlerD3D9<ParamBoolean>::SetElement(
    ID3DXEffect* d3dx_effect,
    D3DXHANDLE dx_element,
    ParamBoolean* element) {
  HR(d3dx_effect->SetBool(dx_element, element->value()));
}

template<>
void EffectParamArrayHandlerD3D9<ParamInteger>::SetElement(
    ID3DXEffect* d3dx_effect,
    D3DXHANDLE dx_element,
    ParamInteger* element) {
  HR(d3dx_effect->SetInt(dx_element, element->value()));
}

template<>
void EffectParamMatrix4ArrayHandlerD3D9<false>::SetElement(
    ID3DXEffect* d3dx_effect,
    D3DXHANDLE dx_element,
    ParamMatrix4* element) {
  Matrix4 param_matrix = element->value();
  HR(d3dx_effect->SetMatrix(
         dx_element,
         reinterpret_cast<D3DXMATRIX*>(&param_matrix[0][0])));
}

template<>
void EffectParamMatrix4ArrayHandlerD3D9<true>::SetElement(
    ID3DXEffect* d3dx_effect,
    D3DXHANDLE dx_element,
    ParamMatrix4* element) {
  Matrix4 param_matrix = transpose(element->value());
  HR(d3dx_effect->SetMatrix(
         dx_element,
         reinterpret_cast<D3DXMATRIX*>(&param_matrix[0][0])));
}

// A class for setting the the appropriate d3d sampler states from a
// o3d Sampler object.
class EffectParamHandlerForSamplersD3D9 : public EffectParamHandlerD3D9 {
 public:
  EffectParamHandlerForSamplersD3D9(ParamSampler* sampler_param,
                                    const D3DXPARAMETER_DESC& pdesc,
                                    LPD3DXCONSTANTTABLE fs_constant_table,
                                    LPDIRECT3DDEVICE9 d3d_device);

  virtual void SetEffectParam(RendererD3D9* renderer, ID3DXEffect* d3d_effect);

  // Resets the value of the parameter to default.  Currently this is used
  // to unbind textures contained in Sampler params.
  virtual void ResetEffectParam(RendererD3D9* renderer,
                                ID3DXEffect* d3d_effect);
 private:
  ParamSampler* sampler_param_;
  // The number of sampler units using this sampler parameter.
  int number_sampler_units_;
  // An array of sampler unit indices.
  int sampler_unit_index_array_[kMaxUnitsPerSampler];
};

// Converts a given D3DX parameter description to an O3D Param type,
// or Param::INVALID if no corresponding type is found.
static const ObjectBase::Class* D3DXPDescToParamType(
    const D3DXPARAMETER_DESC& pdesc) {
  // Matrix4 Param
  if (pdesc.Type == D3DXPT_FLOAT &&
      pdesc.Columns == 4 &&
      pdesc.Rows == 4) {
    return ParamMatrix4::GetApparentClass();
  // Float Param
  } else if (pdesc.Type == D3DXPT_FLOAT &&
             pdesc.Class == D3DXPC_SCALAR) {
    return ParamFloat::GetApparentClass();
  // Float2 Param
  } else if (pdesc.Type == D3DXPT_FLOAT &&
             pdesc.Class == D3DXPC_VECTOR) {
    if (pdesc.Columns == 1) {
      return ParamFloat::GetApparentClass();
    } else if (pdesc.Columns == 2) {
      return ParamFloat2::GetApparentClass();
    } else if (pdesc.Columns == 3) {
      return ParamFloat3::GetApparentClass();
    } else if (pdesc.Columns == 4) {
      return ParamFloat4::GetApparentClass();
    }
  // Integer param
  } else if (pdesc.Type == D3DXPT_INT &&
             pdesc.Class == D3DXPC_SCALAR &&
             pdesc.Columns == 1) {
    return ParamInteger::GetApparentClass();
  // Boolean param
  } else if (pdesc.Type == D3DXPT_BOOL &&
             pdesc.Class == D3DXPC_SCALAR &&
             pdesc.Columns == 1) {
    return ParamBoolean::GetApparentClass();
  // Texture param
  // TODO(o3d): Texture params should be removed once we switch over to
  // using samplers only.
  } else if (pdesc.Type == D3DXPT_TEXTURE &&
             pdesc.Class == D3DXPC_OBJECT) {
    return ParamTexture::GetApparentClass();
  // Sampler param
  } else if (pdesc.Class == D3DXPC_OBJECT && IsSamplerType(pdesc.Type)) {
    return ParamSampler::GetApparentClass();
  }
  return NULL;
}

EffectD3D9::EffectD3D9(ServiceLocator* service_locator,
                       IDirect3DDevice9* d3d_device)
    : Effect(service_locator),
      semantic_manager_(service_locator->GetService<SemanticManager>()),
      renderer_(static_cast<RendererD3D9*>(
          service_locator->GetService<Renderer>())),
      d3d_device_(d3d_device) {
  DCHECK(d3d_device);
}

EffectD3D9::~EffectD3D9() {
}

bool EffectD3D9::PrepareFX(const String& effect,
                           String* prepared_effect) {
  String vertex_shader_entry_point;
  String fragment_shader_entry_point;
  MatrixLoadOrder matrix_load_order;

  // TODO(o3d): Temporary fix to make GL and D3D match until the shader parser
  //     is written.
  if (!ValidateFX(effect,
                  &vertex_shader_entry_point,
                  &fragment_shader_entry_point,
                  &matrix_load_order)) {
    // TODO(o3d): Remove this but for now just let bad ones pass so collada
    //     importer works.
    *prepared_effect = effect;
    return false;
  }

  *prepared_effect = effect +
      "technique Shaders { "
      "  pass p0 { "
      "    VertexShader = compile vs_2_0 " + vertex_shader_entry_point + "();"
      "    PixelShader = compile ps_2_0 " + fragment_shader_entry_point + "();"
      "  }"
      "};";

  set_matrix_load_order(matrix_load_order);

  return true;
}

void EffectD3D9::ClearD3D9Effect() {
  set_source("");
  d3d_vertex_shader_ = NULL;
  d3d_fragment_shader_ = NULL;
  fs_constant_table_ = NULL;
  d3dx_effect_ = NULL;
}

// Initializes the Effect object using the shaders found in an FX formatted
// string.
bool EffectD3D9::LoadFromFXString(const String& effect) {
  ClearD3D9Effect();

  LPD3DXBUFFER error_buffer;

  String prepared_effect;
  // TODO(o3d): Check for failure once shader parser is in.
  PrepareFX(effect, &prepared_effect);

  if (!HR(o3d::D3DXCreateEffect(d3d_device_,
                                prepared_effect.c_str(),
                                (UINT)prepared_effect.size(),
                                NULL,
                                NULL,
                                D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY,
                                NULL,
                                &d3dx_effect_,
                                &error_buffer))) {
    DisplayFXError("FX from String", error_buffer);
    return false;
  }
  if (!InitializeFX()) {
    return false;
  }

  // Get metrics for the length of the shader
  UINT data_size = 0;
  if (d3d_vertex_shader_) {
    d3d_vertex_shader_->GetFunction(NULL, &data_size);
    metric_vertex_shader_instruction_count.AddSample(data_size);
  }
  if (d3d_fragment_shader_) {
    d3d_fragment_shader_->GetFunction(NULL, &data_size);
    metric_pixel_shader_instruction_count.AddSample(data_size);
  }

  set_source(effect);
  return true;
}

// Parses a DX9 error buffer and displays a nicely formatted error string
// in a MessageBox.
void EffectD3D9::DisplayFXError(const String &header,
    LPD3DXBUFFER error_buffer) {
  String compile_errors_string;
  if (error_buffer) {
    LPVOID compile_errors = error_buffer->GetBufferPointer();
    compile_errors_string = (reinterpret_cast<char*>(compile_errors));
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
    std::wstring msg_utf16(msg);
    String msg_utf8 = WideToUTF8(msg_utf16);

    compile_errors_string = header + ":  " + msg_utf8.c_str();
    LocalFree(hLocal);
  }
  O3D_ERROR(service_locator()) << "Effect Compile Error::"
                               << compile_errors_string.c_str();
}

// Creates the vertex and fragment shaders based on the programs found in the
// DX9 effect.
bool EffectD3D9::InitializeFX() {
  bool rc = false;

  // we only handle the first technique, for now
  D3DXHANDLE technique = d3dx_effect_->GetTechnique(0);
  if (!technique) {
    DLOG(ERROR) << "Failed to get technique";
    return false;
  }

  D3DXTECHNIQUE_DESC desc;
  if (!HR(d3dx_effect_->GetTechniqueDesc(technique, &desc))) {
    DLOG(ERROR) << "Failed to get technique description";
    return false;
  }

  if (desc.Passes != 1) {
    O3D_ERROR(service_locator())
        << "Effect Compile Error: "
        << "Multi-pass shaders are unsupported.";
    return false;
  }

  D3DXHANDLE pass = d3dx_effect_->GetPass(technique, 0);
  if (!pass) {
    DLOG(ERROR) << "Failed to get pass";
    return false;
  }

  D3DXPASS_DESC pass_desc;
  if (!HR(d3dx_effect_->GetPassDesc(pass, &pass_desc))) {
    DLOG(ERROR) << "Failed to get pass description";
    return false;
  }

  if (!HR(d3d_device_->CreateVertexShader(
      pass_desc.pVertexShaderFunction, &d3d_vertex_shader_))) {
    DLOG(ERROR) << "Failed to create vertex shader";
    return false;
  }

  if (!HR(d3d_device_->CreatePixelShader(
      pass_desc.pPixelShaderFunction, &d3d_fragment_shader_))) {
    DLOG(ERROR) << "Failed to create pixel shader";
    return false;
  }

  // Get the Fragment Shader constant table.
  if (!HR(o3d::D3DXGetShaderConstantTable(
      pass_desc.pPixelShaderFunction,
      &fs_constant_table_))) {
    DLOG(ERROR) << "Failed to get fragment shader constant table";
    return false;
  }

  return true;
}

// Adds a Parameter Mapping from an O3D param to a d3d parameter if they
// match in type.
// Parameters:
//   param: Param we are attempting to map.
//   pdesc: d3d parameter description.
//   phandle: Handle to d3d parameter.
//   effect_param_cache: Cache to add mapping to.
// Returns:
//   true if a mapping was added.
bool EffectD3D9::AddParameterMapping(
    Param* param,
    const D3DXPARAMETER_DESC& pdesc,
    D3DXHANDLE phandle,
    EffectParamHandlerCacheD3D9* effect_param_cache) {
  // Array param
  if (param->IsA(ParamParamArray::GetApparentClass()) && pdesc.Elements > 0) {
    ParamParamArray* param_param_array = down_cast<ParamParamArray*>(param);
    if (pdesc.Class == D3DXPC_SCALAR &&
        pdesc.Type == D3DXPT_FLOAT) {
      effect_param_cache->AddElement(
          new EffectParamArrayHandlerD3D9<ParamFloat>(
              param_param_array, phandle, pdesc.Elements));
    } else if (pdesc.Class == D3DXPC_VECTOR &&
               pdesc.Type == D3DXPT_FLOAT &&
               pdesc.Columns == 2) {
      effect_param_cache->AddElement(
          new EffectParamArrayHandlerD3D9<ParamFloat2>(
              param_param_array, phandle, pdesc.Elements));
    } else if (pdesc.Class == D3DXPC_VECTOR &&
               pdesc.Type == D3DXPT_FLOAT &&
               pdesc.Columns == 3) {
      effect_param_cache->AddElement(
          new EffectParamArrayHandlerD3D9<ParamFloat3>(
              param_param_array, phandle, pdesc.Elements));
    } else if (pdesc.Class == D3DXPC_VECTOR &&
               pdesc.Type == D3DXPT_FLOAT &&
               pdesc.Columns == 4) {
      effect_param_cache->AddElement(
          new EffectParamArrayHandlerD3D9<ParamFloat4>(
              param_param_array, phandle, pdesc.Elements));
    } else if (pdesc.Class == D3DXPC_SCALAR &&
               pdesc.Type == D3DXPT_INT &&
               pdesc.Columns == 1) {
      effect_param_cache->AddElement(
          new EffectParamArrayHandlerD3D9<ParamInteger>(
              param_param_array, phandle, pdesc.Elements));
    } else if (pdesc.Class == D3DXPC_SCALAR &&
               pdesc.Type == D3DXPT_BOOL &&
               pdesc.Columns == 1) {
      effect_param_cache->AddElement(
          new EffectParamArrayHandlerD3D9<ParamBoolean>(
              param_param_array, phandle, pdesc.Elements));
    } else if (pdesc.Class == D3DXPC_MATRIX_COLUMNS) {
      effect_param_cache->AddElement(
          new EffectParamMatrix4ArrayHandlerD3D9<true>(
              param_param_array, phandle, pdesc.Elements));
    } else if (pdesc.Class == D3DXPC_MATRIX_ROWS) {
      if (matrix_load_order() == COLUMN_MAJOR) {
        // D3D has already created a uniform of type MATRIX_ROWS, but the
        // effect wants column major matrices, so we create a handler
        // for MATRIX_COLUMNS.  This will cause the matrix to be transposed
        // on load.
        effect_param_cache->AddElement(
            new EffectParamMatrix4ArrayHandlerD3D9<true>(
                param_param_array, phandle, pdesc.Elements));
      } else {
        effect_param_cache->AddElement(
            new EffectParamMatrix4ArrayHandlerD3D9<false>(
                param_param_array, phandle, pdesc.Elements));
      }
    } else if (pdesc.Class == D3DXPC_OBJECT && IsSamplerType(pdesc.Type)) {
      effect_param_cache->AddElement(
          new EffectParamSamplerArrayHandlerD3D9(
             param_param_array,
             phandle,
             pdesc,
             fs_constant_table_,
             d3d_device_));
    }
  // Matrix4 Param
  } else if (param->IsA(ParamMatrix4::GetApparentClass()) &&
             pdesc.Class == D3DXPC_MATRIX_COLUMNS) {
    effect_param_cache->AddElement(
        new TypedEffectParamHandlerD3D9<ParamMatrix4,
                                        D3DXPC_MATRIX_COLUMNS>(
            down_cast<ParamMatrix4*>(param), phandle));
  } else if (param->IsA(ParamMatrix4::GetApparentClass()) &&
             pdesc.Class == D3DXPC_MATRIX_ROWS) {
    if (matrix_load_order() == COLUMN_MAJOR) {
      // D3D has already created a uniform of type MATRIX_ROWS, but the
      // effect wants column major matrices, so we create a handler
      // for MATRIX_COLUMNS.  This will cause the matrix to be transposed
      // on load.
      effect_param_cache->AddElement(
          new TypedEffectParamHandlerD3D9<ParamMatrix4,
                                          D3DXPC_MATRIX_COLUMNS>(
              down_cast<ParamMatrix4*>(param), phandle));
    } else {
      effect_param_cache->AddElement(
          new TypedEffectParamHandlerD3D9<ParamMatrix4,
                                          D3DXPC_MATRIX_ROWS>(
              down_cast<ParamMatrix4*>(param), phandle));
    }
  // Float Param
  } else if (param->IsA(ParamFloat::GetApparentClass()) &&
             pdesc.Class == D3DXPC_SCALAR &&
             pdesc.Type == D3DXPT_FLOAT) {
    effect_param_cache->AddElement(
        new TypedEffectParamHandlerD3D9<ParamFloat,
                                        D3DXPC_SCALAR>(
            down_cast<ParamFloat*>(param), phandle));
  // Float2 Param
  } else if (param->IsA(ParamFloat2::GetApparentClass()) &&
             pdesc.Class == D3DXPC_VECTOR &&
             pdesc.Type == D3DXPT_FLOAT &&
             pdesc.Columns == 2) {
    effect_param_cache->AddElement(
        new TypedEffectParamHandlerD3D9<ParamFloat2,
                                        D3DXPC_VECTOR>(
            down_cast<ParamFloat2*>(param), phandle));
  // Float3 Param
  } else if (param->IsA(ParamFloat3::GetApparentClass()) &&
             pdesc.Class == D3DXPC_VECTOR &&
             pdesc.Type == D3DXPT_FLOAT &&
             pdesc.Columns == 3) {
    effect_param_cache->AddElement(
        new TypedEffectParamHandlerD3D9<ParamFloat3,
                                        D3DXPC_VECTOR>(
            down_cast<ParamFloat3*>(param), phandle));
  // Float4 Param
  } else if (param->IsA(ParamFloat4::GetApparentClass()) &&
             pdesc.Class == D3DXPC_VECTOR &&
             pdesc.Type == D3DXPT_FLOAT &&
             pdesc.Columns == 4) {
    effect_param_cache->AddElement(
        new TypedEffectParamHandlerD3D9<ParamFloat4,
                                        D3DXPC_VECTOR>(
            down_cast<ParamFloat4*>(param), phandle));
  // Integer param
  } else if (param->IsA(ParamInteger::GetApparentClass()) &&
             pdesc.Class == D3DXPC_SCALAR &&
             pdesc.Type == D3DXPT_INT &&
             pdesc.Columns == 1) {
    effect_param_cache->AddElement(
        new TypedEffectParamHandlerD3D9<ParamInteger,
                                        D3DXPC_SCALAR>(
            down_cast<ParamInteger*>(param), phandle));
  // Boolean param
  } else if (param->IsA(ParamBoolean::GetApparentClass()) &&
             pdesc.Class == D3DXPC_SCALAR &&
             pdesc.Type == D3DXPT_BOOL &&
             pdesc.Columns == 1) {
    effect_param_cache->AddElement(
        new TypedEffectParamHandlerD3D9<ParamBoolean,
                                        D3DXPC_SCALAR>(
            down_cast<ParamBoolean*>(param), phandle));
    // Texture param
    // TODO(o3d): The texture param block should be removed once we start
    // using samplers only.  In the meantime, we need to create a texture param
    // to be able to handle collada files referencing external fx .
  } else if (param->IsA(ParamTexture::GetApparentClass()) &&
             pdesc.Class == D3DXPC_OBJECT &&
             pdesc.Type == D3DXPT_TEXTURE) {
    effect_param_cache->AddElement(
        new TypedEffectParamHandlerD3D9<ParamTexture,
                                        D3DXPC_OBJECT>(
            down_cast<ParamTexture*>(param), phandle));
  // Sampler param
  } else if (param->IsA(ParamSampler::GetApparentClass()) &&
             pdesc.Class == D3DXPC_OBJECT && IsSamplerType(pdesc.Type)) {
    effect_param_cache->AddElement(
        new EffectParamHandlerForSamplersD3D9(down_cast<ParamSampler*>(param),
                                              pdesc,
                                              fs_constant_table_,
                                              d3d_device_));
  } else {
    return false;
  }
  return true;
}

// Loops through all the parameters in the d3dx effect and tries to
// find matches (by name and type) first in the Transform (param_object1)
// DrawPrimitive Params (param_object2), then in the Primitive Param
// (param_object3) then in the Material params and finally in the Effect params.
// If there exists a Param with the same name as the d3dx effect parameter and a
// compatible type then a handler is created to update the d3d parameter with
// the Param.
void EffectD3D9::UpdateParameterMappings(
    const std::vector<ParamObject*>& param_object_list,
    EffectParamHandlerCacheD3D9* effect_param_cache) {
  // Clear the old ones.
  effect_param_cache->Clear();
  if (d3dx_effect_) {
    // Update all the parameter handles from the effect desc
    D3DXEFFECT_DESC desc;
    d3dx_effect_->GetDesc(&desc);
    for (UINT i = 0; i < desc.Parameters; ++i) {
      D3DXPARAMETER_DESC pdesc;
      D3DXHANDLE phandle = d3dx_effect_->GetParameter(NULL, i);
      d3dx_effect_->GetParameterDesc(phandle, &pdesc);
      Param *param;
      String constant_name(pdesc.Name);
      const ObjectBase::Class* sem_type = NULL;
      if (pdesc.Semantic) {
        sem_type = semantic_manager_->LookupSemantic(pdesc.Semantic);
      }
      bool mapped = false;
      for (unsigned ii = 0; !mapped && ii < param_object_list.size(); ++ii) {
        ParamObject* param_object = param_object_list[ii];
        param = param_object->GetUntypedParam(constant_name);
        if (!param && sem_type) {
          param = param_object->GetUntypedParam(sem_type->name());
        }
        if (param) {
          mapped = AddParameterMapping(param,
                                       pdesc,
                                       phandle,
                                       effect_param_cache);
        }
      }

      // If it's still not mapped attempt to map it to the error sampler param.
      // It will fail if it's not a sampler.
      if (!mapped) {
        param = renderer_->error_param_sampler();
        mapped = AddParameterMapping(param,
                                     pdesc,
                                     phandle,
                                     effect_param_cache);
      }
    }
  }
}

void EffectD3D9::GetParameterInfo(EffectParameterInfoArray* info_array) {
  DCHECK(info_array);
  info_array->clear();
  if (d3dx_effect_) {
    // Add parameters to the Shape for all parameters in the effect
    D3DXEFFECT_DESC desc;
    d3dx_effect_->GetDesc(&desc);
    for (UINT i = 0; i < desc.Parameters; ++i) {
      D3DXPARAMETER_DESC pdesc;
      D3DXHANDLE phandle = d3dx_effect_->GetParameter(NULL, i);
      const ObjectBase::Class* sas_class_type = NULL;
      if (d3dx_effect_->GetParameterDesc(phandle, &pdesc) == S_OK) {
        const ObjectBase::Class* type = D3DXPDescToParamType(pdesc);
        if (type != NULL) {
          if (pdesc.Semantic != NULL &&
              type == ParamMatrix4::GetApparentClass()) {
            sas_class_type = semantic_manager_->LookupSemantic(pdesc.Semantic);
          }
          String semantic((pdesc.Semantic != NULL) ? pdesc.Semantic : "");
          info_array->push_back(EffectParameterInfo(
              pdesc.Name,
              type,
              pdesc.Elements,
              semantic,
              sas_class_type));
        }
      }
    }
  }
}

void EffectD3D9::GetStreamInfo(EffectStreamInfoArray* info_array) {
  DCHECK(info_array);
  info_array->clear();
  if (d3d_vertex_shader_) {
    UINT size;
    d3d_vertex_shader_->GetFunction(NULL, &size);
    scoped_array<DWORD> function(new DWORD[size]);
    d3d_vertex_shader_->GetFunction(function.get(), &size);

    UINT num_semantics;
    HR(o3d::D3DXGetShaderInputSemantics(function.get(),
                                        NULL,
                                        &num_semantics));
    scoped_array<D3DXSEMANTIC> semantics(new D3DXSEMANTIC[num_semantics]);
    HR(o3d::D3DXGetShaderInputSemantics(function.get(),
                                        semantics.get(),
                                        &num_semantics));

    info_array->resize(num_semantics);
    for (UINT i = 0; i < num_semantics; ++i) {
      (*info_array)[i] = EffectStreamInfo(
          SemanticFromDX9UsageType(static_cast<D3DDECLUSAGE>(
              semantics[i].Usage)),
          static_cast<int>(semantics[i].UsageIndex));
    }
  }
}

template<>
void TypedEffectParamHandlerD3D9<ParamMatrix4,
                                 D3DXPC_MATRIX_COLUMNS>::SetEffectParam(
                                     RendererD3D9* renderer,
                                     ID3DXEffect* d3dx_effect) {
  Matrix4 param_matrix = transpose(param_->value());
  HR(d3dx_effect->SetMatrix(
         phandle_,
         reinterpret_cast<D3DXMATRIX*>(&param_matrix[0][0])));
}

template<>
void TypedEffectParamHandlerD3D9<ParamMatrix4,
                                 D3DXPC_MATRIX_ROWS>::SetEffectParam(
                                     RendererD3D9* renderer,
                                     ID3DXEffect* d3dx_effect) {
  Matrix4 param_matrix = param_->value();
  HR(d3dx_effect->SetMatrix(
         phandle_,
         reinterpret_cast<D3DXMATRIX*>(&param_matrix[0][0])));
}

template<>
void TypedEffectParamHandlerD3D9<ParamFloat,
                                 D3DXPC_SCALAR>::SetEffectParam(
                                     RendererD3D9* renderer,
                                     ID3DXEffect* d3dx_effect) {
  HR(d3dx_effect->SetFloat(phandle_, param_->value()));
}

template<>
void TypedEffectParamHandlerD3D9<ParamFloat2,
                                 D3DXPC_VECTOR>::SetEffectParam(
                                     RendererD3D9* renderer,
                                     ID3DXEffect* d3dx_effect) {
  Float2 param_float2 = param_->value();
  HR(d3dx_effect->SetFloatArray(
         phandle_,
         param_float2.GetFloatArray(),
         2));
}

template<>
void TypedEffectParamHandlerD3D9<ParamFloat3,
                                 D3DXPC_VECTOR>::SetEffectParam(
                                     RendererD3D9* renderer,
                                     ID3DXEffect* d3dx_effect) {
  Float3 param_float3 = param_->value();
  HR(d3dx_effect->SetFloatArray(
         phandle_,
         param_float3.GetFloatArray(),
         3));
}

template<>
void TypedEffectParamHandlerD3D9<ParamFloat4,
                                 D3DXPC_VECTOR>::SetEffectParam(
                                     RendererD3D9* renderer,
                                     ID3DXEffect* d3dx_effect) {
  Float4 param_float4 = param_->value();
  HR(d3dx_effect->SetFloatArray(
         phandle_,
         param_float4.GetFloatArray(),
         4));
}

template<>
void TypedEffectParamHandlerD3D9<ParamInteger,
                                 D3DXPC_SCALAR>::SetEffectParam(
                                     RendererD3D9* renderer,
                                     ID3DXEffect* d3dx_effect) {
  HR(d3dx_effect->SetInt(phandle_, param_->value()));
}

template<>
void TypedEffectParamHandlerD3D9<ParamBoolean,
                                 D3DXPC_SCALAR>::SetEffectParam(
                                     RendererD3D9* renderer,
                                     ID3DXEffect* d3dx_effect) {
  HR(d3dx_effect->SetBool(phandle_, param_->value()));
}

// TODO(o3d): The following handler should be removed once we switch to
// using Samplers exclusively.
template<>
void TypedEffectParamHandlerD3D9<ParamTexture,
                                 D3DXPC_OBJECT>::SetEffectParam(
                                     RendererD3D9* renderer,
                                     ID3DXEffect* d3dx_effect) {
  Texture* texture = param_->value();
  // TODO(o3d): If texture is NULL then we don't set the texture on the
  // effect to avoid clobbering texture set by the corresponding sampler in
  // the cases where we use samplers.  The side-effect of this is that if
  // the texture is not set, we could end up using whatever texture was used
  // by the unit before (instead of black).  This handler will be removed
  // once we add support for ColladaFX and samplers so it should be ok.
  if (texture != NULL) {
    IDirect3DBaseTexture9 *d3d_texture = NULL;
    if (!renderer->SafeToBindTexture(texture)) {
      O3D_ERROR(renderer->service_locator())
          << "Attempt to bind texture, " << texture->name() << " when drawing "
          << "to an owned RenderSurface";
      d3d_texture = static_cast<IDirect3DBaseTexture9*>(
          renderer->error_texture()->GetTextureHandle());
    } else {
      d3d_texture =
          static_cast<IDirect3DBaseTexture9*>(texture->GetTextureHandle());
    }
    HR(d3dx_effect->SetTexture(phandle_, d3d_texture));
  }
}

void EffectParamHandlerForSamplersD3D9::SetEffectParam(
    RendererD3D9* renderer,
    ID3DXEffect* d3dx_effect) {
  // Find the texture associated with the sampler first.
  Sampler* sampler = sampler_param_->value();
  if (!sampler) {
    sampler = renderer->error_sampler();
    if (!renderer->error_texture()) {
      O3D_ERROR(sampler_param_->service_locator())
          << "Missing Sampler for ParamSampler " << sampler_param_->name();
    }
  }

  SamplerD3D9* d3d_sampler = down_cast<SamplerD3D9*>(sampler);
  for (int stage = 0; stage < number_sampler_units_; stage++) {
    d3d_sampler->SetTextureAndStates(sampler_unit_index_array_[stage]);
  }
}

void EffectParamHandlerForSamplersD3D9::ResetEffectParam(
    RendererD3D9* renderer,
    ID3DXEffect* d3dx_effect) {
  Sampler* sampler = sampler_param_->value();
  if (!sampler) {
    sampler = renderer->error_sampler();
  }

  SamplerD3D9* d3d_sampler = down_cast<SamplerD3D9*>(sampler);
  for (int stage = 0; stage < number_sampler_units_; stage++) {
    d3d_sampler->ResetTexture(sampler_unit_index_array_[stage]);
  }
}

// Creates a handler for setting up the d3d9 sampler states based on the values
// on Sampler object pointed to by the sampler_param.  It does a lookup
// (by name) in the fragment shader constants to determine the index of the
// texture stage the sampler has been mapped to in hardware.  This index
// will be used when making calls to set the texture and various sampler
// states at render time.
EffectParamHandlerForSamplersD3D9::EffectParamHandlerForSamplersD3D9(
    ParamSampler* sampler_param,
    const D3DXPARAMETER_DESC& pdesc,
    LPD3DXCONSTANTTABLE fs_constant_table,
    LPDIRECT3DDEVICE9 d3d_device)
    : sampler_param_(sampler_param),
      number_sampler_units_(0) {
  if (!fs_constant_table) {
    DLOG(ERROR) << "Fragment shader constant table is NULL";
    return;
  }
  D3DXHANDLE sampler_handle = fs_constant_table->GetConstantByName(
      NULL,
      pdesc.Name);
  if (!sampler_handle) {
    DLOG(ERROR) << "Sampler " << pdesc.Name <<
        " not found in fragment shader";
    return;
  }
  D3DXCONSTANT_DESC desc_array[kMaxUnitsPerSampler];
  UINT num_desc = kMaxUnitsPerSampler;
  fs_constant_table->GetConstantDesc(sampler_handle, desc_array, &num_desc);
  // We have no good way of querying how many descriptions would really be
  // returned as we're capping the number to kMaxUnitsPerSampler (which should
  // be more than sufficient).  If however we do end up with the max number
  // there's a chance that there were actually more so let's log it.
  if (num_desc == kMaxUnitsPerSampler) {
    DLOG(WARNING) << "Number of constant descriptions might have exceeded "
                  << "the maximum of " << kMaxUnitsPerSampler;
  }
  for (UINT desc_index = 0; desc_index < num_desc; desc_index++) {
    D3DXCONSTANT_DESC constant_desc = desc_array[desc_index];
    if (constant_desc.Class == D3DXPC_OBJECT &&
        IsSamplerType(constant_desc.Type)) {
      sampler_unit_index_array_[number_sampler_units_++] =
          constant_desc.RegisterIndex;
    }
  }
  if (number_sampler_units_ == 0) {
    DLOG(ERROR) << "No matching sampler units found for " <<
        pdesc.Name;
  }
}

// Loops through all the parameters needed by the effect and updates the
// corresponding uniforms in the d3d Effect
void EffectD3D9::UpdateShaderConstantsFromEffect(
    const EffectParamHandlerCacheD3D9& effect_param_cache) {

  const EffectParamHandlerCacheD3D9::ElementArray& effect_params =
      effect_param_cache.GetElements();

  EffectParamHandlerCacheD3D9::ElementArray::size_type end =
      effect_params.size();
  for (EffectParamHandlerCacheD3D9::ElementArray::size_type ii = 0;
       ii != end;
       ++ii) {
    effect_params[ii]->SetEffectParam(renderer_, d3dx_effect_);
  }
}

// Updates the values of the vertex and fragment shader parameters using the
// current values of the corresponding Shape Param's and sets the various
// render states if they are different from the default value.
void EffectD3D9::PrepareForDraw(
    const EffectParamHandlerCacheD3D9& effect_param_cache) {
  // Patch in the vertex and fragment shader constants using
  // values from the matching Params of the Shape
  if (d3dx_effect_) {
    UpdateShaderConstantsFromEffect(effect_param_cache);
    UINT numpasses;
    d3dx_effect_->SetTechnique(d3dx_effect_->GetTechnique(0));
    HR(d3dx_effect_->Begin(&numpasses, 0));
    HR(d3dx_effect_->BeginPass(0));
  }
}

// Resets the render states back to their default value.
void EffectD3D9::PostDraw(
    ParamObject* param_object,
    const EffectParamHandlerCacheD3D9& effect_param_cache) {
  if (d3dx_effect_) {
    HR(d3dx_effect_->EndPass());
    HR(d3dx_effect_->End());

    const EffectParamHandlerCacheD3D9::ElementArray& effect_params =
        effect_param_cache.GetElements();

    EffectParamHandlerCacheD3D9::ElementArray::size_type end =
        effect_params.size();
    for (EffectParamHandlerCacheD3D9::ElementArray::size_type ii = 0;
         ii != end;
         ++ii) {
      effect_params[ii]->ResetEffectParam(renderer_, d3dx_effect_);
    }
  }
}

// Handler for lost device. This invalidates the effect for a device reset.
// Returns true on success.
bool EffectD3D9::OnLostDevice() {
  if (d3dx_effect_)
    return HR(d3dx_effect_->OnLostDevice());
  else
    return true;
}

// Handler for reset device. This restores the effect after a device reset.
// Returns true on success.
bool EffectD3D9::OnResetDevice() {
  if (d3dx_effect_)
    return HR(d3dx_effect_->OnResetDevice());
  else
    return true;
}

}  // namespace o3d
