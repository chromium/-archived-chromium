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


#include "core/cross/precompile.h"
#include "core/cross/object_manager.h"
#include "core/cross/named_object.h"
#include "core/cross/pack.h"
#include "core/cross/client.h"

namespace o3d {

const InterfaceId ObjectManager::kInterfaceId =
    InterfaceTraits<ObjectManager>::kInterfaceId;

ObjectManager::ObjectManager(ServiceLocator* service_locator)
    : service_locator_(service_locator),
      service_(service_locator_, this) {
}

ObjectManager::~ObjectManager() {
  // Free all the packs.
  pack_array_.clear();

  DLOG_ASSERT(object_map_.empty()) << "Client node leak.";
}

std::vector<ObjectBase*> ObjectManager::GetObjects(
    const String& name,
    const String& class_type_name) const {
  ObjectBaseArray objects;
  ObjectMap::const_iterator end(object_map_.end());
  for (ObjectMap::const_iterator iter(object_map_.begin());
       iter != end;
       ++iter) {
    if (iter->second->IsAClassName(class_type_name) &&
        iter->second->IsA(NamedObjectBase::GetApparentClass()) &&
        down_cast<NamedObjectBase*>(iter->second)->name().compare(name) == 0) {
      objects.push_back(iter->second);
    }
  }
  return objects;
}

std::vector<ObjectBase*> ObjectManager::GetObjectsByClassName(
    const String& class_type_name) const {
  ObjectBaseArray objects;
  ObjectMap::const_iterator end(object_map_.end());
  for (ObjectMap::const_iterator iter(object_map_.begin());
       iter != end;
       ++iter) {
    if (iter->second->IsAClassName(class_type_name)) {
      objects.push_back(iter->second);
    }
  }
  return objects;
}

ObjectBase *ObjectManager::GetObjectBaseById(
    Id id,
    const ObjectBase::Class *type) const {
  ObjectMap::const_iterator object_find(object_map_.find(id));
  ObjectBase* object = NULL;
  if (object_find != object_map_.end()) {
    object = object_find->second;
  }

  if (object && !object->IsA(type)) {
    object = NULL;
  }

  return object;
}

void ObjectManager::RegisterObject(ObjectBase *object) {
  DLOG_ASSERT(object_map_.find(object->id()) == object_map_.end())
      << "attempt to register duplicate id in client";
  object_map_.insert(std::make_pair(object->id(), object));
}

void ObjectManager::UnregisterObject(ObjectBase *object) {
  ObjectMap::iterator object_find(object_map_.find(object->id()));
  DLOG_ASSERT(object_find != object_map_.end())
      << "Unregistering an unknown object.";

  if (object_find != object_map_.end()) {
    object_map_.erase(object_find);
  }
}

bool ObjectManager::DestroyPack(Pack* pack) {
  PackRefArray::iterator iter = find(pack_array_.begin(),
                                     pack_array_.end(),
                                     pack);
  DLOG_ASSERT(iter != pack_array_.end()) << "Destruction of unknown pack.";
  if (iter != pack_array_.end()) {
    pack_array_.erase(iter);
    return true;
  }
  return false;
}

Pack* ObjectManager::CreatePack() {
  Pack::Ref pack(new Pack(service_locator_));
  if (pack) {
    pack_array_.push_back(pack);
  }
  return pack;
}

void ObjectManager::DestroyAllPacks() {
  pack_array_.clear();
}
}  // namespace o3d
