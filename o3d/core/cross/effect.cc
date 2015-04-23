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


// This file contains the definition of the Effect class.

#include "core/cross/precompile.h"
#include <cctype>
#include <algorithm>
#include "core/cross/effect.h"
#include "core/cross/param_array.h"
#include "core/cross/renderer.h"
#include "core/cross/types.h"
#include "core/cross/error.h"

namespace o3d {

namespace {
struct EffectToUpper {
  char operator() (char c) const  { return std::toupper(c); }
};
}

EffectParameterInfo::EffectParameterInfo(
    const String& name,
    const ObjectBase::Class* class_type,
    int num_elements,
    const String& semantic,
    const ObjectBase::Class* sas_class_type)
      : name_(name),
        class_type_(class_type),
        num_elements_(num_elements),
        semantic_(semantic),
        sas_class_type_(sas_class_type) {
  // Apparently CG uppercases the semantics so we need to do the same
  // in D3D.
  std::transform(semantic_.begin(),
                 semantic_.end(),
                 semantic_.begin(),
                 EffectToUpper());
}

O3D_DEFN_CLASS(Effect, ParamObject);
O3D_DEFN_CLASS(ParamEffect, RefParamBase);

const char* Effect::kVertexShaderEntryPointPrefix =
    "// #o3d VertexShaderEntryPoint ";
const char* Effect::kFragmentShaderEntryPointPrefix =
    "// #o3d PixelShaderEntryPoint ";
const char* Effect::kMatrixLoadOrderPrefix =
    "// #o3d MatrixLoadOrder ";

Effect::Effect(ServiceLocator* service_locator)
    : ParamObject(service_locator),
      weak_pointer_manager_(this),
      matrix_load_order_(ROW_MAJOR) {
}

void Effect::CreateUniformParameters(ParamObject* param_object) {
  CreateSpecifiedParameters(param_object, false);
}

void Effect::CreateSASParameters(ParamObject* param_object) {
  CreateSpecifiedParameters(param_object, true);
}

// Creates parameters on a ParamObject corresponding to the internal effect
// parameters.
void Effect::CreateSpecifiedParameters(ParamObject* param_object, bool sas) {
  String errors;
  EffectParameterInfoArray param_infos;
  GetParameterInfo(&param_infos);
  for (unsigned ii = 0; ii < param_infos.size(); ++ii) {
    const EffectParameterInfo& param_info = param_infos[ii];
    if ((param_info.sas_class_type() != NULL) == sas) {
      Param* param = param_object->GetUntypedParam(param_info.name());
      if (param) {
        // Param exists. Is the the correct type?
        if (!param->IsA(param_info.class_type())) {
          // Remove it
          if (!param_object->RemoveParam(param)) {
            errors += "Could not remove param '" + param->name() +
                      "' type '" + param->GetClassName() +
                      "' on '" + param_object->name() +
                      "' while trying to replace it with param of type '" +
                      param_info.class_type()->name() + "' for Effect '" +
                      name() + "'";
          } else {
            param = NULL;
          }
        }
      }
      if (!param) {
        const ObjectBase::Class* type = param_info.class_type();
        if (param_info.num_elements() == 0) {
          // Non-array type
          param = param_object->CreateParamByClass(
              param_info.name(),
              param_info.sas_class_type() ? param_info.sas_class_type() :
                                            param_info.class_type());
        } else {
          // Array type
          param =
              param_object->CreateParam<ParamParamArray>(param_info.name());
        }
        if (!param) {
          errors += String(errors.empty() ? "" : "\n") +
                    String("Could not create Param '") + param_info.name() +
                    String("' type '") + String(type->name()) +
                    String(" for Effect '") + name() + "'";
        }
      }
    }
  }
  if (errors.length()) {
    O3D_ERROR(service_locator()) << errors;
  }
}

namespace {

String::size_type GetEndOfIdentifier(const String& original,
                                     String::size_type start) {
  if (start < original.size()) {
    // check that first character is alpha or '_'
    if (isalpha(original[start]) || original[start] == '_') {
      String::size_type end = original.size();
      String::size_type position = start;
      while (position < end) {
        char c = original[position];
        if (!isalnum(c) && c != '_') {
          break;
        }
        ++position;
      }
      return position;
    }
  }
  return String::npos;
}

bool GetIdentifierAfterString(const String& original,
                              const String& phrase,
                              String* word) {
  String::size_type position = original.find(phrase);
  if (position == String::npos) {
    return false;
  }

  // Find end of identifier
  String::size_type start = position + phrase.size();
  String::size_type end = GetEndOfIdentifier(original, start);
  if (end != start && end != String::npos) {
    *word = String(original, start, end - start);
    return true;
  }
  return false;
}

}  // anonymous namespace

// TODO(gman): Replace this with the runtime shader parser.
//    For now it's very stupid. It requires the word "techinque" not appear
//    anywhere inside the file. Then it searches for
//
//    // #o3d VertexShaderEntryPoint name
//    // #o3d PixelShaderEntryPoint name
//
//    with that exact syntax. No extra whitespace.
//    If it doesn't find both it's a fails.
bool Effect::ValidateFX(const String& effect,
                        String* vertex_shader_entry_point,
                        String* fragment_shader_entry_point,
                        MatrixLoadOrder* matrix_load_order) {
  if (!GetIdentifierAfterString(effect,
                                kVertexShaderEntryPointPrefix,
                                vertex_shader_entry_point)) {
    O3D_ERROR(service_locator()) << "Failed to find \""
                                 << kVertexShaderEntryPointPrefix
                                 << "\" in Effect:" << effect;
    return false;
  }
  if (!GetIdentifierAfterString(effect,
                                kFragmentShaderEntryPointPrefix,
                                fragment_shader_entry_point)) {
    O3D_ERROR(service_locator()) << "Failed to find \""
                                 << kFragmentShaderEntryPointPrefix
                                 << "\" in Effect";
    return false;
  }
  String matrix_load_order_str;
  if (!GetIdentifierAfterString(effect,
                                kMatrixLoadOrderPrefix,
                                &matrix_load_order_str)) {
    O3D_ERROR(service_locator()) << "Failed to find \""
                                 << kMatrixLoadOrderPrefix
                                 << "\" in Effect";
    return false;
  }
  bool column_major = !strcmp(matrix_load_order_str.c_str(), "ColumnMajor");
  *matrix_load_order = column_major ? COLUMN_MAJOR : ROW_MAJOR;

  return true;
}

ObjectBase::Ref Effect::Create(ServiceLocator* service_locator) {
  Renderer* renderer = service_locator->GetService<Renderer>();
  if (NULL == renderer) {
    O3D_ERROR(service_locator) << "No Render Device Available";
    return ObjectBase::Ref();
  }
  return ObjectBase::Ref(renderer->CreateEffect());
}

ObjectBase::Ref ParamEffect::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamEffect(service_locator, false, false));
}
}  // namespace o3d
