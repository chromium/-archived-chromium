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


#ifndef O3D_CORE_CROSS_OBJECT_MANAGER_H_
#define O3D_CORE_CROSS_OBJECT_MANAGER_H_

#include <map>
#include <vector>

#include "core/cross/object_base.h"
#include "core/cross/named_object.h"
#include "core/cross/service_implementation.h"
#include "core/cross/service_dependency.h"
#include "core/cross/types.h"
#include "core/cross/error.h"

namespace o3d {

class Pack;

// Maintains a collection of all objects deriving from ObjectBase. Objects and
// lists of objects can be found based on various criteria.
class ObjectManager {
 public:
  static const InterfaceId kInterfaceId;

  explicit ObjectManager(ServiceLocator* service_locator);
  ~ObjectManager();

  // Disallows pack creation.
  void DisallowPackCreation();

  // Searches in the Client for a base object by its id.  If the dynamic type
  // of the object matches the requested type, then a pointer to the instance
  // is returned.
  // This is an internal function. You should use GetById from C++.
  // Parameters:
  //  id: The id of the ObjectBase to search for type: The class instance
  //  specifier used to filter the return value.
  // Returns:
  //  A pointer to the ObjectBase instance if found and of the matching
  //  dynamic type or NULL otherwise.
  ObjectBase *GetObjectBaseById(Id id, const ObjectBase::Class *type) const;

  // Searches in the Client for an object by its id. This function is for
  // Javascript.
  // Parameters:
  //   id: id of object to look for.
  // Returns:
  //   Pointer to the object or NULL if not found.
  ObjectBase* GetObjectById(Id id) const {
    return GetObjectBaseById(id, ObjectBase::GetApparentClass());
  }

  // Searches by id for an Object created by the Client. To search
  // for an object regardless of type use:
  //   Client::GetById<ObjectBase>(obj_id)
  // To search for an object of a specific type use:
  //   Client::GetById<Type>(obj_id)
  // for example, to search for Transform use:
  //   Client::GetById<Transform>(transform_id)
  // Parameters:
  //  id: Unique id of the object to search for
  // Returns:
  //  Pointer to the object with matching id or NULL if no object is found
  template<class T> T* GetById(Id id) const {
    return static_cast<T *>(GetObjectBaseById(id, T::GetApparentClass()));
  }

  // Get an object by name typesafe. This function is for C++
  // Example:
  //   Buffer* buffer = client->Get<Buffer>("name");
  // Parameters:
  //   name: name of object to search for.
  // Returns:
  //   std::vector of pointers to type of the objects that matched by name.
  template<typename T>
  std::vector<T*> Get(const String& name) const {
    std::vector<T*> objects;
    if (ObjectBase::ClassIsA(T::GetApparentClass(),
                             NamedObject::GetApparentClass())) {
      ObjectMap::const_iterator end(object_map_.end());
      for (ObjectMap::const_iterator iter(object_map_.begin());
           iter != end;
           ++iter) {
        if (iter->second->IsA(T::GetApparentClass())) {
          if (static_cast<NamedObject*>(
              iter->second)->name().compare(name) == 0) {
            objects.push_back(static_cast<T*>(iter->second));
          }
        }
      }
    }
    return objects;
  }
  // Searches the Client for objects of a particular name and type.
  // This function is for Javascript.
  // Parameters:
  //   name: name of object to look for.
  //   type_name: name of class to look for.
  // Returns:
  //   Array of raw pointers to the found objects.
  std::vector<ObjectBase*> GetObjects(const String& name,
                                      const String& type_name) const;

  // Search the client for all objects of a certain class
  // Parameters:
  //  class_type_name: the Class of the object. It is okay to pass base types
  //                   for example Node::GetApparentClass()->name will return
  //                   both Transforms and Shapes.
  // Returns:
  //   Array of Object Pointers.
  std::vector<ObjectBase*> GetObjectsByClassName(
      const String& class_type_name) const;

  // Search the client for all objects of a certain class
  // Returns:
  //   Array of Pointers to the requested class.
  template<typename T>
  std::vector<T*> GetByClass() const {
    std::vector<T*> objects;
    ObjectMap::const_iterator end(object_map_.end());
    for (ObjectMap::const_iterator iter(object_map_.begin());
         iter != end;
         ++iter) {
      if (iter->second->IsA(T::GetApparentClass())) {
        objects.push_back(static_cast<T*>(iter->second));
      }
    }
    return objects;
  }

  void RegisterObject(ObjectBase *object);
  void UnregisterObject(ObjectBase *object);

  // Pack methods --------------------------

  // Removes all internal references to the Pack from the client.
  // The pack, and all objects contained in it are permitted to be destroyed
  // after the packs destruction.  Nodes will only be destoryed after all
  // references to them have been removed.
  // This is an internal function not to be exposed to the external world.
  // Returns:
  //   True if the pack was successfully deleted.
  bool DestroyPack(Pack* pack);

  // Creates a pack object, and registers it within the Client's internal
  // dictionary strutures.  Note that multiple packs may share the same name.
  // The system does not enforce pack name uniqueness.
  // Returns:
  //  A smart-pointer reference to the newly created pack object.
  Pack* CreatePack();

  // Destroys all registered packs.
  void DestroyAllPacks();

  size_t GetNumObjects() const {
    return object_map_.size();
  }

 private:
  typedef std::vector<SmartPointer<Pack> > PackRefArray;

  // Dictionary of Objects indexed by their unique ID
  typedef std::map<Id, ObjectBase*> ObjectMap;

  ServiceLocator* service_locator_;
  ServiceImplementation<ObjectManager> service_;

  // Map of objects to Ids
  ObjectMap object_map_;

  // Array required to maintain references to the currently live pack objects.
  PackRefArray pack_array_;

  DISALLOW_COPY_AND_ASSIGN(ObjectManager);
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_OBJECT_MANAGER_H_
