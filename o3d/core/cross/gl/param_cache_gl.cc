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


// This file contains the definition of the ParamCacheGL class.

#include "core/cross/precompile.h"
#include "core/cross/error.h"
#include "core/cross/param_array.h"
#include "core/cross/renderer.h"
#include "core/cross/semantic_manager.h"
#include "core/cross/gl/param_cache_gl.h"
#include "core/cross/gl/effect_gl.h"
#include "core/cross/gl/sampler_gl.h"
#include "core/cross/gl/renderer_gl.h"
#include "core/cross/element.h"
#include "core/cross/draw_element.h"

namespace o3d {

typedef std::vector<ParamObject *> ParamObjectList;

ParamCacheGL::ParamCacheGL(SemanticManager* semantic_manager,
                           Renderer* renderer)
    : semantic_manager_(semantic_manager),
      renderer_(renderer),
      last_vertex_program_(0),
      last_fragment_program_(0) {
}

bool ParamCacheGL::ValidateEffect(Effect* effect) {
  DLOG_ASSERT(effect);

  EffectGL* effect_gl = down_cast<EffectGL*>(effect);
  return (effect_gl->cg_vertex_program() == last_vertex_program_ ||
          effect_gl->cg_fragment_program() == last_fragment_program_);
}

void ParamCacheGL::UpdateCache(Effect* effect,
                               DrawElement* draw_element,
                               Element* element,
                               Material* material,
                               ParamObject* override) {
  DLOG_ASSERT(effect);
  EffectGL* effect_gl = down_cast<EffectGL*>(effect);

  ScanCgEffectParameters(effect_gl->cg_vertex_program(),
                         effect_gl->cg_fragment_program(),
                         draw_element,
                         element,
                         material,
                         override);

  last_vertex_program_ = effect_gl->cg_vertex_program();
  last_fragment_program_ = effect_gl->cg_fragment_program();
}

template <typename T>
class TypedEffectParamHandlerGL : public EffectParamHandlerGL {
 public:
  explicit TypedEffectParamHandlerGL(T* param)
      : param_(param) {
  }
  virtual void SetEffectParam(RendererGL* renderer, CGparameter cg_param);
 private:
  T* param_;
};

class EffectParamHandlerGLMatrixRows : public EffectParamHandlerGL {
 public:
  explicit EffectParamHandlerGLMatrixRows(ParamMatrix4* param)
      : param_(param) {
  }
  virtual void SetEffectParam(RendererGL* renderer, CGparameter cg_param) {
    // set the data as floats in row major order.
    Matrix4 mat = param_->value();
    cgSetMatrixParameterfr(cg_param, &mat[0][0]);
  }
 private:
  ParamMatrix4* param_;
};

class EffectParamHandlerGLMatrixColumns : public EffectParamHandlerGL {
 public:
  explicit EffectParamHandlerGLMatrixColumns(ParamMatrix4* param)
      : param_(param) {
  }
  virtual void SetEffectParam(RendererGL* renderer, CGparameter cg_param) {
    // set the data as floats in column major order.
    Matrix4 mat = param_->value();
    cgSetMatrixParameterfc(cg_param, &mat[0][0]);
  }
 private:
  ParamMatrix4* param_;
};

template <>
void TypedEffectParamHandlerGL<ParamFloat>::SetEffectParam(
    RendererGL* renderer,
    CGparameter cg_param) {
  Float f = param_->value();
  cgSetParameter1f(cg_param, f);
};

template <>
void TypedEffectParamHandlerGL<ParamFloat2>::SetEffectParam(
    RendererGL* renderer,
    CGparameter cg_param) {
  Float2 f = param_->value();
  cgSetParameter2fv(cg_param, f.GetFloatArray());
};

template <>
void TypedEffectParamHandlerGL<ParamFloat3>::SetEffectParam(
    RendererGL* renderer,
    CGparameter cg_param) {
  Float3 f = param_->value();
  cgSetParameter3fv(cg_param, f.GetFloatArray());
};

template <>
void TypedEffectParamHandlerGL<ParamFloat4>::SetEffectParam(
    RendererGL* renderer,
    CGparameter cg_param) {
  Float4 f = param_->value();
  cgSetParameter4fv(cg_param, f.GetFloatArray());
};

template <>
void TypedEffectParamHandlerGL<ParamInteger>::SetEffectParam(
    RendererGL* renderer,
    CGparameter cg_param) {
  int i = param_->value();
  cgSetParameter1i(cg_param, i);
};

template <>
void TypedEffectParamHandlerGL<ParamBoolean>::SetEffectParam(
    RendererGL* renderer,
    CGparameter cg_param) {
  int i = param_->value();
  cgSetParameter1i(cg_param, i);
};

class EffectParamHandlerForSamplersGL : public EffectParamHandlerGL {
 public:
  explicit EffectParamHandlerForSamplersGL(ParamSampler* param)
      : param_(param) {
  }
  virtual void SetEffectParam(RendererGL* renderer, CGparameter cg_param) {
    SamplerGL* sampler_gl = down_cast<SamplerGL*>(param_->value());
    if (!sampler_gl) {
      // Use the error sampler.
      sampler_gl = down_cast<SamplerGL*>(renderer->error_sampler());
      // If no error texture is set then generate an error.
      if (!renderer->error_texture()) {
        O3D_ERROR(param_->service_locator())
            << "Missing Sampler for ParamSampler " << param_->name();
      }
    }
    sampler_gl->SetTextureAndStates(cg_param);
  }
  virtual void ResetEffectParam(RendererGL* renderer, CGparameter cg_param) {
    SamplerGL* sampler_gl = down_cast<SamplerGL*>(param_->value());
    if (!sampler_gl) {
      sampler_gl = down_cast<SamplerGL*>(renderer->error_sampler());
    }
    sampler_gl->ResetTexture(cg_param);
  }
 private:
  ParamSampler* param_;
};

template <typename T>
class EffectParamArrayHandlerGL : public EffectParamHandlerGL {
 public:
  explicit EffectParamArrayHandlerGL(ParamParamArray* param)
      : param_(param) {
  }
  virtual void SetEffectParam(RendererGL* renderer, CGparameter cg_param) {
    ParamArray* param = param_->value();
    if (param) {
      int size = cgGetArraySize(cg_param, 0);
      if (size != static_cast<int>(param->size())) {
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
            CGparameter cg_element = cgGetArrayParameter(cg_param, i);
            SetElement(cg_element, down_cast<T*>(untyped_element));
          } else {
            O3D_ERROR(param->service_locator())
                << "Param in ParamArray at index " << i << " is not a "
                << T::GetApparentClassName();
          }
        }
      }
    }
  }
  void SetElement(CGparameter cg_element, T* param);

 private:
  ParamParamArray* param_;
};

