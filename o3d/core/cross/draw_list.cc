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


// This file contains the definition of DrawList.

#include "core/cross/precompile.h"
#include "core/cross/draw_list.h"
#include "core/cross/transformation_context.h"
#include "core/cross/draw_list_manager.h"
#include "core/cross/renderer.h"
#include "core/cross/material.h"
#include "core/cross/element.h"
#include "core/cross/draw_element.h"
#include "core/cross/render_context.h"

namespace o3d {

O3D_DEFN_CLASS(DrawList, NamedObject);
O3D_DEFN_CLASS(ParamDrawList, RefParamBase);

// A DrawElementInfo is used to render an individual DrawElement
// with a particular set of matrices.
class DrawElementInfo {
 public:
  DrawElementInfo()
      : element_(NULL),
        draw_element_(NULL),
        material_(NULL),
        override_(NULL) {
  }

  void Set(const Matrix4& world,
           const Matrix4& world_view_projection,
           DrawElement* draw_element,
           Element* element,
           Material* material,
           ParamObject* override,
           ParamCache* param_cache) {
    world_ = world;
    world_view_projection_ = world_view_projection;
    draw_element_ = draw_element;
    element_ = element;
    material_ = material;
    override_ = override;
    param_cache_ = param_cache;
    priority_ = element->priority();
    effect_ = material->effect();
    state_ = material->state();
    // TODO: Since I'm pulling effect and state out here it would be
    //    a small optmization to use these values in rendering since it's
    //    possible they were pulled from a param chain. Still, it's unlikely
    //    effect or state will have a complicted chain.
  }
  inline float priority() const {
    return priority_;
  }
  inline float z_value() const {
    return z_value_;
  }
  inline Effect* effect() const {
    return effect_;
  }
  inline State* state() const {
    return state_;
  }
  void ComputeZValue(TransformationContext* transformation_context) {
    if (element_->ParamsUsedByZSortHaveInputConnetions()) {
      transformation_context->set_world(world_);
      transformation_context->set_world_view_projection(world_view_projection_);
    }
    Float3 z_sort_point = element_->z_sort_point();
    z_value_ = (world_view_projection_ * Point3(z_sort_point[0],
                                                z_sort_point[1],
                                                z_sort_point[2])).getZ();
  }
  void Render(RenderContext* render_context,
              TransformationContext* transformation_context) {
    transformation_context->set_world(world_);
    transformation_context->set_world_view_projection(world_view_projection_);
    render_context->renderer()->RenderElement(element_,
                                              draw_element_,
                                              material_,
                                              override_,
                                              param_cache_);
  }

 private:
  Matrix4 world_;
  Matrix4 world_view_projection_;
  Element* element_;
  DrawElement* draw_element_;
  Material* material_;
  ParamObject* override_;
  ParamCache* param_cache_;
  float priority_;  // pulled out for sorting
  float z_value_;  // pulled out for sorting
  Effect* effect_;  // pulled out for sorting
  State* state_;  // pulled out for sorting
};


DrawList::DrawList(ServiceLocator* service_locator)
    : NamedObject(service_locator),
      transformation_context_(service_locator->
          GetService<TransformationContext>()),
      view_(Matrix4::identity()),
      projection_(Matrix4::identity()),
      top_draw_element_info_(0),
      global_index_(0),
      weak_pointer_manager_(this) {
  DrawListManager* draw_list_manager =
      service_locator->GetService<DrawListManager>();
  DCHECK(draw_list_manager);
  global_index_ = draw_list_manager->RegisterDrawList(this);
}

DrawList::~DrawList() {
  // delete all pass elements.
  DrawElementInfoArray::iterator end(draw_element_infos_.end());
  for (DrawElementInfoArray::iterator it(draw_element_infos_.begin());
       it != end;
       ++it) {
    delete (*it);
  }

  // Free our global index.
  DrawListManager* draw_list_manager =
      service_locator()->GetService<DrawListManager>();
  DCHECK(draw_list_manager);
  draw_list_manager->UnregisterDrawList(this);
}

ObjectBase::Ref DrawList::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new DrawList(service_locator));
}

