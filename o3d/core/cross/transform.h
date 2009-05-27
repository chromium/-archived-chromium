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


// This file contains declarations for TransformParamMatrix4 and Transform.

#ifndef O3D_CORE_CROSS_TRANSFORM_H_
#define O3D_CORE_CROSS_TRANSFORM_H_

#include <vector>
#include "core/cross/param_object.h"
#include "core/cross/param.h"
#include "core/cross/types.h"
#include "core/cross/shape.h"
#include "core/cross/param_cache.h"
#include "core/cross/bounding_box.h"

namespace o3d {

class Transform;
class Client;
class Pack;

// Array container for Transform pointers.
typedef std::vector<Transform*> TransformArray;

// Iterators for TransformArray.
typedef TransformArray::const_iterator TransformArrayConstIterator;
typedef TransformArray::iterator TransformArrayIterator;

// The Transform defines a single 4x4 matrix transformational element. A
// Transform can have one or no parents and an arbitrary number of children.
// The Transform stores two transformation matrices, the local_matrix matrix
// and the world_matrix which caches the the transformation between the local
// coordinate system and the world (root) coordinate system when the Transform
// belongs to the scenegraph.
class Transform : public ParamObject {
  friend class Client;
 public:
  typedef SmartPointer<Transform> Ref;
  typedef WeakPointer<Transform> WeakPointerType;

  // Array for Transform references.
  typedef std::vector<Transform::Ref> TransformRefArray;

  // Names of Transform Params.
  static const char* kLocalMatrixParamName;
  static const char* kWorldMatrixParamName;
  static const char* kVisibleParamName;
  static const char* kBoundingBoxParamName;
  static const char* kCullParamName;

  ~Transform();

  // Returns true if any params used during tree traversal have input
  // connections.
  inline bool ParamsUsedByTreeTraversalHaveInputConnections() {
    return cull_param_ref_->input_connection() != NULL ||
        bounding_box_param_ref_->input_connection() != NULL;
  }

  // Sets the parent of the Transform by re-parenting the Transform under
  // parent. Setting parent to NULL removes the Transform and the entire subtree
  // below it from the scenegraph. The operation will fail if the parenting
  // operation will produce a cycle in the scenegraph.
  //
  // NOTE!: If in setting the parent to NULL the transform is no longer
  // referenced by any other transforms and is not referenced by any packs it
  // will be deleted
  //
  // Paremeters:
  //   parent: the new parent or NULL to un-parent the transform
  void SetParent(Transform *parent);

  // Returns the Transform's parent
  // Parameters:
  //   None
  // Returns:
  //   Pointer to the parent Transform of this Transform.
  inline Transform* parent() const {
    return parent_;
  }

  // Returns the visibility for this Transform.
  bool visible() const {
    return visible_param_ref_->value();
  }

  // Sets the visibility for this Transform.
  void set_visible(bool value) {
    visible_param_ref_->set_value(value);
  }

  // Returns the BoundingBox of this Transform.
  BoundingBox bounding_box() const {
    return bounding_box_param_ref_->value();
  }

  // Sets the BoundingBox used to cull this Transform.
  void set_bounding_box(const BoundingBox& bounding_box) {
    bounding_box_param_ref_->set_value(bounding_box);
  }

  // Returns the cull setting of this Transform. true = attempt to cull by
  // bounding box, false = do not attempt to cull.
  bool cull() const {
    return cull_param_ref_->value();
  }

  // Sets the cull setting for this Transform. true = attempt to cull by
  // bounding box, false = do not attempt to cull
  void set_cull(bool cull) {
    cull_param_ref_->set_value(cull);
  }

  // Returns the local transform matrix.
  Matrix4 local_matrix() const {
    return local_matrix_param_ref_->value();
  }

  // Sets the local transform matrix.
  void set_local_matrix(const Matrix4& local_matrix) {
    local_matrix_param_ref_->set_value(local_matrix);
  }

  // Returns the world transform matrix.  The world transformation matrix
  // gets updated each frame by the client if there's a direct path from the
  // scenegraph root to the Transform.
  Matrix4 world_matrix() const {
    return world_matrix_param_ref_->value();
  }

  // Evaluates and returns the current world matrix.
  // The world transform matrix returned is always valid for all transforms
  // which have a path to the scene root.
  // Returns:
  //   World Matrix.
  Matrix4 GetUpdatedWorldMatrix();

  // Returns the immediate children of the Transform.  Note that the returned
  // array consists of raw pointers and is a copy of the internal array.
  TransformArray GetChildren() const;

