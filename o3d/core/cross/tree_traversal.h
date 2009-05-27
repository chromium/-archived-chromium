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


// This file contains the declaration of TreeTraversal.

#ifndef O3D_CORE_CROSS_TREE_TRAVERSAL_H_
#define O3D_CORE_CROSS_TREE_TRAVERSAL_H_

#include <map>
#include <vector>
#include "core/cross/transform.h"
#include "core/cross/render_node.h"
#include "core/cross/draw_context.h"

namespace o3d {

class TransformationContext;
class Shape;

// A TreeTraversal has multiple DrawLists registered with it. Each DrawList has
// a DrawContext registered with it. At render time the TreeTraversal walks the
// transform tree from the transform it's pointing at and for each drawable it
// finds who's matertial matches one of its registered DrawLists it adds that
// drawable to that DrawList.
class TreeTraversal : public RenderNode {
 public:
  typedef SmartPointer<TreeTraversal> Ref;

  // Names of TreeTraversal Params.
  static const char* kTransformParamName;

  // Returns the Transform the TreeTraversal will start traversing from.
  Transform* transform() const {
    return transform_param_->value();
  }

  // Set the Transform the TreeTraversal will start traversing from.
  void set_transform(Transform *transform) {
    transform_param_->set_value(transform);
  }

  virtual void Render(RenderContext *render_context);

  // Registers a DrawList with this TreeTraversal so that when this
  // TreeTraversal traverses its tree materials that use this DrawList will be
  // added though possibly culled by the view frustum of the DrawContext.
  // Note: passing in the same passlist more than once will override the
  // previous drawContext/reset settings for that passlist.
  // Parameters:
  //   draw_list: DrawList to register.
  //   draw_context: DrawContext to use with the DrawList.
  //   reset: true if you want the pass list reset before traversing.
  void RegisterDrawList(DrawList* draw_list,
                        DrawContext* draw_context,
                        bool reset);

  // Unregisters a DrawList with this TreeTraversal.
  // Parameters:
  //   draw_list: DrawList to unregister.
  // Returns:
  //   true if unregistered. false if this draw_list was not registered.
  bool UnregisterDrawList(DrawList* draw_list);

 private:
  explicit TreeTraversal(ServiceLocator* service_locator);

  // DrawContextInfo is used to keep track of some info we need per draw context
  // registered with this TreeTraversal.
  class DrawContextInfo {
   public:
    // This has to exist so std::map operator[] can work but an uninitialized
    // DrawContextInfo is never direcly accessed.
    DrawContextInfo() { }

    DrawContextInfo(DrawContext* draw_context, bool reset)
        : draw_context_(DrawContext::Ref(draw_context)),
          reset_(reset) {
    }

    DrawContext* draw_context() const {
      return draw_context_;
    }

    bool reset() const {
      return reset_;
    }

    const Matrix4& view() const {
      return view_;
    }

    const Matrix4& projection() const {
      return projection_;
    }

    const Matrix4& view_projection() const {
      return view_projection_;
    }

    int cull_depth() const {
      return cull_depth_;
    }

    void set_cull_depth(int depth) {
      cull_depth_ = depth;
    }

    void ResetCullDepth() {
      cull_depth_ = -1;
    }

    bool IsCulled() {
      return cull_depth_ >= 0;
    }

    // Updates the view, projection and viewProjection based on the DrawContext.
    void UpdateViewProjection();

   private:
    DrawContext::Ref draw_context_;
    bool reset_;
    int cull_depth_;
    Matrix4 view_;
    Matrix4 projection_;
    Matrix4 view_projection_;
  };

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Adds an instance of a Shape to the DrawList it's material requests if
  // we are gathering stuff for that DrawList.
  // Parameters:
  //   render_context: Rendering info.
  //   shape: Shape to add to list.
  //   override: Transform used to override params and store param cache.
  //   world_matrix: The worldMatrix for this Shape.
  void AddInstance(RenderContext* render_context,
                   Shape* shape,
                   Transform* override,
                   const Matrix4& world_matrix);

  // Walks a transform, optionally attempts to cull it. If not culled, walks its
  // children and attempts to add its Shapes to the corresponding registered
  // DrawLists.
  // Parameters:
  //   render_context: Rendering info.
  //   transform: Transform to walk.
  //   depth: depth we've walked so far.
  //   num_non_culled_draw_contexts: How many contexts we have left to process.
  void WalkTransform(RenderContext* render_context,
                     Transform* transform,
                     int depth,
                     int num_non_culled_draw_contexts);

  // Sets the standard parameters on the client so that Param chains might
  // get valid values.
  void SetStandardParameters(const Matrix4& world,
                             const Matrix4& world_view_projection,
                             DrawContextInfo* draw_context_info);

  // The Transform that we start to traverse.
  ParamTransform::Ref transform_param_;

  // The DrawList we will use when traversing and which context to apply to
  // them while traversing.
  typedef std::map<DrawList::Ref, DrawContextInfo> DrawListDrawContextInfoMap;
  DrawListDrawContextInfoMap draw_list_draw_context_info_map_;

  // An Array of pointers to DrawContextInfos indexed by DrawList::global_index
  // both for quick lookup AND the corresponding entry is NULL then we are not
  // processing that DrawList.
  typedef std::vector<DrawContextInfo*> DrawContextInfoArray;
  DrawContextInfoArray draw_context_infos_by_draw_list_global_index_;

  TransformationContext* transformation_context_;

  bool standard_params_have_been_set_;  // true if standard params
                                        // have been set.

  O3D_DECL_CLASS(TreeTraversal, RenderNode);
  DISALLOW_COPY_AND_ASSIGN(TreeTraversal);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_TREE_TRAVERSAL_H_
