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


// This file contains the declaration of DrawList.

#ifndef O3D_CORE_CROSS_DRAW_LIST_H_
#define O3D_CORE_CROSS_DRAW_LIST_H_

#include <vector>
#include "core/cross/param.h"
#include "core/cross/types.h"

namespace o3d {

class TransformationContext;
class DrawElement;
class DrawElementInfo;
class Element;
class Material;
class ParamObject;
class ParamCache;
class RenderContext;

// A DrawList is a list of things to render. It is filled out by a TreeTraversal
// and is rendered by a DrawPass. A single DrawList can be filled out / added to
// by multiple TreeTraversals. It can also be rendered by multiple DrawPasses.
class DrawList : public NamedObject {
 public:
  typedef SmartPointer<DrawList> Ref;
  typedef WeakPointer<DrawList> WeakPointerType;

  ~DrawList();

  enum SortMethod {
    BY_PERFORMANCE = 0,
    BY_Z_ORDER = 1,
    BY_PRIORITY = 2,
  };

  // Resets the DrawList to have no elements.
  // Parameters:
  //   view: View Matrix to use when drawing this list.
  //   projection: Projection Matrix to use when drawing this list.
  void Reset(const Matrix4& view, const Matrix4& projection);

  // Adds a DrawElement to this DrawList.
  // Parameters:
  //   draw_element: DrawElement for this DrawElement.
  //   primitive: Primitive for this DrawElement.
  //   material: Material to render this DrawElement.
  //   override: ParamObject to override parameters for this DrawElement.
  //   world: World Matrix to render this DrawElement.
  //   world_view_projection: World View Projection Matrix to render this
  //       DrawElement.
  void AddDrawElement(DrawElement* draw_element,
                      Element* element,
                      Material* material,
                      ParamObject* override,
                      ParamCache* param_cache,
                      const Matrix4& world,
                      const Matrix4& world_view_projection);

  // Render the elements of this DrawList.
  void Render(RenderContext* render_context, SortMethod sort_method);

  // Return the global index for this DrawList.
  unsigned int global_index() {
    return global_index_;
  }

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

 private:
  explicit DrawList(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // We store draw element infos by pointer so they are easy to sort.
  typedef std::vector<DrawElementInfo*> DrawElementInfoArray;

  TransformationContext* transformation_context_;

  // The view matrix that was used when this list was filled out.
  Matrix4 view_;

  // The projection matrix that was used when this list was filled out.
  Matrix4 projection_;

  // Array to hold draw elements.
  DrawElementInfoArray draw_element_infos_;

  // The top (next to be used) draw element ifno.
  unsigned int top_draw_element_info_;

  // Index of this draw list in the client for quick lookup.
  unsigned int global_index_;

  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  O3D_DECL_CLASS(DrawList, NamedObject);
  DISALLOW_COPY_AND_ASSIGN(DrawList);
};

class ParamDrawList : public TypedRefParam<DrawList> {
 public:
  typedef SmartPointer<ParamDrawList> Ref;

  ParamDrawList(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedRefParam<DrawList>(service_locator, dynamic, read_only) {
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamDrawList, RefParamBase);
  DISALLOW_COPY_AND_ASSIGN(ParamDrawList);
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_DRAW_LIST_H_
