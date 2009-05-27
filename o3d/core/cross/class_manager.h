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


#ifndef O3D_CORE_CROSS_CLASS_MANAGER_H_
#define O3D_CORE_CROSS_CLASS_MANAGER_H_

#include <map>
#include <vector>

#include "core/cross/service_implementation.h"
#include "core/cross/iclass_manager.h"

namespace o3d {

// Maintains a collection of Class objects. Allows Classes to be retreived
// by name and objects of those Classes to be created.
class ClassManager : public IClassManager {
 public:
  explicit ClassManager(ServiceLocator* service_locator);

  virtual void AddClass(const ObjectBase::Class* class_type,
                        ObjectCreateFunc function);

  virtual const ObjectBase::Class* GetClassByClassName(
      const String& class_name) const;

  virtual bool ClassNameIsAClass(const String& derived_class_name,
                                 const ObjectBase::Class* base_class) const;

  virtual ObjectBase::Ref CreateObjectByClass(
      const ObjectBase::Class* object_class);

  virtual ObjectBase::Ref CreateObject(const String& type_name);

  std::vector<const ObjectBase::Class*> GetAllClasses() const;

 private:
  ServiceLocator* service_locator_;
  ServiceImplementation<IClassManager> service_;

  // A class to hold data about a class in the ObjectClassInfoNameMap;

  class ObjectClassInfo {
   public:

    ObjectClassInfo(const ObjectBase::Class* class_type,
                    ObjectCreateFunc func)
        : class_type_(class_type),
          creation_func_(func) { }

    const ObjectBase::Class* class_type() const {
      return class_type_;
    }

    ObjectCreateFunc creation_func() const {
      return creation_func_;
    }

   private:
    const ObjectBase::Class* class_type_;
    const ObjectCreateFunc creation_func_;
  };

  // A map by string of ObjectClassInfos.
  typedef std::map<const String, ObjectClassInfo> ObjectClassInfoNameMap;
  // A map by ObjectBase::Class of Object creations functions.
  typedef std::map<const ObjectBase::Class*,
                   ObjectCreateFunc> ObjectCreatorClassMap;

  // ObjectClassInfo by name.
  ObjectClassInfoNameMap object_class_info_name_map_;

  // ObjectClassInfo by class.
  ObjectCreatorClassMap object_creator_class_map_;
};
}

#endif  // O3D_CORE_CROSS_CLASS_MANAGER_H_