template <bool column_major>
class EffectParamArrayMatrix4HandlerGL : public EffectParamHandlerGL {
 public:
  explicit EffectParamArrayMatrix4HandlerGL(ParamParamArray* param)
      : param_(param) {
  }
  virtual void SetEffectParam(RendererGL* renderer, CGparameter cg_param) {
    ParamArray* param = param_->value();
    if (param) {
      int size = cgGetArraySize(cg_param, 0);
      if (size != static_cast<int>(param->size())) {
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
            CGparameter cg_element = cgGetArrayParameter(cg_param, i);
            SetElement(cg_element, down_cast<ParamMatrix4*>(untyped_element));
          } else {
            O3D_ERROR(param->service_locator())
                << "Param in ParamArray at index " << i
                << " is not a ParamMatrix4";
          }
        }
      }
    }
  }
  void SetElement(CGparameter cg_element, ParamMatrix4* param);

 private:
  ParamParamArray* param_;
};

class EffectParamArraySamplerHandlerGL : public EffectParamHandlerGL {
 public:
  explicit EffectParamArraySamplerHandlerGL(ParamParamArray* param)
      : param_(param) {
  }
  virtual void SetEffectParam(RendererGL* renderer, CGparameter cg_param) {
    ParamArray* param = param_->value();
    if (param) {
      int size = cgGetArraySize(cg_param, 0);
      if (size != static_cast<int>(param->size())) {
        O3D_ERROR(param->service_locator())
            << "number of params in ParamArray does not match number of params "
            << "needed by shader array";
      } else {
        for (int i = 0; i < size; ++i) {
          Param* untyped_element = param->GetUntypedParam(i);
          // TODO(gman): Make this check happen when building the param cache.
          //    To do that would require that ParamParamArray mark it's owner
          //    as changed if a Param in it's ParamArray changes.
          if (untyped_element->IsA(ParamSampler::GetApparentClass())) {
            CGparameter cg_element = cgGetArrayParameter(cg_param, i);
            ParamSampler* element = down_cast<ParamSampler*>(untyped_element);
            SamplerGL* sampler_gl = down_cast<SamplerGL*>(element->value());
            if (!sampler_gl) {
              // Use the error sampler.
              sampler_gl = down_cast<SamplerGL*>(renderer->error_sampler());
              // If no error texture is set then generate an error.
              if (!renderer->error_texture()) {
                O3D_ERROR(param_->service_locator())
                    << "Missing Sampler for ParamSampler '" << param_->name()
                    << "' index " << i;
              }
            }
            sampler_gl->SetTextureAndStates(cg_element);
          } else {
            O3D_ERROR(param->service_locator())
                << "Param in ParamArray at index " << i
                << " is not a ParamSampler";
          }
        }
      }
    }
  }
  virtual void ResetEffectParam(RendererGL* renderer, CGparameter cg_param) {
    ParamArray* param = param_->value();
    if (param) {
      int size = cgGetArraySize(cg_param, 0);
      if (size == static_cast<int>(param->size())) {
        for (int i = 0; i < size; ++i) {
          Param* untyped_element = param->GetUntypedParam(i);
          if (untyped_element->IsA(ParamSampler::GetApparentClass())) {
            CGparameter cg_element = cgGetArrayParameter(cg_param, i);
            ParamSampler* element = down_cast<ParamSampler*>(untyped_element);
            SamplerGL* sampler_gl = down_cast<SamplerGL*>(element->value());
            if (!sampler_gl) {
              sampler_gl = down_cast<SamplerGL*>(renderer->error_sampler());
            }
            sampler_gl->ResetTexture(cg_param);
          }
        }
      }
    }
  }

