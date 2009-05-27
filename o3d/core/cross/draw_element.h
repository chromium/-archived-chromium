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


// This file cotnains the declaration of DrawElement.

#ifndef O3D_CORE_CROSS_DRAW_ELEMENT_H_
#define O3D_CORE_CROSS_DRAW_ELEMENT_H_

#include <vector>
#include "core/cross/param_object.h"
#include "core/cross/material.h"

namespace o3d {

class Element;

// A DrawElement is what is actually "Drawn". It sits below a Element
// and draws that Element with a different material. You can also override that
// material/effect's params with params directly on the DrawElement.
class DrawElement : public ParamObject {
 public:
  typedef SmartPointer<DrawElement> Ref;

  static const char* kMaterialParamName;

  explicit DrawElement(ServiceLocator* service_locator);
  virtual ~DrawElement();

  // Returns the Material object bound to the DrawElement.
  Material* material() const {
    return material_param_ref_->value();
  }

  // Binds an Material object to the Material.
  void set_material(Material* material) {
    material_param_ref_->set_value(material);
  }

  // Sets the owner for this DrawElement.
  // Parameters:
  //   new_owner: Element to be our new owner. Pass in null to stop being owned.
  void SetOwner(Element* new_owner);

  Element* owner() const {
    return owner_;
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  ParamMaterial::Ref material_param_ref_;  // Material to render with.

  Element* owner_;  // our current owner.

  O3D_DECL_CLASS(DrawElement, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(DrawElement);
};

typedef std::vector<DrawElement*> DrawElementArray;
typedef std::vector<DrawElement::Ref> DrawElementRefArray;

}  // namespace o3d

#endif  // O3D_CORE_CROSS_DRAW_ELEMENT_H_
