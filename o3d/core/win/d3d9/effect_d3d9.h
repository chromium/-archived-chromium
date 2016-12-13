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


// This file contains the declaration of the EffectD3D9 class.

#ifndef O3D_CORE_WIN_D3D9_EFFECT_D3D9_H_
#define O3D_CORE_WIN_D3D9_EFFECT_D3D9_H_

#include <d3d9.h>
#include <d3dx9.h>
#include <atlbase.h>
#include <vector>
#include "core/cross/effect.h"

interface ID3DXEffect;

namespace o3d {

class Material;
class ParamObject;
class RendererD3D9;
class SemanticManager;
class ShapeDataD3D9;

// A class to set an effect parameter from an O3D parameter.
class EffectParamHandlerD3D9 {
 public:
  virtual ~EffectParamHandlerD3D9() { }

  // Sets a D3D9 Effect Parameter by an O3D Param.
  virtual void SetEffectParam(RendererD3D9* renderer,
                              ID3DXEffect* d3d_effect) = 0;

  // Resets a D3D9 Effect parameter to default value.  This is currently
  // used to unbind textures contained in Sampler parameters.
  virtual void ResetEffectParam(RendererD3D9* renderer,
                                ID3DXEffect* d3d_effect) {}
};

// This class creates a use limited array of pointers to instances of a certain
// class. It will destroy this instances when Clear() is called or on
// destruction. You can only add to the array and clear it. No other operations
// are supported.
template <typename T>
class ClassPointerArray {
 public:
  // Public declaration of the type used to store the array.
  typedef std::vector<T*> ElementArray;

  ~ClassPointerArray() {
    Clear();
  }

  // Clears the array and delete all the elements it points to.
  void Clear() {
    for (ElementArray::size_type ii = 0; ii < elements_.size(); ++ii) {
      delete elements_[ii];
    }
    elements_.clear();
  }

  // Adds an element to the array.
  // Parameters:
  //   element: Element to add.
  void AddElement(T* element) {
    elements_.push_back(element);
  }

  // Gets a const reference to the elements currently in the array
  // as a const stl::vector& so you can use the array with standard stl
  // operations.
  // Returns:
  //   const references to the elements.
  const ElementArray& GetElements() const {
    return elements_;
  }
 private:
  ElementArray elements_;
};

typedef ClassPointerArray<EffectParamHandlerD3D9> EffectParamHandlerCacheD3D9;

// EffectD3D9 is an implementation of the Effect object for DX9.  It provides
// the API for setting the vertex and framgent shaders for the Effect in HLSL.
// Currently the two shaders can either be provided separately as HLSL code
// or together in the DX9 FX format.
class EffectD3D9 : public Effect {
 public:
  EffectD3D9(ServiceLocator* service_locator, IDirect3DDevice9* d3d_device);
  ~EffectD3D9();

  // Reads the vertex and fragment shaders from string in the DX9 FX format.
  // It returns true if the shaders were successfully compiled.
  bool LoadFromFXString(const String& effect);

  // Binds the shaders to the device and sets up all the shader parameters using
  // the values from the matching Param's of the ParamObject.
  void PrepareForDraw(const EffectParamHandlerCacheD3D9& effect_param_cache);

  // Removes any pipeline state-changes installed during a draw.
  void PostDraw(ParamObject* param_object,
                const EffectParamHandlerCacheD3D9& effect_param_cache);

  // Gets info about the parameters this effect needs.
  // Overriden from Effect.
  virtual void GetParameterInfo(EffectParameterInfoArray* info_array);

  // Gets info about the parameters this effect needs.
  // Overriden from Effect.
  virtual void GetStreamInfo(EffectStreamInfoArray* info_array);

  // Returns a pointer to the DX9 vertex shader.
  IDirect3DVertexShader9* d3d_vertex_shader() const {
    return d3d_vertex_shader_;
  }

  // Returns a pointer to the DX9 fragment shader.
  IDirect3DPixelShader9* d3d_fragment_shader() const {
    return d3d_fragment_shader_;
  }

  // Updates the binds between the params in the list of param objecst and the
  // shader parameters. The two are matched using their names.
  void UpdateParameterMappings(
      const std::vector<ParamObject*>& param_object_list,
      EffectParamHandlerCacheD3D9* effect_param_cache);

  // Handler for lost device. This invalidates the effect for a device reset.
  bool OnLostDevice();

  // Handler for reset device. This restores the effect after a device reset.
  bool OnResetDevice();

 private:
  void UpdateShaderConstantsFromEffect(
      const EffectParamHandlerCacheD3D9& effect_param_cache);

  // Adds a Parameter Mapping from an O3D param to a d3d parameter if they
  // match in type.
  // Parameters:
  //   param: Param we are attempting to map.
  //   pdesc: d3d parameter description.
  //   phandle: Handle to d3d parameter.
  //   effect_param_cache: Cache to add mapping to.
  // Returns:
  //   true if a mapping was added.
  bool AddParameterMapping(Param* param,
                           const D3DXPARAMETER_DESC& pdesc,
                           D3DXHANDLE phandle,
                           EffectParamHandlerCacheD3D9* effect_param_cache);

  bool InitializeFX();
  void DisplayFXError(const String &header, LPD3DXBUFFER error_buffer);

  // Prepares o3d effect string by converting to platform specific effect
  // string.
  bool PrepareFX(const String& effect, String* prepared_effect);

  // Clears, Frees the D3D9 parts of this effect.
  void ClearD3D9Effect();

  SemanticManager* semantic_manager_;
  RendererD3D9* renderer_;

  CComPtr<IDirect3DVertexShader9> d3d_vertex_shader_;
  CComPtr<IDirect3DPixelShader9> d3d_fragment_shader_;
  CComPtr<ID3DXConstantTable> fs_constant_table_;
  CComPtr<IDirect3DDevice9> d3d_device_;
  CComPtr<ID3DXEffect> d3dx_effect_;
};

}  // namespace o3d

#endif  // O3D_CORE_WIN_D3D9_EFFECT_D3D9_H_