 private:
  ParamParamArray* param_;
};

template<>
void EffectParamArrayHandlerGL<ParamFloat>::SetElement(
    CGparameter cg_element,
    ParamFloat* param) {
  cgSetParameter1f(cg_element, param->value());
}

template<>
void EffectParamArrayHandlerGL<ParamFloat2>::SetElement(
    CGparameter cg_element,
    ParamFloat2* param) {
  Float2 f = param->value();
  cgSetParameter2fv(cg_element, f.GetFloatArray());
}

template<>
void EffectParamArrayHandlerGL<ParamFloat3>::SetElement(
    CGparameter cg_element,
    ParamFloat3* param) {
  Float3 f = param->value();
  cgSetParameter3fv(cg_element, f.GetFloatArray());
}

template<>
void EffectParamArrayHandlerGL<ParamFloat4>::SetElement(
    CGparameter cg_element,
    ParamFloat4* param) {
  Float4 f = param->value();
  cgSetParameter4fv(cg_element, f.GetFloatArray());
}

template<>
void EffectParamArrayMatrix4HandlerGL<false>::SetElement(
    CGparameter cg_element,
    ParamMatrix4* param) {
  // set the data as floats in row major order.
  Matrix4 mat = param->value();
  cgSetMatrixParameterfr(cg_element, &mat[0][0]);
}

