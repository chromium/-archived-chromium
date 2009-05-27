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


// This file implements class ClassManager.

#include "core/cross/precompile.h"
#include "core/cross/bitmap.h"
#include "core/cross/bounding_box.h"
#include "core/cross/buffer.h"
#include "core/cross/canvas.h"
#include "core/cross/canvas_paint.h"
#include "core/cross/canvas_shader.h"
#include "core/cross/class_manager.h"
#include "core/cross/clear_buffer.h"
#include "core/cross/counter.h"
#include "core/cross/curve.h"
#include "core/cross/draw_context.h"
#include "core/cross/draw_pass.h"
#include "core/cross/effect.h"
#include "core/cross/function.h"
#include "core/cross/material.h"
#include "core/cross/matrix4_axis_rotation.h"
#include "core/cross/matrix4_composition.h"
#include "core/cross/matrix4_scale.h"
#include "core/cross/matrix4_translation.h"
#include "core/cross/param_array.h"
#include "core/cross/param_operation.h"
#include "core/cross/primitive.h"
#include "core/cross/render_surface_set.h"
#include "core/cross/sampler.h"
#include "core/cross/shape.h"
#include "core/cross/skin.h"
#include "core/cross/standard_param.h"
#include "core/cross/state_set.h"
#include "core/cross/stream.h"
#include "core/cross/stream_bank.h"
#include "core/cross/transform.h"
#include "core/cross/tree_traversal.h"
#include "core/cross/viewport.h"

