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


// This file contains the declaration to the Effect class.

#ifndef O3D_CORE_CROSS_EFFECT_H_
#define O3D_CORE_CROSS_EFFECT_H_

#include <vector>
#include "core/cross/param_object.h"
#include "core/cross/param.h"
#include "core/cross/stream.h"
#include "core/cross/types.h"

namespace o3d {

class Texture;
class Transform;
class ParamObject;

// Used to return info about a parameter an effect needs.
class EffectParameterInfo {
 public:
  EffectParameterInfo() : class_type_(NULL), sas_class_type_(NULL) {
  }

  // Parameters:
  //   name: name of parameter.
  //   class_type: type of parameter
  //   num_elements:  number of elements.  Non-zero for array types, zero
  //                  for non-array types.
  //   semantic: semantic.
  //   sas_class_type: type of standard parameter to use for this param.
  EffectParameterInfo(const String& name,
                      const ObjectBase::Class* class_type,
                      int num_elements,
                      const String& semantic,
                      const ObjectBase::Class* sas_class_type);

  // Name of parameter.
  const String& name() const {
    return name_;
  }

  // Type of parameter.
  const ObjectBase::Class* class_type() const {
    return class_type_;
  }

  // Number of elements.
  int num_elements() const {
    return num_elements_;
  }

  // The semantic is always in upper case.
  const String& semantic() const {
    return semantic_;
  }

  // If this is a Standard Parameter (SAS) this will be a class.
  // Otherwise it will be NULL.
  const ObjectBase::Class* sas_class_type () const {
    return sas_class_type_;
  }

 private:
  String name_;
  const ObjectBase::Class* class_type_;
  int num_elements_;
  String semantic_;
  const ObjectBase::Class* sas_class_type_;
};

typedef std::vector<EffectParameterInfo> EffectParameterInfoArray;

// Used to return info about a Stream an effect needs.
class EffectStreamInfo {
 public:
  EffectStreamInfo()
      : semantic_(Stream::UNKNOWN_SEMANTIC),
        semantic_index_(0) {
  }

  EffectStreamInfo(Stream::Semantic semantic,
                   int semantic_index)
      : semantic_(semantic),
        semantic_index_(semantic_index) {
  }

  // Get the semantic associated with the Stream.
  Stream::Semantic semantic() const {
    return semantic_;
  }

  // Get the semantic index associated with the Stream.
  int semantic_index() const {
    return semantic_index_;
  }

 private:
  Stream::Semantic semantic_;
  int semantic_index_;
};

typedef std::vector<EffectStreamInfo> EffectStreamInfoArray;

// An Effect object carries all the settings needed to
// completely specify a full graphics pipeline, from culling at the
// beginning to blending at the end.  An Effect contains the vertex and fragment
// shader.
class Effect : public ParamObject {
 public:
  typedef SmartPointer<Effect> Ref;
  typedef WeakPointer<Effect> WeakPointerType;

  // The MatrixLoadOrder is the order in which Matrix parameters are
  // loaded to the GPU.
  // ROW_MAJOR means matrix[0] represents the first row of the matrix.  This
  // format is used when doing matrix/vector multiplication as mul(v, M).
  // COLUMN_MAJOR means matrix[0] represents the first column of the matrix.
  // This format is used when doing matrix/vector multiplication as mul(M, v),
  // and usually requires the Matrix parameter to be transposed on load.
  enum MatrixLoadOrder {
    ROW_MAJOR,
    COLUMN_MAJOR,
  };

  static const char* kVertexShaderEntryPointPrefix;
  static const char* kFragmentShaderEntryPointPrefix;
  static const char* kMatrixLoadOrderPrefix;

  // Accessor for effect source.
  const String& source() {
    return source_;
  }

  // Loads the vertex and fragment shader programs from an string containing
  // a DirectX FX description.
  virtual bool LoadFromFXString(const String& effect) = 0;

  // For each of the effect's uniform parameters, creates corresponding
  // parameters on the given ParamObject. Skips SAS Parameters.
  // Parameters:
  //  param_object: object that will hold the created params
  void CreateUniformParameters(ParamObject* param_object);

  // For each of the effect's uniform parameters, if it is a SAS parameter
  // creates corresponding StandardParamMatrix4 parameters on the given
  // ParamObject.
  // Parameters:
  //  param_object: object that will hold the created params
  void CreateSASParameters(ParamObject* param_object);

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }
  // Gets info about the parameters this effect needs.
  // Parameters:
  //   info_array: EffectParameterInfoArray to receive info.
  virtual void GetParameterInfo(EffectParameterInfoArray* info_array) = 0;

  // Gets info about the varying parameters this effects vertex shader needs.
  // Parameters:
  //   info_array: EffectParameterInfoArray to receive info.
  virtual void GetStreamInfo(EffectStreamInfoArray* info_array) = 0;

  // Sets the order that matrices will be loaded to the GPU for this effect
  // (row major or column major).
  // Parameters:
  //   matrix_load_order:  ROW_MAJOR or COLUMN_MAJOR
  void set_matrix_load_order(MatrixLoadOrder matrix_load_order) {
    matrix_load_order_ = matrix_load_order;
  }

  // Gets the order that matrices will be loaded to the GPU for this effect
  // (row major or column major).
  MatrixLoadOrder matrix_load_order() const {
    return matrix_load_order_;
  }

  // Validates an effect meaning parse the effect and verify it does not
  // break any o3d rules. For example it must NOT have any technique
  // or render states or texture generation or sampler statements etc and it
  // must have the o3d entry point specification.
  // Parameters:
  //   effect: Effect string in o3d format.
  //   vertex_shader_entry_point: String to receive name of vertex shader entry
  //       point.
  //   fragment_shader_entry_point: String to receive name of fragment
  //     shader entry point.
  // Returns:
  //   true if effect was valid.
  bool ValidateFX(const String& effect,
                  String* vertex_shader_entry_point,
                  String* fragment_shader_entry_point,
                  MatrixLoadOrder* matrix_load_order);

 protected:
  explicit Effect(ServiceLocator* service_locator);

  // Accessor for source.
  void set_source(const String& source) {
    source_ = source;
  }

  // For each of the effect's uniform parameters, creates corresponding
  // parameters on the given ParamObject. Creates SAS or non-SAS only
  // depending on the sas argument.
  // Parameters:
  //   param_object: object that will hold the created params.
  //   sas: true means create only SAS params. false means create only non-SAS
  //       params.
  void CreateSpecifiedParameters(ParamObject* param_object, bool sas);

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;
  MatrixLoadOrder matrix_load_order_;

  // The source for the shaders on this effect.
  String source_;

  O3D_DECL_CLASS(Effect, NamedObject);
  DISALLOW_COPY_AND_ASSIGN(Effect);
};  // Effect

// Array container for Effect pointers
typedef std::vector<Effect*> EffectArray;

class ParamEffect : public TypedRefParam<Effect> {
 public:
  typedef SmartPointer<ParamEffect> Ref;

  ParamEffect(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedRefParam<Effect>(service_locator, dynamic, read_only) {}

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamEffect, RefParamBase);
  DISALLOW_COPY_AND_ASSIGN(ParamEffect);
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_EFFECT_H_