template<>
void EffectParamArrayMatrix4HandlerGL<true>::SetElement(
    CGparameter cg_element,
    ParamMatrix4* param) {
  // set the data as floats in column major order.
  Matrix4 mat = param->value();
  cgSetMatrixParameterfc(cg_element, &mat[0][0]);
}

template<>
void EffectParamArrayHandlerGL<ParamInteger>::SetElement(
    CGparameter cg_element,
    ParamInteger* param) {
  cgSetParameter1i(cg_element, param->value());
}

template<>
void EffectParamArrayHandlerGL<ParamBoolean>::SetElement(
    CGparameter cg_element,
    ParamBoolean* param) {
  cgSetParameter1i(cg_element, param->value());
}

static EffectParamHandlerGL::Ref GetHandlerFromParamAndCgType(
    EffectGL* effect_gl,
    Param *param,
    CGtype cg_type) {
  EffectParamHandlerGL::Ref handler;
  if (param->IsA(ParamParamArray::GetApparentClass())) {
    ParamParamArray* param_param_array = down_cast<ParamParamArray*>(param);
    switch (cg_type) {
      case CG_FLOAT:
      case CG_FLOAT1:
        handler = EffectParamHandlerGL::Ref(
            new EffectParamArrayHandlerGL<ParamFloat>(param_param_array));
        break;
      case CG_FLOAT2:
        handler = EffectParamHandlerGL::Ref(
            new EffectParamArrayHandlerGL<ParamFloat2>(param_param_array));
        break;
      case CG_FLOAT3:
        handler = EffectParamHandlerGL::Ref(
            new EffectParamArrayHandlerGL<ParamFloat3>(param_param_array));
        break;
      case CG_FLOAT4:
        handler = EffectParamHandlerGL::Ref(
            new EffectParamArrayHandlerGL<ParamFloat4>(param_param_array));
        break;
      case CG_FLOAT4x4:
        if (effect_gl->matrix_load_order() == Effect::COLUMN_MAJOR) {
          handler = EffectParamHandlerGL::Ref(
              new EffectParamArrayMatrix4HandlerGL<true>(param_param_array));
        } else {
          handler = EffectParamHandlerGL::Ref(
              new EffectParamArrayMatrix4HandlerGL<false>(param_param_array));
        }
        break;
      case CG_INT:
      case CG_INT1:
        handler = EffectParamHandlerGL::Ref(
            new EffectParamArrayHandlerGL<ParamInteger>(param_param_array));
        break;
      case CG_BOOL:
      case CG_BOOL1:
        handler = EffectParamHandlerGL::Ref(
            new EffectParamArrayHandlerGL<ParamBoolean>(param_param_array));
        break;
      case CG_SAMPLER:
      case CG_SAMPLER1D:
      case CG_SAMPLER2D:
      case CG_SAMPLER3D:
      case CG_SAMPLERCUBE:
        handler = EffectParamHandlerGL::Ref(
            new EffectParamArraySamplerHandlerGL(param_param_array));
        break;
    }
  } else if (param->IsA(ParamMatrix4::GetApparentClass())) {
    if (cg_type == CG_FLOAT4x4) {
      if (effect_gl->matrix_load_order() == Effect::COLUMN_MAJOR) {
        // set the data as floats in column major order.
        handler = EffectParamHandlerGL::Ref(
            new EffectParamHandlerGLMatrixColumns(
                down_cast<ParamMatrix4*>(param)));
      } else {
        // set the data as floats in row major order.
        handler = EffectParamHandlerGL::Ref(
            new EffectParamHandlerGLMatrixRows(
                down_cast<ParamMatrix4*>(param)));
      }
    }
  } else if (param->IsA(ParamFloat::GetApparentClass())) {
    if (cg_type == CG_FLOAT ||
        cg_type == CG_FLOAT1) {
      handler = EffectParamHandlerGL::Ref(
          new TypedEffectParamHandlerGL<ParamFloat>(
              down_cast<ParamFloat*>(param)));
    }
  } else if (param->IsA(ParamFloat2::GetApparentClass())) {
    if (cg_type == CG_FLOAT2) {
      handler = EffectParamHandlerGL::Ref(
          new TypedEffectParamHandlerGL<ParamFloat2>(
              down_cast<ParamFloat2*>(param)));
    }
  } else if (param->IsA(ParamFloat3::GetApparentClass())) {
    if (cg_type == CG_FLOAT3) {
      handler = EffectParamHandlerGL::Ref(
          new TypedEffectParamHandlerGL<ParamFloat3>(
              down_cast<ParamFloat3*>(param)));
    }
  } else if (param->IsA(ParamFloat4::GetApparentClass())) {
    if (cg_type == CG_FLOAT4) {
      handler = EffectParamHandlerGL::Ref(
          new TypedEffectParamHandlerGL<ParamFloat4>(
              down_cast<ParamFloat4*>(param)));
    }
  }  else if (param->IsA(ParamInteger::GetApparentClass())) {
    if (cg_type == CG_INT || cg_type == CG_INT1) {
      handler = EffectParamHandlerGL::Ref(
          new TypedEffectParamHandlerGL<ParamInteger>(
              down_cast<ParamInteger*>(param)));
    }
  } else if (param->IsA(ParamBoolean::GetApparentClass())) {
    if (cg_type == CG_BOOL || cg_type == CG_BOOL1) {
      handler = EffectParamHandlerGL::Ref(
          new TypedEffectParamHandlerGL<ParamBoolean>(
              down_cast<ParamBoolean*>(param)));
    }
  } else if (param->IsA(ParamSampler::GetApparentClass())) {
    if (cg_type == CG_SAMPLER ||
        cg_type == CG_SAMPLER1D ||
        cg_type == CG_SAMPLER2D ||
        cg_type == CG_SAMPLER3D ||
        cg_type == CG_SAMPLERCUBE) {
      handler = EffectParamHandlerGL::Ref(
          new EffectParamHandlerForSamplersGL(
              down_cast<ParamSampler*>(param)));
    }
  }
  return handler;
}

