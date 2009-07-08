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


// This file contains the definition of the D3D9 versions of effect-related
// resource classes.

#ifndef O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_EFFECT_D3D9_H__
#define O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_EFFECT_D3D9_H__

#include <vector>
#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/service/win/d3d9/d3d9_utils.h"
#include "command_buffer/service/cross/resource.h"

namespace o3d {
namespace command_buffer {

class GAPID3D9;
class EffectD3D9;

// ps_2_0 limit
static const unsigned int kMaxSamplerUnits = 16;

// D3D version of EffectParam. This class keeps a reference to the D3D effect.
class EffectParamD3D9: public EffectParam {
 public:
  EffectParamD3D9(effect_param::DataType data_type,
                  EffectD3D9 *effect,
                  D3DXHANDLE handle);
  virtual ~EffectParamD3D9();

  // Sets the data into the D3D effect parameter.
  bool SetData(GAPID3D9 *gapi, unsigned int size, const void * data);

  // Gets the description of the parameter.
  bool GetDesc(unsigned int size, void *data);

  // Resets the effect back-pointer. This is called when the effect gets
  // destroyed, to invalidate the parameter.
  void ResetEffect() { effect_ = NULL; }

  static EffectParamD3D9 *Create(EffectD3D9 *effect, D3DXHANDLE handle);
 private:
  EffectD3D9 *effect_;
  D3DXHANDLE handle_;
  unsigned int sampler_unit_count_;
  scoped_array<unsigned int> sampler_units_;
};

// D3D9 version of Effect.
class EffectD3D9 : public Effect {
 public:
  EffectD3D9(ID3DXEffect *d3d_effect,
             ID3DXConstantTable *fs_constant_table,
             IDirect3DVertexShader9 *d3d_vertex_shader);
  virtual ~EffectD3D9();
  // Compiles and creates an effect from source code.
  static EffectD3D9 *Create(GAPID3D9 *gapi,
                            const String &effect_code,
                            const String &vertex_program_entry,
                            const String &fragment_program_entry);
  // Applies the effect states (vertex shader, pixel shader) to D3D.
  bool Begin(GAPID3D9 *gapi);
  // Resets the effect states (vertex shader, pixel shader) to D3D.
  void End(GAPID3D9 *gapi);
  // Commits parameters to D3D, if they were modified while the effect is
  // active.
  bool CommitParameters(GAPID3D9 *gapi);

  // Gets the number of parameters in the effect.
  unsigned int GetParamCount();
  // Creates an effect parameter with the specified index.
  EffectParamD3D9 *CreateParam(unsigned int index);
  // Creates an effect parameter of the specified name.
  EffectParamD3D9 *CreateParamByName(const char *name);
  // Gets the number of stream inputs for the effect.
  unsigned int GetStreamCount();
  // Gets the stream data with the specified index.
  bool GetStreamDesc(unsigned int index, unsigned int size, void *data);
 private:
  typedef std::vector<EffectParamD3D9 *> ParamList;
  typedef std::vector<effect_stream::Desc> StreamList;

  // Links a param into this effect.
  void LinkParam(EffectParamD3D9 *param);
  // Unlinks a param into this effect.
  void UnlinkParam(EffectParamD3D9 *param);
  // Sets sampler states.
  bool SetSamplers(GAPID3D9 *gapi);
  // Sets streams vector.
  bool SetStreams();

  ID3DXEffect *d3d_effect_;
  IDirect3DVertexShader9 *d3d_vertex_shader_;
  ID3DXConstantTable *fs_constant_table_;
  ParamList params_;
  StreamList streams_;
  bool sync_parameters_;
  ResourceID samplers_[kMaxSamplerUnits];

  friend class EffectParamD3D9;
  DISALLOW_COPY_AND_ASSIGN(EffectD3D9);
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_EFFECT_D3D9_H__
