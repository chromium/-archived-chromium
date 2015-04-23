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


// This file contains the declaration of the EffectParamGL and EffectGL classes.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_EFFECT_GL_H_
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_EFFECT_GL_H_

#include <vector>
#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/service/cross/resource.h"
#include "command_buffer/service/cross/gl/gl_utils.h"

namespace o3d {
namespace command_buffer {

class GAPIGL;
class EffectGL;

// GL version of EffectParam.
class EffectParamGL: public EffectParam {
 public:
  EffectParamGL(effect_param::DataType data_type,
                EffectGL *effect,
                unsigned int param_index);
  virtual ~EffectParamGL();

  // Sets the data into the GL effect parameter.
  bool SetData(GAPIGL *gapi, unsigned int size, const void * data);

  // Gets the description of the parameter.
  bool GetDesc(unsigned int size, void *data);

  // Resets the effect back-pointer. This is called when the effect gets
  // destroyed, to invalidate the parameter.
  void ResetEffect() { effect_ = NULL; }

  // Creates an EffectParamGL from the EffectGL, by index.
  static EffectParamGL *Create(EffectGL *effect,
                               unsigned int index);
 private:
  EffectGL *effect_;
  unsigned int low_level_param_index_;
  DISALLOW_COPY_AND_ASSIGN(EffectParamGL);
};

// GL version of Effect.
class EffectGL : public Effect {
 public:
  EffectGL(CGprogram vertex_program,
           CGprogram fragment_program);
  virtual ~EffectGL();

  // Compiles and creates an effect from source code.
  static EffectGL *Create(GAPIGL *gapi,
                          const String &effect_code,
                          const String &vertex_program_entry,
                          const String &fragment_program_entry);

  // Applies the effect states (vertex shader, pixel shader) to GL.
  bool Begin(GAPIGL *gapi);

  // Resets the effect states (vertex shader, pixel shader) to GL.
  void End(GAPIGL *gapi);

  // Gets the number of parameters in the effect.
  unsigned int GetParamCount() const;

  // Creates an effect parameter with the specified index.
  EffectParamGL *CreateParam(unsigned int index);

  // Creates an effect parameter of the specified name.
  EffectParamGL *CreateParamByName(const char *name);

 private:
  struct LowLevelParam {
    const char *name;
    CGparameter vp_param;
    CGparameter fp_param;
    ResourceID sampler_id;
  };
  typedef std::vector<LowLevelParam> LowLevelParamList;
  typedef std::vector<EffectParamGL *> ParamResourceList;

  static CGparameter GetEitherCgParameter(
      const LowLevelParam &low_level_param) {
    return low_level_param.vp_param ?
        low_level_param.vp_param : low_level_param.fp_param;
  }

  int GetLowLevelParamIndexByName(const char *name);
  void AddLowLevelParams(CGparameter cg_param, bool vp);

  // Creates the low level structures.
  void Initialize();

  // Links a param into this effect.
  void LinkParam(EffectParamGL *param);

  // Unlinks a param into this effect.
  void UnlinkParam(EffectParamGL *param);

  CGprogram vertex_program_;
  CGprogram fragment_program_;
  // List of all the Param resources created.
  ParamResourceList resource_params_;
  // List of all the Cg parameters present in either the vertex program or the
  // fragment program.
  LowLevelParamList low_level_params_;
  // List of the indices of the low level params that are samplers.
  std::vector<unsigned int> sampler_params_;
  bool update_samplers_;

  friend class EffectParamGL;
  DISALLOW_COPY_AND_ASSIGN(EffectGL);
};


}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_EFFECT_GL_H_