// Local helper function for scanning varying Cg parameters of a
// program or effect and recording their entries into the varying map.
static void ScanVaryingParameters(CGprogram program,
                                  CGenum name_space,
                                  ParamCacheGL* param_cache_gl) {
  CGparameter cg_param = cgGetFirstLeafParameter(program, name_space);
  for (; cg_param; cg_param = cgGetNextLeafParameter(cg_param)) {
    if (!cgIsParameterReferenced(cg_param))
      continue;
    CGenum variability = cgGetParameterVariability(cg_param);
    CGenum direction = cgGetParameterDirection(cg_param);
    if (variability == CG_VARYING && direction == CG_IN) {
      // Add a link between the parameter and no stream (index -1)
      // NOTE: Stream indexes will be set later in
      // InsertMissingVertexStreams().
      if (param_cache_gl->varying_map().find(cg_param) ==
          param_cache_gl->varying_map().end()) {
        const char* cg_name = cgGetParameterName(cg_param);
        param_cache_gl->varying_map().insert(std::make_pair(cg_param, -1));
        DLOG(INFO) << "ElementGL Found CG_VARYING \""
                   << cg_name << " : "
                   << cgGetParameterSemantic(cg_param) << "\"";
      }
    }
  }
}

// Local helper function for scanning uniform Cg parameters of a
// program or effect and recording their entries into the parameter maps.
static void ScanUniformParameters(SemanticManager* semantic_manager,
                                  Renderer* renderer,
                                  CGprogram program,
                                  CGenum name_space,
                                  ParamCacheGL* param_cache_gl,
                                  const ParamObjectList& param_objects,
                                  EffectGL* effect_gl) {
  CGparameter cg_param = cgGetFirstParameter(program, name_space);
  for (; cg_param; cg_param = cgGetNextParameter(cg_param)) {
    if (!cgIsParameterReferenced(cg_param))
      continue;
    CGenum direction = cgGetParameterDirection(cg_param);
    if (direction != CG_IN)
      continue;
    CGtype cg_type = cgGetParameterType(cg_param);
    CGenum variability = cgGetParameterVariability(cg_param);
    const char* cg_name = cgGetParameterName(cg_param);

    if (variability == CG_UNIFORM) {
      // We have a CGparameter to add, find a Param that matches it by name.
      if (cg_type == CG_TEXTURE) {
        // CG_TEXTURE objects are handled by CG_SAMPLER objects.
        continue;
      }

      // TODO(o3d): The following code block should be removed once we start
      // creating sampler params for all effects coming in via the importer. For
      // the time being, we keep an extra ParamTexture that does the job it used
      // to do.  If we are using a ParamSampler on the object then the
      // ParamTexture will have no value and therefore its handler will have no
      // side-effects.
      if (cg_type == CG_SAMPLER ||
          cg_type == CG_SAMPLER1D ||
          cg_type == CG_SAMPLER2D ||
          cg_type == CG_SAMPLER3D ||
          cg_type == CG_SAMPLERCUBE) {
        // Uniform is a Sampler object. Find the CG_TEXTURE object
        // assigned to the CG_SAMPLER, then find a Param object with the
        // same name as the CG_TEXTURE. This is the tricky bit!
        if (param_cache_gl->sampler_map().find(cg_param) ==
            param_cache_gl->sampler_map().end()) {
          ParamTexture* param =
              effect_gl->GetTextureParamFromCgSampler(cg_param,
                                                      param_objects);
          if (param) {
            param_cache_gl->sampler_map().insert(std::make_pair(cg_param,
                                                                param));
          }
        }
      }

      // Find a Param of the same name, and record the link.
      if (param_cache_gl->uniform_map().find(cg_param) ==
          param_cache_gl->uniform_map().end()) {
        const ObjectBase::Class *sem_class = NULL;
        // Try looking by SAS class name.
        const char* cg_semantic = cgGetParameterSemantic(cg_param);
        if (cg_semantic != NULL && cg_semantic[0] != '\0') {
          // NOTE: this semantic is not the regularised profile semantic
          // output from the CGC compiler but the actual user supplied
          // semantic from the shader source code, so this match is valid.
          sem_class = semantic_manager->LookupSemantic(cg_semantic);
        }
        EffectParamHandlerGL::Ref handler;
        // Look through all the param objects to find a matching param.
        unsigned last = param_objects.size() - 1;
        for (unsigned int i = 0; i < param_objects.size(); ++i) {
          ParamObject *param_object = param_objects[i];
          Param *param = param_object->GetUntypedParam(cg_name);
          if (!param && sem_class) {
            param = param_object->GetUntypedParam(sem_class->name());
          }
          if (!param) {
            // If this is the last param object and we didn't find a matching
            // param then if it's a sampler use the error sampler
            if (i == last) {
              if (cg_type == CG_SAMPLER ||
                  cg_type == CG_SAMPLER1D ||
                  cg_type == CG_SAMPLER2D ||
                  cg_type == CG_SAMPLER3D ||
                  cg_type == CG_SAMPLERCUBE) {
                param =
                    renderer->error_param_sampler();
              }
            }
            if (!param) {
              continue;
            }
          }
          if (cg_type == CG_ARRAY) {
            // Substitute the first element's type for our type.
            cg_type = cgGetParameterType(cgGetArrayParameter(cg_param, 0));
          }
          handler = GetHandlerFromParamAndCgType(effect_gl, param, cg_type);
          if (!handler.IsNull()) {
            param_cache_gl->uniform_map().insert(
                std::make_pair(cg_param, handler));
            DLOG(INFO) << "ElementGL Matched CG_PARAMETER \""
                       << cg_name << "\" to Param \""
                       << param->name() << "\" from \""
                       << param_object->name() << "\"";
            break;
          } else {
            // We found a param, but it didn't match the type. keep looking.
            DLOG(ERROR) << "ElementGL Param \""
                        << param->name() << "\" type \""
                        << param->GetClassName() << "\" from \""
                        << param_object->name()
                        << "\" does not match CG_PARAMETER \""
                        << cg_name << "\"";
          }
        }
        if (handler.IsNull()) {
          DLOG(ERROR) << "No matching Param for CG_PARAMETER \""
                      << cg_name << "\"";
        }
      }
    }
  }
}