void DrawList::Reset(const Matrix4& view, const Matrix4& projection) {
  view_ = view;
  projection_ = projection;
  top_draw_element_info_ = 0;
}

void DrawList::AddDrawElement(DrawElement* draw_element,
                              Element* element,
                              Material* material,
                              ParamObject* override,
                              ParamCache* param_cache,
                              const Matrix4& world,
                              const Matrix4& world_view_projection) {
  // DrawElementInfos only get created once and then reused forever. Then never
  // get freed until the DrawList gets destroyed. This saves lots of
  // allocations/deallocation that would otherwise happen every frame.

  // If I instead used size() then I would have to clear() draw_element_infos_
  // each frame which in turn would mean I'd need to keep a separate container
  // of DrawElementInfos to keep track of the pass elements I'm keeping around
  // and pulling them out of that container and putting them in this one.

  // Instead I just keep using the ones in this container and use
  // top_draw_element_info_ to track the highest used DrawElementInfo.
  while (top_draw_element_info_ >= draw_element_infos_.size()) {
    draw_element_infos_.push_back(new DrawElementInfo());
  }
  DrawElementInfo* pass_element =
      draw_element_infos_[top_draw_element_info_++];
  pass_element->Set(world,
                    world_view_projection,
                    draw_element,
                    element,
                    material,
                    override,
                    param_cache);
}

inline static bool CompareByPriority(const DrawElementInfo* lhs,
                                     const DrawElementInfo* rhs) {
  return lhs->priority() < rhs->priority();
}

inline static bool CompareByZValue(const DrawElementInfo* lhs,
                                   const DrawElementInfo* rhs) {
  return lhs->z_value() > rhs->z_value();
}

inline static bool CompareByPerformance(const DrawElementInfo* lhs,
                                        const DrawElementInfo* rhs) {
  int effectDif = lhs->effect() - rhs->effect();

  if (effectDif != 0) {
    return effectDif < 0;
  }

  return (lhs->state() - rhs->state()) < 0;
}

void DrawList::Render(RenderContext* render_context,
                      SortMethod sort_method) {
  if (top_draw_element_info_ > 0) {
    // Set the view and projection to what they where when these draw elements
    // were put on this draw list.
    transformation_context_->set_view(view_);
    transformation_context_->set_projection(projection_);
    transformation_context_->set_view_projection(
        transformation_context_->projection() *
        transformation_context_->view());

    switch (sort_method) {
      case BY_Z_ORDER: {
        // Compute a Z value for each entry
        for (unsigned ii = 0; ii < top_draw_element_info_; ++ii) {
          draw_element_infos_[ii]->ComputeZValue(transformation_context_);
        }
        std::sort(draw_element_infos_.begin(),
                  draw_element_infos_.begin() + top_draw_element_info_,
                  CompareByZValue);
        break;
      }
      case BY_PRIORITY:
        std::sort(draw_element_infos_.begin(),
                  draw_element_infos_.begin() + top_draw_element_info_,
                  CompareByPriority);
        break;
      default:  // BY_PERFORMANCE
        std::sort(draw_element_infos_.begin(),
                  draw_element_infos_.begin() + top_draw_element_info_,
                  CompareByPerformance);
        break;
    }

    // TODO: Since the ViewProjection never changes for this entire
    //    list we could optmize by storing it in the client and changing
    //    the SAS stuff to use that one.
    for (unsigned ii = 0; ii < top_draw_element_info_; ++ii) {
      draw_element_infos_[ii]->Render(render_context, transformation_context_);
    }
  }
}

ObjectBase::Ref ParamDrawList::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamDrawList(service_locator, false, false));
}
}  // namespace o3d