namespace o3d {

ClassManager::ClassManager(ServiceLocator* service_locator)
    : service_locator_(service_locator),
      service_(service_locator_, this) {
  // Params
  AddTypedClass<ParamBoolean>();
  AddTypedClass<ParamBoundingBox>();
  AddTypedClass<ParamDrawContext>();
  AddTypedClass<ParamDrawList>();
  AddTypedClass<ParamEffect>();
  AddTypedClass<ParamFloat>();
  AddTypedClass<ParamFloat2>();
  AddTypedClass<ParamFloat3>();
  AddTypedClass<ParamFloat4>();
  AddTypedClass<ParamFunction>();
  AddTypedClass<ParamInteger>();
  AddTypedClass<ParamMaterial>();
  AddTypedClass<ParamMatrix4>();
  AddTypedClass<ParamParamArray>();
  AddTypedClass<ParamRenderSurface>();
  AddTypedClass<ParamRenderDepthStencilSurface>();
  AddTypedClass<ParamSampler>();
  AddTypedClass<ParamSkin>();
  AddTypedClass<ParamState>();
  AddTypedClass<ParamStreamBank>();
  AddTypedClass<ParamString>();
  AddTypedClass<ParamTexture>();
  AddTypedClass<ParamTransform>();

  // Param operations.
  AddTypedClass<Matrix4AxisRotation>();
  AddTypedClass<Matrix4Composition>();
  AddTypedClass<Matrix4Scale>();
  AddTypedClass<Matrix4Translation>();
  AddTypedClass<ParamOp2FloatsToFloat2>();
  AddTypedClass<ParamOp3FloatsToFloat3>();
  AddTypedClass<ParamOp4FloatsToFloat4>();
  AddTypedClass<ParamOp16FloatsToMatrix4>();
  AddTypedClass<TRSToMatrix4>();

  // SAS Params
  AddTypedClass<WorldParamMatrix4>();
  AddTypedClass<WorldInverseParamMatrix4>();
  AddTypedClass<WorldTransposeParamMatrix4>();
  AddTypedClass<WorldInverseTransposeParamMatrix4>();

  AddTypedClass<ViewParamMatrix4>();
  AddTypedClass<ViewInverseParamMatrix4>();
  AddTypedClass<ViewTransposeParamMatrix4>();
  AddTypedClass<ViewInverseTransposeParamMatrix4>();

  AddTypedClass<ProjectionParamMatrix4>();
  AddTypedClass<ProjectionInverseParamMatrix4>();
  AddTypedClass<ProjectionTransposeParamMatrix4>();
  AddTypedClass<ProjectionInverseTransposeParamMatrix4>();

  AddTypedClass<WorldViewParamMatrix4>();
  AddTypedClass<WorldViewInverseParamMatrix4>();
  AddTypedClass<WorldViewTransposeParamMatrix4>();
  AddTypedClass<WorldViewInverseTransposeParamMatrix4>();

  AddTypedClass<ViewProjectionParamMatrix4>();
  AddTypedClass<ViewProjectionInverseParamMatrix4>();
  AddTypedClass<ViewProjectionTransposeParamMatrix4>();
  AddTypedClass<ViewProjectionInverseTransposeParamMatrix4>();

  AddTypedClass<WorldViewProjectionParamMatrix4>();
  AddTypedClass<WorldViewProjectionInverseParamMatrix4>();
  AddTypedClass<WorldViewProjectionTransposeParamMatrix4>();
  AddTypedClass<WorldViewProjectionInverseTransposeParamMatrix4>();

  // Other Objects.
  AddTypedClass<Canvas>();
  AddTypedClass<CanvasLinearGradient>();
  AddTypedClass<CanvasPaint>();
  AddTypedClass<ClearBuffer>();
  AddTypedClass<Counter>();
  AddTypedClass<Curve>();
  AddTypedClass<DrawContext>();
  AddTypedClass<DrawElement>();
  AddTypedClass<DrawList>();
  AddTypedClass<DrawPass>();
  AddTypedClass<Effect>();
  AddTypedClass<FunctionEval>();
  AddTypedClass<IndexBuffer>();
  AddTypedClass<Material>();
  AddTypedClass<ParamArray>();
  AddTypedClass<ParamObject>();
  AddTypedClass<Primitive>();
  AddTypedClass<RenderFrameCounter>();
  AddTypedClass<RenderNode>();
  AddTypedClass<RenderSurfaceSet>();
  AddTypedClass<Sampler>();
  AddTypedClass<SecondCounter>();
  AddTypedClass<Shape>();
  AddTypedClass<Skin>();
  AddTypedClass<SkinEval>();
  AddTypedClass<SourceBuffer>();
  AddTypedClass<State>();
  AddTypedClass<StateSet>();
  AddTypedClass<StreamBank>();
  AddTypedClass<Texture2D>();
  AddTypedClass<TextureCUBE>();
  AddTypedClass<TickCounter>();
  AddTypedClass<Transform>();
  AddTypedClass<TreeTraversal>();
  AddTypedClass<VertexBuffer>();
  AddTypedClass<Viewport>();
}

void ClassManager::AddClass(const ObjectBase::Class* object_class,
                            ObjectCreateFunc function) {
  DLOG_ASSERT(object_class_info_name_map_.find(object_class->name()) ==
              object_class_info_name_map_.end())
      << "attempt to register duplicate class name";
  object_class_info_name_map_.insert(
      std::make_pair(object_class->name(),
                     ObjectClassInfo(object_class, function)));
  DLOG_ASSERT(object_creator_class_map_.find(object_class) ==
              object_creator_class_map_.end())
      << "attempt to register duplicate class";
  object_creator_class_map_.insert(std::make_pair(object_class,
                                                  function));
}

const ObjectBase::Class* ClassManager::GetClassByClassName(
    const String& class_name) const {
  ObjectClassInfoNameMap::const_iterator iter =
      object_class_info_name_map_.find(class_name);

  if (iter == object_class_info_name_map_.end()) {
    // Try adding the o3d namespace prefix
    String prefixed_class_name(O3D_STRING_CONSTANT("") + class_name);
    iter = object_class_info_name_map_.find(prefixed_class_name);
  }

  return (iter != object_class_info_name_map_.end()) ?
      iter->second.class_type() : NULL;
}

bool ClassManager::ClassNameIsAClass(
    const String& derived_class_name,
    const ObjectBase::Class* base_class) const {
  const ObjectBase::Class* derived_class = GetClassByClassName(
      derived_class_name);
  return derived_class && ObjectBase::ClassIsA(derived_class, base_class);
}

// Factory method to create a new object by class name.

ObjectBase::Ref ClassManager::CreateObject(const String& type_name) {
  ObjectClassInfoNameMap::const_iterator iter =
      object_class_info_name_map_.find(type_name);
  if (iter == object_class_info_name_map_.end()) {
    // Try adding the o3d namespace prefix
    String prefixed_type_name(O3D_STRING_CONSTANT("") + type_name);
    iter = object_class_info_name_map_.find(prefixed_type_name);
  }

  if (iter != object_class_info_name_map_.end()) {
    return ObjectBase::Ref(iter->second.creation_func()(service_locator_));
  }

  return ObjectBase::Ref();
}

// Factory method to create a new object by class.

ObjectBase::Ref ClassManager::CreateObjectByClass(
    const ObjectBase::Class* object_class) {
  ObjectCreatorClassMap::const_iterator iter =
      object_creator_class_map_.find(object_class);

  if (iter != object_creator_class_map_.end()) {
    return ObjectBase::Ref(iter->second(service_locator_));
  }

  return ObjectBase::Ref();
}

std::vector<const ObjectBase::Class*> ClassManager::GetAllClasses() const {
  std::vector<const ObjectBase::Class*> classes;
  for (ObjectClassInfoNameMap::const_iterator it =
           object_class_info_name_map_.begin();
       it != object_class_info_name_map_.end(); ++it) {
    classes.push_back(it->second.class_type());
  }
  return classes;
}

}  // namespace o3d