static void DoScanCgEffectParameters(SemanticManager* semantic_manager,
                                     Renderer* renderer,
                                     ParamCacheGL* param_cache_gl,
                                     CGprogram cg_vertex,
                                     CGprogram cg_fragment,
                                     EffectGL* effect_gl,
                                     const ParamObjectList& param_objects) {
  ScanVaryingParameters(cg_vertex, CG_PROGRAM, param_cache_gl);
  ScanVaryingParameters(cg_vertex, CG_GLOBAL, param_cache_gl);
  ScanUniformParameters(semantic_manager,
                        renderer,
                        cg_vertex,
                        CG_PROGRAM,
                        param_cache_gl,
                        param_objects,
                        effect_gl);
  ScanUniformParameters(semantic_manager,
                        renderer,
                        cg_vertex,
                        CG_GLOBAL,
                        param_cache_gl,
                        param_objects,
                        effect_gl);
  // Do not record varying inputs for a fragment program
  ScanUniformParameters(semantic_manager,
                        renderer,
                        cg_fragment,
                        CG_PROGRAM,
                        param_cache_gl,
                        param_objects,
                        effect_gl);
  ScanUniformParameters(semantic_manager,
                        renderer,
                        cg_fragment,
                        CG_GLOBAL,
                        param_cache_gl,
                        param_objects,
                        effect_gl);
}

