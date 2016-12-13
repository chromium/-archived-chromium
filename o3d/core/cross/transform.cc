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


// This file contains the definition of the Transform class.

#include "core/cross/precompile.h"
#include "core/cross/transform.h"
#include "core/cross/renderer.h"
#include "core/cross/error.h"

namespace o3d {

namespace {

// Returns true if the child transform is already an ancestor of the
// parent_transform argument.
// Note:  Routine will infinite loop if cycles exist in the Transform graph.
bool ParentingIntroducesCycle(Transform* child_transform,
                              Transform* parent_transform) {
  if (!parent_transform || !child_transform) {
    return false;
  }

  // Break the recursion if the parent and child are the same transform.
  if (child_transform == parent_transform) {
    return true;
  }

  // Otherwise, recurse on each of the children of the child_transform.
  const TransformRefArray& children(child_transform->GetChildrenRefs());
  TransformRefArray::const_iterator children_iter(children.begin()),
      end(children.end());
  for (; children_iter != end; ++children_iter) {
    if (ParentingIntroducesCycle(*children_iter, parent_transform)) {
      return true;
    }
  }

  return false;
}

void GetTransformsInTreeRecursive(const Transform* tree_root,
                                  TransformArray* children) {
  children->push_back(const_cast<Transform*>(tree_root));
  const TransformRefArray& children_array = tree_root->GetChildrenRefs();
  TransformRefArray::const_iterator children_iter(children_array.begin()),
      children_end(children_array.end());
  for (; children_iter != children_end; ++children_iter) {
    GetTransformsInTreeRecursive(*children_iter, children);
  }
}

}  // end unnamed namespace

O3D_DEFN_CLASS(Transform, ParamObject);
O3D_DEFN_CLASS(ParamTransform, RefParamBase);

const char* Transform::kLocalMatrixParamName =
    O3D_STRING_CONSTANT("localMatrix");
const char* Transform::kWorldMatrixParamName =
    O3D_STRING_CONSTANT("worldMatrix");
const char* Transform::kVisibleParamName =
    O3D_STRING_CONSTANT("visible");
const char* Transform::kBoundingBoxParamName =
    O3D_STRING_CONSTANT("boundingBox");
const char* Transform::kCullParamName =
    O3D_STRING_CONSTANT("cull");

Transform::Transform(ServiceLocator* service_locator)
    : ParamObject(service_locator),
      parent_(NULL),
      param_cache_manager_(service_locator->GetService<Renderer>()),
      weak_pointer_manager_(this) {
  RegisterParamRef(kLocalMatrixParamName, &local_matrix_param_ref_);
  SlaveParamMatrix4::RegisterParamRef(kWorldMatrixParamName,
                                      &world_matrix_param_ref_,
                                      this);
  RegisterParamRef(kVisibleParamName, &visible_param_ref_);
  RegisterParamRef(kBoundingBoxParamName, &bounding_box_param_ref_);
  RegisterParamRef(kCullParamName, &cull_param_ref_);

  set_visible(true);
}

Transform::~Transform() {
  // Sets any children to have no parent.
  TransformArray children(GetChildren());
  TransformArray::iterator end(children.end());
  for (TransformArray::iterator iter(children.begin());
       iter != end;
       ++iter) {
    (*iter)->SetParent(NULL);
  }
}

void Transform::UpdateOutputs() {
  Matrix4 world_matrix;
  if (world_matrix_param_ref_->input_connection() == NULL) {
    if (parent_) {
      world_matrix_param_ref_->set_dynamic_value(parent_->world_matrix() *
                                                 local_matrix());
    } else {
      world_matrix_param_ref_->set_dynamic_value(local_matrix());
    }
  }
}

ObjectBase::Ref Transform::Create(ServiceLocator* service_locator) {
  Renderer* renderer = service_locator->GetService<Renderer>();
  if (NULL == renderer) {
    O3D_ERROR(service_locator) << "No Render Device Available";
    return ObjectBase::Ref();
  }
  return ObjectBase::Ref(new Transform(service_locator));
}

// Returns a (read-only) array of immediate children for that transform.
TransformArray Transform::GetChildren() const {
  // TODO: Unify the system behaviour when working with arrays of refs and
  // raw pointers to avoid costly conversions.
  TransformArray children_array;
  children_array.reserve(child_array_.size());
  TransformRefArray::const_iterator child_iter(child_array_.begin()),
      child_end(child_array_.end());
  for (; child_iter != child_end; ++child_iter) {
    children_array.push_back(child_iter->Get());
  }

  return children_array;
}

// Removes a child transform from the child_array_.  Returns true if the
// operation succeeds.
bool Transform::RemoveChild(Transform *child) {
  TransformRefArray::iterator end = remove(child_array_.begin(),
                                           child_array_.end(),
                                           Transform::Ref(child));

  // child should never be in the child array more than once.
  DLOG_ASSERT(std::distance(end, child_array_.end()) <= 1);

  // The child was never found.
  if (end == child_array_.end())
    return false;

  // We need to do this before erasing the child since the child may get
  // destroyed when we erase its reference below.
  child->world_matrix_param_ref_->DecrementNotCachableCountOnParamChainForInput(
      world_matrix_param_ref_);

  // Actually remove the child from the child array.
  child_array_.erase(end, child_array_.end());

  return true;
}

// Adds a child to the transform.  Always returns true.
bool Transform::AddChild(Transform* child) {
  child->world_matrix_param_ref_->IncrementNotCachableCountOnParamChainForInput(
      world_matrix_param_ref_);
  child_array_.push_back(Transform::Ref(child));
  return true;
}

// Return the transforms in a given subtree.  This method performs a depth-first
// traversal of the tree to get the results
TransformArray Transform::GetTransformsInTree() const {
  TransformArray tree_transforms;
  GetTransformsInTreeRecursive(this, &tree_transforms);
  return tree_transforms;
}

// Functor object for comparing Transform names.
namespace {
class CompareTransformName {
 public:
  explicit CompareTransformName(const String& n) : test_name_(n) {}
  bool operator()(Transform*& transform) {
    return transform->name() == test_name_;
  }
 private:
  String test_name_;
};
}

// Search for transforms by name in the subtree below this transform including
// this transform.
TransformArray Transform::GetTransformsByNameInTree(const String& name) const {
  // Get all the transforms in the subtree
  TransformArray transforms = GetTransformsInTree();
  // Partition the container into two sections depending on whether the
  // transform name matches the desired name.
  TransformArray::iterator i =
      std::partition(transforms.begin(),
                     transforms.end(),
                     CompareTransformName(name));
  // delete the transforms that don't match the name.
  transforms.erase(i, transforms.end());
  return transforms;
}

// Sets the parent of a transform. Calling "transform->SetParent(NULL)"
// disconnects the parent-child relationship if one already exists, and does
// nothing if the transform has no parent.
void Transform::SetParent(Transform *new_parent) {
  // Ensure that this parent assignment does not create a cycle in the
  // transform graph.
  if (ParentingIntroducesCycle(this, new_parent)) {
    O3D_ERROR(service_locator())
        << "Cannot set parent as it creates a cycle";
    return;
  }

  // Creates a temporary reference to ourselves because if our current parent
  // holds the only reference to us then we'll get deleted the moment we call
  // RemoveChild. This temporary reference will let go automatically when
  // the function exits.
  Transform::Ref temp_reference(this);

  // First check if the transform already has a parent.  If it does then
  // remove it from its current parent first.
  if (parent_ != NULL) {
    bool removed = parent_->RemoveChild(this);
    DLOG_ASSERT(removed);
    if (!removed)
      return;
  }

  // If we are just unparenting the transform then we are done.
  if (new_parent == NULL) {
    parent_ = NULL;
    return;
  }

  // Add the transform as a child of its new parent.
  parent_ = new_parent;
  bool added = new_parent->AddChild(this);
  DLOG_ASSERT(added);

  // If we failed to add the child to the parent then leave the child transform
  // an orphan in order to avoid any inconsistencies in the scenegraph
  if (!added)
    parent_ = NULL;
}

// Explicitly calculates and returns the world matrix.  The world matrix
// is computed as the product of the parent world matrix and the local matrix
// unless there's an input bind to the WorldMatrix param in which case
// we return the value from the bind.
Matrix4 Transform::GetUpdatedWorldMatrix() {
  if (world_matrix_param_ref_->input_connection() != NULL) {
    return world_matrix_param_ref_->value();
  } else {
    Matrix4 world_matrix;
    if (parent()) {
      world_matrix = parent()->GetUpdatedWorldMatrix() * local_matrix();
    } else {
      world_matrix = local_matrix();
    }
    world_matrix_param_ref_->set_dynamic_value(world_matrix);
    return world_matrix;
  }
}

// Adds a shape do this transform.
void Transform::AddShape(Shape* shape) {
  shape_array_.push_back(Shape::Ref(shape));
}

// Removes a shape from this transform.
bool Transform::RemoveShape(Shape* shape) {
  ShapeRefArray::iterator iter = std::find(shape_array_.begin(),
                                           shape_array_.end(),
                                           Shape::Ref(shape));
  if (iter != shape_array_.end()) {
    shape_array_.erase(iter);
    return true;
  }
  return false;
}

// Gets an Array of shapes in this transform.
ShapeArray Transform::GetShapes() const {
  ShapeArray shapes;
  shapes.reserve(shape_array_.size());
  std::copy(shape_array_.begin(),
            shape_array_.end(),
            std::back_inserter(shapes));
  return shapes;
}

void Transform::SetShapes(const ShapeArray& shapes) {
  shape_array_.resize(shapes.size());
  for (int i = 0; i != shapes.size(); ++i) {
    shape_array_[i] = Shape::Ref(shapes[i]);
  }
}

void Transform::CreateDrawElements(Pack* pack, Material* material) {
  for (unsigned cc = 0; cc < child_array_.size(); ++cc) {
    child_array_[cc]->CreateDrawElements(pack, material);
  }
  for (unsigned ss = 0; ss < shape_array_.size(); ++ss) {
    shape_array_[ss]->CreateDrawElements(pack, material);
  }
}

void Transform::ConcreteGetInputsForParam(const Param* param,
                                          ParamVector* inputs) const {
  // If it's the world matrix it's affected by our local matrix and our parent
  // unless the world matrix has an input_connection.
  if (param == world_matrix_param_ref_) {
    if (world_matrix_param_ref_->input_connection() == NULL) {
      inputs->push_back(local_matrix_param_ref_);
      if (parent()) {
        inputs->push_back(parent()->world_matrix_param_ref_);
      }
    }
  }
}

void Transform::ConcreteGetOutputsForParam(const Param* param,
                                           ParamVector* outputs) const {
  // If it's the local matrix then it affects the world matrix unless the world
  // matrix has an input_connection.
  if (param == local_matrix_param_ref_) {
    if (world_matrix_param_ref_->input_connection() == NULL) {
      outputs->push_back(world_matrix_param_ref_);
    }
  } else if (param == world_matrix_param_ref_) {
    // If it's the world matrix it affects all the children's world matrices
    // unless they have an input connection.
    for (unsigned ii = 0; ii < child_array_.size(); ++ii) {
      Transform* child = child_array_[ii];
      Param* param = child->world_matrix_param_ref_;
      if (param->input_connection() == NULL) {
        outputs->push_back(param);
      }
    }
  }
}

ObjectBase::Ref ParamTransform::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamTransform(service_locator, false, false));
}

}  // namespace o3d