  // Returns the immediate children of the Transform as an array of refs.
  const TransformRefArray& GetChildrenRefs() const {
    return child_array_;
  }

  // Returns all the transforms in a subtree.
  // Returns this transform and all its descendants. Note that this does not
  // have to be in the transform graph.
  // Returns:
  //  An array containing pointers to the transforms of the subtree
  TransformArray GetTransformsInTree() const;

  // Searches for transforms that match the given name in the hierarchy under
  // and including this transform.  Since there can be more than one transform
  // with a given name, results are returned in an array.
  // Parameters:
  //  name: transform name to look for
  // Returns:
  //  An array of pointers to transforms matching the given name
  TransformArray GetTransformsByNameInTree(const String& name) const;

  // Adds a shape do this transform.
  // Parameters:
  //   shape: Shape to add.
  void AddShape(Shape* shape);

  // Removes a shape from this transform.
  // Parameters:
  //   shape: Shape to remove.
  // Returns:
  //   true if successful, false if shape was not in this transform.
  bool RemoveShape(Shape* shape);

  // Gets an Array of shapes in this transform.
  // Returns:
  //   ShapeArray of shapes in this transform.
  ShapeArray GetShapes() const;

  // Sets the array of shapes for this transform.
  void SetShapes(const ShapeArray& shapes);

  // Gets a direct const reference to the shapes in this transform.
  // Returns:
  //   const reference to the shapes in this transform.
  const ShapeRefArray& GetShapeRefs() const {
    return shape_array_;
  }

  // Walks the tree of transforms starting with this transform and creates
  // draw elements. If an Element already has a DrawElement that uses material a
  // new DrawElement will not be created.
  // Parameters:
  //   pack: pack used to manage created elements.
  //   material: material to use for each element. If you pass NULL it will use
  //      the material on the element to which a draw element is being added.
  //      In other words, a DrawElement will use the material of the
  //      corresponidng Element if material is NULL. This allows you to easily
  //      setup the default (just draw as is) by passing NULL or setup a shadow
  //      pass by passing in a shadow material.
  void CreateDrawElements(Pack* pack,
                          Material* material);

  // Gets the ParamCacheManager for this transform.
  inline ParamCacheManager& param_cache_manager() {
    return param_cache_manager_;
  }

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

  // Update the world Matrix.
  void UpdateOutputs();

 protected:
  // Removes a child transform from the child array. Does not change the child
  // transform's parent.
  virtual bool RemoveChild(Transform* child);

  // Adds a child transform to the child array.  Does not change the child
  // transform's parent.
  virtual bool AddChild(Transform* child);

  // Overridden from ParamObject
  // For the given Param, returns all the inputs that affect that param through
  // this ParamObject.
  virtual void ConcreteGetInputsForParam(const Param* param,
                                         ParamVector* inputs) const;

  // Overridden from ParamObject
  // For the given Param, returns all the outputs that the given param will
  // affect through this ParamObject.
  virtual void ConcreteGetOutputsForParam(const Param* param,
                                          ParamVector* outputs) const;

 private:
  typedef SlaveParam<ParamMatrix4, Transform> SlaveParamMatrix4;

  explicit Transform(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Every transform in the transform graph hierarchy has a Transform as a
  // parent. If parent_ is NULL then the transform is not in the hierarchy,
  Transform *parent_;

  // World (model) matrix which takes into account the entire transformation
  // hierarchy above the transform.
  SlaveParamMatrix4::Ref world_matrix_param_ref_;

  // Local transformation matrix.
  ParamMatrix4::Ref local_matrix_param_ref_;

  // Visible param.
  ParamBoolean::Ref visible_param_ref_;

  // Bounding box to cull by.
  ParamBoundingBox::Ref bounding_box_param_ref_;

  // Culling on or off.
  ParamBoolean::Ref cull_param_ref_;

  // Array of refs to children Transforms for this transform.
  TransformRefArray child_array_;

  // Array of refs to Shapes under this transform
  ShapeRefArray shape_array_;

  // Caches of Params for rendering.
  ParamCacheManager param_cache_manager_;

  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  O3D_DECL_CLASS(Transform, ParamObject)
};  // Transform

class ParamTransform : public TypedRefParam<Transform> {
 public:
  typedef SmartPointer<ParamTransform> Ref;

  ParamTransform(ServiceLocator* service_locator,
                 bool dynamic,
                 bool read_only)
      : TypedRefParam<Transform>(service_locator, dynamic, read_only) {}

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamTransform, RefParamBase)
};

typedef std::vector<Transform::Ref> TransformRefArray;

}  // namespace o3d

#endif  // O3D_CORE_CROSS_TRANSFORM_H_
