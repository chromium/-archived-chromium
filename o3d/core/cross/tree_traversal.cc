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


// This file contains the definition of TreeTraveral.

#include "core/cross/precompile.h"
#include "core/cross/tree_traversal.h"
#include "core/cross/shape.h"
#include "core/cross/draw_list.h"
#include "core/cross/transformation_context.h"
#include "core/cross/renderer.h"
#include "core/cross/error.h"

namespace o3d {

O3D_DEFN_CLASS(TreeTraversal, RenderNode);

const char* TreeTraversal::kTransformParamName =
    O3D_STRING_CONSTANT("transform");

ObjectBase::Ref TreeTraversal::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new TreeTraversal(service_locator));
}

TreeTraversal::TreeTraversal(ServiceLocator* service_locator)
    : RenderNode(service_locator),
      transformation_context_(service_locator->
          GetService<TransformationContext>()) {
  RegisterParamRef(kTransformParamName, &transform_param_);
}

void TreeTraversal::RegisterDrawList(
    DrawList* draw_list,
    DrawContext* draw_context,
    bool reset) {
  DCHECK(draw_list);
  DCHECK(draw_context);

  draw_list_draw_context_info_map_[DrawList::Ref(draw_list)] =
      DrawContextInfo(draw_context, reset);
}

bool TreeTraversal::UnregisterDrawList(DrawList* draw_list) {
  return draw_list_draw_context_info_map_.erase(DrawList::Ref(draw_list)) > 0;
}

void TreeTraversal::Render(RenderContext* render_context) {
  // Reset the draw context infos array so we can rebuild it.
  draw_context_infos_by_draw_list_global_index_.clear();

  // Reset any DrawLists that need resetting and set the pass list flags.
  DrawListDrawContextInfoMap::iterator end(
      draw_list_draw_context_info_map_.end());
  for (DrawListDrawContextInfoMap::iterator iter(
           draw_list_draw_context_info_map_.begin());
       iter != end;
       ++iter) {
    DrawContextInfo& draw_context_info = iter->second;
    draw_context_info.ResetCullDepth();

    // Update the view projection matrix so we don't have to compute it more
    // than once later.
    draw_context_info.UpdateViewProjection();

    // If we are supposed to reset the DrawList reset it.
    DrawList* draw_list = iter->first;
    if (draw_context_info.reset()) {
      draw_list->Reset(draw_context_info.view(),
                       draw_context_info.projection());
    }

    // Add it to draw_context_infos_by_draw_list_global_index_ array.
    unsigned global_index = draw_list->global_index();
    if (global_index >= draw_context_infos_by_draw_list_global_index_.size()) {
      draw_context_infos_by_draw_list_global_index_.insert(
          draw_context_infos_by_draw_list_global_index_.end(),
          global_index - draw_context_infos_by_draw_list_global_index_.size() + 1,
          NULL);
    }
    draw_context_infos_by_draw_list_global_index_[global_index] =
        &draw_context_info;
  }

  // At this point draw_context_infos_by_draw_list_global_index_ is big enough
  // to so that there is a directly accessable entry for each DrawList that this
  // TreeTraveral is filling out. In other words. If
  // draw_context_infos_by_draw_list_global_index_[
  //     material->draw_list()->global_index()]
  // is non NULL then we want the DrawElement using that material.

  // Only after clearing any DrawLists do we bail out if there is no transform.
  Transform* transform1 = transform();
  if (!transform1) {
    return;
  }

  // Return if this transform is not visible.
  if (!transform1->visible()) {
    return;
  }

  // Now walk ourselves and all our children.
  WalkTransform(render_context,
                transform1,
                0,
                static_cast<int>(draw_list_draw_context_info_map_.size()));
}

void TreeTraversal::SetStandardParameters(const Matrix4& world,
                                          const Matrix4& world_view_projection,
                                          DrawContextInfo* draw_context_info) {
  transformation_context_->set_world(world);
  transformation_context_->set_view(draw_context_info->view());
  transformation_context_->set_projection(draw_context_info->projection());
  transformation_context_->set_view_projection(
      draw_context_info->view_projection());
  transformation_context_->set_world_view_projection(world_view_projection);
}

