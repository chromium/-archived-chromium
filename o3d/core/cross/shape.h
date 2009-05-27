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


// This file contains declarations for the Shape class.

#ifndef O3D_CORE_CROSS_SHAPE_H_
#define O3D_CORE_CROSS_SHAPE_H_

#include <vector>
#include "core/cross/material.h"
#include "core/cross/element.h"

namespace o3d {

class Pack;

// TODO: A Shape is something made of Primitives. What would a HeightMap
// be made of? It seems like Shape should be based on something like class
// Drawable that is a little more abstract/virtual so that we can easily add
// things later like SkinnedShape or NurbsSurface etc..

// The Shape represents a collection of Elements. The typical example is a
// cube with 6 faces where each face uses a different material would be
// represented as 1 Shape with 6 Elements, one for each material.
class Shape : public ParamObject {
 public:
  typedef SmartPointer<Shape> Ref;

  // Gets an Array of Elements in this shape.
  // Returns:
  //   ElementArray of elements in this shape.
  ElementArray GetElements() const;

  void SetElements(const ElementArray& elements);

  // Gets a direct const reference to the elements owned by this shape.
  // Returns:
  //   const reference to the elements in this shape.
  const ElementRefArray& GetElementRefs() const {
    return elements_;
  }

  // Creates DrawElements for each Element owned by this Shape. If an Element
  // already has a DrawElement that uses material a new DrawElement will not be
  // created.
  // Parameters:
  //   pack: pack used to manage created DrawElements. material: material to use
  //   for each DrawElement. If you pass NULL it
  //      will use the material on the element to which a draw element is being
  //      added. In other words, a DrawPrimtive will use the material of the
  //      corresponidng Element if material is NULL. This allows you to easily
  //      setup the default (just draw as is) by passing NULL or setup a shadow
  //      pass by passing in a shadow material.
  void CreateDrawElements(Pack* pack,
                          Material* material);

  // Adds an Element to this shape. This is an internal function and should not
  // be called directly. use Element::SetOwner
  // Parameters:
  //   element: Element to add.
  void AddElement(Element* element);

  // Removes an element from this Shape. This is an internal function and should
  // not be called directly. use Element::SetOwner
  // Parameters:
  //   element: Element to remove.
  // Returns:
  //   true if successful, false if element was not owned by this shape.
  bool RemoveElement(Element* element);

 private:
  explicit Shape(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // The elements of this Shape.
  ElementRefArray elements_;

  O3D_DECL_CLASS(Shape, ParamObject)
  DISALLOW_COPY_AND_ASSIGN(Shape);
};  // Shape

typedef std::vector<Shape*> ShapeArray;
typedef std::vector<Shape::Ref> ShapeRefArray;

}  // namespace o3d

#endif  // O3D_CORE_CROSS_SHAPE_H_