// Search the leaf parameters of a CGeffect and it's shaders for parameters
// using cgGetFirstEffectParameter() / cgGetFirstLeafParameter() /
// cgGetNextLeafParameter(). Add the CGparameters found to the parameter
// maps on the DrawElement.
void ParamCacheGL::ScanCgEffectParameters(CGprogram cg_vertex,
                                          CGprogram cg_fragment,
                                          ParamObject* draw_element,
                                          ParamObject* element,
                                          Material* material,
                                          ParamObject* override) {
  DLOG(INFO) << "DrawElementGL ScanCgEffectParameters";
  DLOG_ASSERT(material);
  DLOG_ASSERT(draw_element);
  DLOG_ASSERT(element);
  EffectGL* effect_gl = static_cast<EffectGL*>(material->effect());
  DLOG_ASSERT(effect_gl);
  if (cg_vertex == NULL)  {
    DLOG(ERROR) << "Can't scan an empty Vertex Program for Cg Parameters.";
    return;
  }
  if (cg_fragment == NULL)  {
    DLOG(ERROR) << "Can't scan an empty Fragment Program for Cg Parameters.";
    return;
  }

  uniform_map_.clear();
  varying_map_.clear();
  sampler_map_.clear();
  ParamObjectList param_object_list;
  param_object_list.push_back(override);
  param_object_list.push_back(draw_element);
  param_object_list.push_back(element);
  param_object_list.push_back(material);
  param_object_list.push_back(effect_gl);
  param_object_list.push_back(semantic_manager_->sas_param_object());
  DoScanCgEffectParameters(semantic_manager_,
                           renderer_,
                           this,
                           cg_vertex,
                           cg_fragment,
                           effect_gl,
                           param_object_list);
}

}  // namespace o3d
