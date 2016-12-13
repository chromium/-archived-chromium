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


#ifndef O3D_CORE_CROSS_ICLASS_MANAGER_H_
#define O3D_CORE_CROSS_ICLASS_MANAGER_H_

#include <vector>

#include "core/cross/service_locator.h"
#include "core/cross/object_base.h"

namespace o3d {

class ServiceLocator;

// A pointer to a function that creates an object derived from NamedObject.
typedef ObjectBase::Ref (*ObjectCreateFunc)(ServiceLocator* service_locator);

// Maintains a collection of Class objects. Allows Classes to be retreived
// by name and objects of those Classes to be created.
class IClassManager {
 public:
  static const InterfaceId kInterfaceId;

  IClassManager() {}
  virtual ~IClassManager() {}

  // Registers a new Object creation function by string so that CreateObject can
  // create this new type.
  // Paramters:
  //   class_type: Class of type as provided by ObjectBase::GetClass.
  //   function: A function that creates an object of that type.
  virtual void AddClass(const ObjectBase::Class* class_type,
                        ObjectCreateFunc function) = 0;

  // A typesafe version of AddClass. It guarantees that the creator and
  // class match. (Seems like this would be better as a macro since there would
  // be no code generation).
  template <typename T>
  void AddTypedClass() {
    AddClass(T::GetApparentClass(), T::Create);
  }

  // Returns the ObjectBase::Class for a particular class name. It only works
  // for classes that have been registered through AddClass.
  // Parameters:
  //   class_name: name of the class to look for.
  // Returns:
  //   ObjectBase::Class* for the given class name or NULL if there is no match.
  virtual const ObjectBase::Class* GetClassByClassName(
      const String& class_name) const = 0;

  // Returns true if class_name is or is derived from base_class_type. It only
  // works for classes that have been registered through AddClass.
  // Parameters:
  //   derived_class_name: Class name of derived class.
  //   base_class: Class of type to check for.
  // Returns:
  //   true if derived_class_name is or is derived from base_class.
  virtual bool ClassNameIsAClass(
      const String& derived_class_name,
      const ObjectBase::Class* base_class) const = 0;

  // Creates an Object by Class. This is an internal function. Do not use
  // directly.
  // Parameters:
  //   object_class: ObjectBase::Class* to type of class to create.
  // Returns:
  //   ObjectBase::Ref to created object.
  virtual ObjectBase::Ref CreateObjectByClass(
      const ObjectBase::Class* object_class) = 0;

  // Creates an Object by Class name. This is an internal function. Do not use
  // directly.
  // Parameters:
  //   type_name: Name of the Class to create.
  // Returns:
  //   ObjectBase::Ref to created object.
  virtual ObjectBase::Ref CreateObject(const String& type_name) = 0;

  // Get all the classes registered in the class manager.
  virtual std::vector<const ObjectBase::Class*>
      GetAllClasses() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(IClassManager);
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_ICLASS_MANAGER_H_
