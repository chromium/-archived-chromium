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


// This file contains declarations for the StandardParamMatrix4 template,
// and typedefs for the 24 instantiations.

#ifndef O3D_CORE_CROSS_STANDARD_PARAM_H_
#define O3D_CORE_CROSS_STANDARD_PARAM_H_

#include "core/cross/param.h"
#include "core/cross/transformation_context.h"

namespace o3d {

// The StandardParamMatrix4 class provides matrices computed from the 24
// standard values:  6 combinations of the world, view and projection matrices
// retrieved from the Context on the Client, and their inverse, transpose,
// and inverse transpose (24 in all).

// Predefined matrix semantics for Params.  These correspond to the
// Standard Annotations and Semantics (SAS) that O3D
// understands.  See http://developer.nvidia.com/object/using_sas.html.
// See also Effect::GetParameterSemantic().

// Here we create a list of macros so we can expand the list different ways
// multiple times. This makes it MUCH easier to maintain. Instead of editing
// in 144 places to edit something we only need to edit in 1.
#undef O3D_STANDARD_ANNOTATION_ENTRY
#define O3D_STANDARD_ANNOTATIONS                \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      WORLD,                                    \
      World)                                    \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      WORLD_INVERSE,                            \
      WorldInverse)                             \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      WORLD_TRANSPOSE,                          \
      WorldTranspose)                           \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      WORLD_INVERSE_TRANSPOSE,                  \
      WorldInverseTranspose)                    \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      VIEW,                                     \
      View)                                     \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      VIEW_INVERSE,                             \
      ViewInverse)                              \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      VIEW_TRANSPOSE,                           \
      ViewTranspose)                            \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      VIEW_INVERSE_TRANSPOSE,                   \
      ViewInverseTranspose)                     \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      PROJECTION,                               \
      Projection)                               \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      PROJECTION_INVERSE,                       \
      ProjectionInverse)                        \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      PROJECTION_TRANSPOSE,                     \
      ProjectionTranspose)                      \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      PROJECTION_INVERSE_TRANSPOSE,             \
      ProjectionInverseTranspose)               \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      WORLD_VIEW,                               \
      WorldView)                                \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      WORLD_VIEW_INVERSE,                       \
      WorldViewInverse)                         \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      WORLD_VIEW_TRANSPOSE,                     \
      WorldViewTranspose)                       \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      WORLD_VIEW_INVERSE_TRANSPOSE,             \
      WorldViewInverseTranspose)                \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      VIEW_PROJECTION,                          \
      ViewProjection)                           \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      VIEW_PROJECTION_INVERSE,                  \
      ViewProjectionInverse)                    \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      VIEW_PROJECTION_TRANSPOSE,                \
      ViewProjectionTranspose)                  \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      VIEW_PROJECTION_INVERSE_TRANSPOSE,        \
      ViewProjectionInverseTranspose)           \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      WORLD_VIEW_PROJECTION,                    \
      WorldViewProjection)                      \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      WORLD_VIEW_PROJECTION_INVERSE,            \
      WorldViewProjectionInverse)               \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      WORLD_VIEW_PROJECTION_TRANSPOSE,          \
      WorldViewProjectionTranspose)             \
  O3D_STANDARD_ANNOTATION_ENTRY(                \
      WORLD_VIEW_PROJECTION_INVERSE_TRANSPOSE,  \
      WorldViewProjectionInverseTranspose)

#undef O3D_STANDARD_ANNOTATION_ENTRY
#define O3D_STANDARD_ANNOTATION_ENTRY(enum_name, class_name) enum_name,

enum Semantic {
  INVALID_SEMANTIC,
  O3D_STANDARD_ANNOTATIONS
};

template <Semantic S> class StandardParamMatrix4 : public ParamMatrix4 {
 public:
  explicit StandardParamMatrix4(ServiceLocator* service_locator)
      : ParamMatrix4(service_locator, true, true),
        transformation_context_(service_locator->
                                GetService<TransformationContext>()) {
    SetNotCachable();
  }

  virtual void ComputeValue();

 private:
  TransformationContext* transformation_context_;
  DISALLOW_COPY_AND_ASSIGN(StandardParamMatrix4<S>);
};

#undef O3D_STANDARD_ANNOTATION_ENTRY
#define O3D_STANDARD_ANNOTATION_ENTRY(enum_name, class_name)                 \
class class_name ## ParamMatrix4 : public StandardParamMatrix4<enum_name> {  \
 public:                                                                     \
  /* Factory Create method. */                                               \
  static ObjectBase::Ref Create(ServiceLocator* service_locator) {           \
    return ObjectBase::Ref(new class_name ## ParamMatrix4(service_locator)); \
  }                                                                          \
 private:                                                                    \
  class_name ## ParamMatrix4(ServiceLocator* service_locator)                \
  : StandardParamMatrix4<enum_name>(service_locator) {                       \
  }                                                                          \
  O3D_DECL_CLASS(class_name ## ParamMatrix4, ParamMatrix4);                  \
};

O3D_STANDARD_ANNOTATIONS;
}  // namespace o3d

#endif  // O3D_CORE_CROSS_STANDARD_PARAM_H_
