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


// This file contains the definition of Element.

#include "core/cross/precompile.h"
#include "core/cross/element.h"
#include "core/cross/error.h"
#include "core/cross/pack.h"
#include "core/cross/shape.h"

namespace o3d {

const char* Element::kMaterialParamName =
    O3D_STRING_CONSTANT("material");
const char* Element::kBoundingBoxParamName =
    O3D_STRING_CONSTANT("boundingBox");
const char* Element::kPriorityParamName =
    O3D_STRING_CONSTANT("priority");
const char* Element::kZSortPointParamName =
    O3D_STRING_CONSTANT("zSortPoint");
const char* Element::kCullParamName =
    O3D_STRING_CONSTANT("cull");

O3D_DEFN_CLASS(Element, ParamObject);

Element::Element(ServiceLocator* service_locator)
    : ParamObject(service_locator),
      owner_(NULL) {
  RegisterParamRef(kMaterialParamName, &material_param_ref_);
  RegisterParamRef(kBoundingBoxParamName, &bounding_box_param_ref_);
  RegisterParamRef(kPriorityParamName, &priority_param_ref_);
  RegisterParamRef(kZSortPointParamName, &z_sort_point_param_ref_);
  RegisterParamRef(kCullParamName, &cull_param_ref_);
}

Element::~Element() {
}

void Element::SetOwner(Shape* new_owner) {
  // Hold a ref to ourselves so we make sure we don't get deleted while
  // as we remove ourself from our current owner.
  Element::Ref temp(this);

  if (owner_ != NULL) {
    bool removed = owner_->RemoveElement(this);
    DLOG_ASSERT(removed);
  }

  owner_ = new_owner;

  if (new_owner) {
    new_owner->AddElement(this);
  }
}

// Adds a DrawElement to this Element and associates it to a group name.

void Element::AddDrawElement(DrawElement* draw_element) {
  DCHECK(draw_element);
  draw_elements_.push_back(DrawElement::Ref(draw_element));
}

// Removes a DrawElement from this Element by group name.
bool Element::RemoveDrawElement(DrawElement* draw_element) {
  DrawElementRefArray::iterator iter = std::find(
      draw_elements_.begin(),
      draw_elements_.end(),
      DrawElement::Ref(draw_element));
  if (iter != draw_elements_.end()) {
    draw_elements_.erase(iter);
    return true;
  }
  return false;
}

// Gets all the DrawPrimtives under this Element.
DrawElementArray Element::GetDrawElements() const {
  DrawElementArray draw_elements;
  draw_elements.reserve(draw_elements_.size());
  std::copy(draw_elements_.begin(),
            draw_elements_.end(),
            std::back_inserter(draw_elements));
  return draw_elements;
}

DrawElement* Element::CreateDrawElement(Pack* pack, Material* material) {
  DrawElement* draw_element = pack->Create<DrawElement>();
  draw_element->set_material(material);
  draw_element->SetOwner(this);
  return draw_element;
}
}  // namespace o3d
