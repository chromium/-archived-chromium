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


// This file contains the definition of the Shape class.

#include "core/cross/precompile.h"
#include "core/cross/shape.h"
#include "core/cross/param_object.h"
#include "core/cross/render_node.h"

namespace o3d {

O3D_DEFN_CLASS(Shape, ParamObject)

Shape::Shape(ServiceLocator* service_locator)
    : ParamObject(service_locator) {
}

ObjectBase::Ref Shape::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new Shape(service_locator));
}

// Adds a element do this shape.
void Shape::AddElement(Element* element) {
  elements_.push_back(Element::Ref(element));
}

// Removes a element from this Shape.
bool Shape::RemoveElement(Element* element) {
  ElementRefArray::iterator iter = std::find(elements_.begin(),
                                             elements_.end(),
                                             Element::Ref(element));
  if (iter != elements_.end()) {
    elements_.erase(iter);
    return true;
  }
  return false;
}

// Gets an Array of elements in this shape.
ElementArray Shape::GetElements() const {
  ElementArray elements;
  elements.reserve(elements_.size());
  std::copy(elements_.begin(),
            elements_.end(),
            std::back_inserter(elements));
  return elements;
}

void Shape::SetElements(const ElementArray& elements) {
  elements_.resize(elements.size());
  for (int i = 0; i != elements.size(); ++i) {
    elements_[i] = Element::Ref(elements[i]);
  }
}

namespace {
class FindByMaterial {
 public:
  explicit FindByMaterial(Material* material) : material_(material) { }

  bool operator() (const DrawElement* draw_element) {
    return draw_element->material() == material_;
  }
 private:
  Material* material_;
};
}

void Shape::CreateDrawElements(Pack* pack,
                               Material* material) {
  for (unsigned pp = 0; pp < elements_.size(); ++pp) {
    Element* element = elements_[pp].Get();
    const DrawElementRefArray& draw_elements = element->GetDrawElementRefs();
    if (std::find_if(draw_elements.begin(),
                     draw_elements.end(),
                     FindByMaterial(material)) == draw_elements.end()) {
      elements_[pp]->CreateDrawElement(pack, material);
    }
  }
}

}  // namespace o3d
