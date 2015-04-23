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


// This file contains the definition of the ObjectBase class.

#include "core/cross/precompile.h"
#include "core/cross/object_base.h"
#include "core/cross/service_locator.h"
#include "core/cross/object_manager.h"
#include "core/cross/id_manager.h"

namespace o3d {

ObjectBase::Class ObjectBase::class_ = {
  O3D_STRING_CONSTANT("ObjectBase"), NULL
};

const char* ObjectBase::Class::unqualified_name() const {
  if (strncmp(
      name_,
      O3D_NAMESPACE O3D_NAMESPACE_SEPARATOR,
      sizeof(O3D_NAMESPACE) + sizeof(O3D_NAMESPACE_SEPARATOR) - 2) == 0) {
    return name_ + sizeof(O3D_NAMESPACE) + sizeof(O3D_NAMESPACE_SEPARATOR) - 2;
  }
  return name_;
}

ObjectBase::ObjectBase(ServiceLocator *service_locator)
    : id_(IdManager::CreateId()),
      service_locator_(service_locator) {
  // Upon object construction, register this object with the object manager
  // to allow for central lookup.
  ObjectManager* object_manager = service_locator_->GetService<ObjectManager>();
  if (object_manager) {
    object_manager->RegisterObject(this);
  }
}

ObjectBase::~ObjectBase() {
  // Upon destruction, unregister this object from the owning client.
  ObjectManager* object_manager = service_locator_->GetService<ObjectManager>();
  if (object_manager) {
    object_manager->UnregisterObject(this);
  }
}

bool ObjectBase::ClassIsA(const Class *derived, const Class *base) {
  // find base in the hierarchy of derived
  for (; derived; derived = derived->parent()) {
    if (derived == base) return true;
  }
  return false;
}

bool ObjectBase::ClassIsAClassName(const Class *derived, const String& name) {
  // find base in the hierarchy of derived
  for (; derived; derived = derived->parent()) {
    if (name.compare(derived->name()) == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace o3d
