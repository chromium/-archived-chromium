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


// This file cotnains the declaration of Element.

#ifndef O3D_CORE_CROSS_ELEMENT_H_
#define O3D_CORE_CROSS_ELEMENT_H_

#include <vector>
#include "core/cross/param_object.h"
#include "core/cross/bounding_box.h"
#include "core/cross/ray_intersection_info.h"
#include "core/cross/material.h"
#include "core/cross/draw_element.h"
#include "core/cross/state.h"

namespace o3d {

class RenderContext;
class Renderer;
class Pack;
class Shape;
class RayIntersectionInfo;
class BoundingBox;
class ParamCache;

typedef std::vector<String> StringArray;
typedef std::vector<DrawElement::Ref> DrawElementRefArray;

// The Element is an abstract base class. It's purpose is to manage
// DrawElements for things inherited from Element.
class Element : public ParamObject {
 public:
  typedef SmartPointer<Element> Ref;

  virtual ~Element();

  static const char* kMaterialParamName;
  static const char* kBoundingBoxParamName;
  static const char* kZSortPointParamName;
  static const char* kPriorityParamName;
  static const char* kCullParamName;

  // Returns true if any params used during tree traversal have input
  // connections.
  inline bool ParamsUsedByTreeTraversalHaveInputConnections() {
    return cull_param_ref_->input_connection() != NULL ||
        bounding_box_param_ref_->input_connection() != NULL;
  }

  // Returns true if any params used for z sort have input connections.
  inline bool ParamsUsedByZSortHaveInputConnetions() {
    return z_sort_point_param_ref_->input_connection() != NULL;
  }

  // Returns the Material object bound to the Element.
  Material* material() const {
    return material_param_ref_->value();
  }

  // Binds an Material object to the Material.
  void set_material(Material* material) {
    material_param_ref_->set_value(material);
  }

  // Returns the BoundingBox of this Element.
  BoundingBox bounding_box() const {
    return bounding_box_param_ref_->value();
  }

  // Sets the BoundingBox used to cull this Element.
  void set_bounding_box(const BoundingBox& bounding_box) {
    bounding_box_param_ref_->set_value(bounding_box);
  }

  // Returns the z sort point of this Element.
  Float3 z_sort_point() const {
    return z_sort_point_param_ref_->value();
  }

  // Sets the point used to zsort this Element if the DrawPass is set to sort by
  // z order.
  void set_z_sort_point(const Float3& z_sort_point) {
    z_sort_point_param_ref_->set_value(z_sort_point);
  }

  // Returns the priority of this Element
  float priority() const {
    return priority_param_ref_->value();
  }

  // Sets the priority used to sort this Element if the DrawPass is set to sort
  // by priority.
  void set_priority(float priority) {
    priority_param_ref_->set_value(priority);
  }

  // Returns the cull setting of this Element. true = attempt to cull by
  // bounding box, false = do not attempt to cull.
  bool cull() const {
    return cull_param_ref_->value();
  }

  // Sets the cull setting for this Element. true = attempt to cull by
  // bounding box, false = do not attempt to cull
  void set_cull(bool cull) {
    cull_param_ref_->set_value(cull);
  }

  // Sets the owner of this Element. Passing in NULL will remove this
  // element from having an owner.
  // Parameters:
  //   shape: Shape that will own this Element.
  void SetOwner(Shape* shape);

  // Gets the current owner of this Element.
  // Returns:
  //   A pointer to the owner of this element.
  Shape* owner() {
    return owner_;
  }

  // Render this Element.
  virtual void Render(Renderer* renderer,
                      DrawElement* draw_element,
                      Material* material,
                      ParamObject* param_object,
                      ParamCache* param_cache) = 0;

  // Adds a DrawElement to this Element.
  // This is an internal function. Use DrawElement::SetOwner.
  // Parameter:
  //   draw_element: DrawElement to add.
  void AddDrawElement(DrawElement* draw_element);

  // Removes a DrawElement from this Element.
  // This is an internal function. Use DrawElement::SetOwner.
  // Parameters:
  //   draw_element: DrawElement to remove.
  // Returns:
  //   True if removed. False if there this draw element was not on this
  //   Element.
  bool RemoveDrawElement(DrawElement* draw_element);

  // Gets all the DrawElements under this Element.
  // Returns:
  //   Array of raw pointers to DrawElements.
  DrawElementArray GetDrawElements() const;

  // for this Element. Note that unlike Shape::CreateDrawElements and
  // Transform::CreateDrawElements this one will create more than one element
  // for the same material.
  // Parameters:
  //   pack: pack used to manage created DrawElements.
  //   material: material to use for the created DrawElement. If you pass NULL
  //      it will use the material on this Element. This allows you to easily
  //      setup the default (just draw as is) by passing NULL or setup a shadow
  //      pass by passing in a shadow material.
  // Returns:
  //   The DrawElement created.
  DrawElement* CreateDrawElement(Pack* pack, Material* material);

  // Gets a direct const reference to all the DrawPrimtives under this
  // Element.
  // Returns:
  //   Array of refs to DrawElements.
  const DrawElementRefArray& GetDrawElementRefs() const {
    return draw_elements_;
  }

  // Computes the intersection of a ray in the same coordinate system as
  // the specified POSITION stream.
  // Parameters:
  //   position_stream_index: Index of POSITION stream.
  //   cull: which side of the triangles to ignore.
  //   start: position of start of ray in local space. end: position of end of
  //   ray. in local space. result: pointer to ray intersection info to store
  //       the result. If result->valid() is false then something was wrong.
  //       Check IErrorStatus::GetLastError(). If result->intersected() is true
  //       then the ray intersected a something. result->position() is the exact
  //       point of intersection.
  virtual void IntersectRay(int position_stream_index,
                            State::Cull cull,
                            const Point3& start,
                            const Point3& end,
                            RayIntersectionInfo* result) const = 0;

  // Computes the bounding box in same coordinate system as the specified
  // POSITION stream.
  // Parameters:
  //   position_stream_index: Index of POSITION stream.
  //   result: A pointer to boundingbox to store the result.
  virtual void GetBoundingBox(int position_stream_index,
                              BoundingBox* result) const = 0;

 protected:
  explicit Element(ServiceLocator* service_locator);

 private:
  ParamMaterial::Ref material_param_ref_;  // Material to render with.
  ParamFloat3::Ref z_sort_point_param_ref_;  // Point to zsort by.
  ParamFloat::Ref priority_param_ref_;  // Point to priority sort by.
  ParamBoundingBox::Ref bounding_box_param_ref_;  // Bounding box to cull by.
  ParamBoolean::Ref cull_param_ref_;  // Culling on or off.

  // Draw elements under this Element.
  DrawElementRefArray draw_elements_;

  // The Shape we are currnetly owned by.
  Shape* owner_;

  O3D_DECL_CLASS(Element, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(Element);
};

typedef std::vector<Element*> ElementArray;
typedef std::vector<Element::Ref> ElementRefArray;

}  // namespace o3d

#endif  // O3D_CORE_CROSS_ELEMENT_H_