void TreeTraversal::WalkTransform(RenderContext* render_context,
                                  Transform* transform,
                                  int depth,
                                  int num_non_culled_draw_contexts) {
  Renderer* renderer = render_context->renderer();
  Matrix4 world = transform->world_matrix();
  Matrix4 world_view_projection;
  bool cull_depth_was_set = false;

  renderer->IncrementTransformsProcessed();

  // Attempt to cull this transform for each draw_context
  DrawListDrawContextInfoMap::iterator end(
      draw_list_draw_context_info_map_.end());
  for (DrawListDrawContextInfoMap::iterator iter(
           draw_list_draw_context_info_map_.begin());
       iter != end;
       ++iter) {
    DrawList* draw_list = iter->first;
    bool have_world_view_projection = false;

    DrawContextInfo* draw_context_info =
        draw_context_infos_by_draw_list_global_index_[
            draw_list->global_index()];
    DLOG_ASSERT(draw_context_info);

    // Before we cull, if the cull or bounding box params have input
    // connections we need to setup the standard params.
    if (transform->ParamsUsedByTreeTraversalHaveInputConnections()) {
      Matrix4 world_view_projection =
          draw_context_info->view_projection() * world;
      have_world_view_projection = true;
      SetStandardParameters(world, world_view_projection, draw_context_info);
    }

    if (transform->cull()) {
      // Are we still processing this draw_context?
      //
      // The way this works is, draw_context_info->cull_depth is set to (-1) for
      // each DrawContextInfo at the start. As we descend the tree, assuming
      // transform->cull is true for a given transform, then if the cull_depth
      // is still (-1) we need to check this transform to see if its boundingbox
      // makes it culled. If that's true then we set the cull_depth to the depth
      // in the tree at which it was culled. That means at any time
      // draw_context_infos_by_draw_list_global_index[draw_list_global_index]->
      //   cull_depth() >= 0 means that draw list has already been completely
      //   culled.
      //
      // Each time we set a cull_depth we decrement
      // num_non_culled_draw_contexts. If it goes to zero then there is no point
      // going any deeper. The entire sub tree is culled for all draw_contexts.
      //
      // On the way back out IF the depth we are at matches a cull_depth then we
      // can clear it back to -1. To help this process we keep a flag that
      // whether or not we set 1 or more cull_depths in (cull_depth_was_set)
      if (!draw_context_info->IsCulled()) {
        if (!have_world_view_projection) {
          world_view_projection = draw_context_info->view_projection() * world;
        }
        // NOTE: Computing the world view projection matrix this way means
        //     that no matter what, we only cull to that worldViewProjection.
        //     In other words the user can not supply is own funky
        //     worldViewProjection for culling using param binds where as he
        //     can supply one for rendering.
        if (!transform->bounding_box().InFrustum(world_view_projection)) {
          renderer->IncrementTransformsCulled();
          --num_non_culled_draw_contexts;
          draw_context_info->set_cull_depth(depth);
          cull_depth_was_set = true;
        }
      }
    }
  }

  if (num_non_culled_draw_contexts > 0) {
    // Process Shapes
    {
      const ShapeRefArray& shapes = transform->GetShapeRefs();
      ShapeRefArray::size_type size = shapes.size();
      for (ShapeRefArray::size_type ii = 0; ii < size; ++ii) {
        AddInstance(render_context, shapes[ii], transform, world);
      }
    }

    // Process all the children
    {
      int children_depth = depth + 1;
      const TransformRefArray& children = transform->GetChildrenRefs();
      TransformRefArray::size_type size = children.size();
      for (TransformRefArray::size_type ii = 0; ii < size; ++ii) {
        if (children[ii]->visible()) {
          WalkTransform(render_context,
                        children[ii],
                        children_depth,
                        num_non_culled_draw_contexts);
        }
      }
    }
  }

  // Reset any cull_depths at our depth
  if (cull_depth_was_set) {
    DrawListDrawContextInfoMap::iterator end(
        draw_list_draw_context_info_map_.end());
    for (DrawListDrawContextInfoMap::iterator iter(
             draw_list_draw_context_info_map_.begin());
         iter != end;
         ++iter) {
      DrawList* draw_list = iter->first;

      DrawContextInfo* draw_context_info =
          draw_context_infos_by_draw_list_global_index_[
              draw_list->global_index()];
      DLOG_ASSERT(draw_context_info);

      if (draw_context_info->cull_depth() == depth) {
        draw_context_info->ResetCullDepth();
        ++num_non_culled_draw_contexts;
      }
    }
  }
}

void TreeTraversal::AddInstance(RenderContext* render_context,
                                Shape* shape,
                                Transform* override,
                                const Matrix4& world) {
  Renderer* renderer = render_context->renderer();
  const ElementRefArray& elements = shape->GetElementRefs();
  ElementRefArray::size_type num_elements = elements.size();
  for (ElementRefArray::size_type ii = 0; ii < num_elements; ++ii) {
    Element* element = elements[ii];
    const DrawElementRefArray& draw_elements =
        element->GetDrawElementRefs();
    DrawElementRefArray::const_iterator end(draw_elements.end());
    for (DrawElementRefArray::const_iterator iter(draw_elements.begin());
         iter != end;
         ++iter) {
      renderer->IncrementDrawElementsProcessed();
      DrawElement* draw_element = iter->Get();

      // We MUST get the cache BEFORE culling so that we usually get the same
      // cache for the same draw element.
      ParamCache* param_cache =
          override->param_cache_manager().GetNextCache(
              render_context->renderer());

      Material* material = draw_element->material();
      if (!material) {
        material = element->material();
      }

      if (!material) {
        continue;
      }

      if (!material->effect()) {
        continue;
      }

      DrawList* draw_list = material->draw_list();
      if (!draw_list) {
        continue;
      }

      // Is this pass list something we want?
      if (draw_list->global_index() >=
          draw_context_infos_by_draw_list_global_index_.size()) {
        continue;
      }

      DrawContextInfo* draw_context_info =
          draw_context_infos_by_draw_list_global_index_[
              draw_list->global_index()];
      if (draw_context_info && !draw_context_info->IsCulled()) {
        Matrix4 world_view_projection(
            draw_context_info->view_projection() * world);
        // Yes it is. Should we attempt to cull it?
        // Before we cull, if the cull or bounding box params have input
        // connections we need to setup the standard params.
        if (element->ParamsUsedByTreeTraversalHaveInputConnections()) {
          SetStandardParameters(world,
                                world_view_projection,
                                draw_context_info);
        }

        if (element->cull()) {
          // NOTE: Computing the world view projection matrix this way means
          //     that no matter what, we only cull to that worldViewProjection.
          //     In other words the user can not supply is own funky
          //     worldViewProjection for culling using param binds.
          if (!element->bounding_box().InFrustum(world_view_projection)) {
            renderer->IncrementDrawElementsCulled();
            continue;
          }
        }
        draw_list->AddDrawElement(draw_element,
                                  element,
                                  material,
                                  override,
                                  param_cache,
                                  world,
                                  world_view_projection);
      }
    }
  }
}

void TreeTraversal::DrawContextInfo::UpdateViewProjection() {
  view_ = draw_context_->view();
  projection_ = draw_context_->projection();
  view_projection_ = projection_ * view_;
}

}  // namespace o3d
