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


// This file contains the declaration of the ObjectBase class and definitions
// for the macros used to define the O3D object classes.

#ifndef O3D_CORE_CROSS_OBJECT_BASE_H__
#define O3D_CORE_CROSS_OBJECT_BASE_H__

#include <vector>

#include "core/cross/types.h"
#include "core/cross/smart_ptr.h"

#define O3D_NAMESPACE "o3d"
#define O3D_NAMESPACE_SEPARATOR "."

// Macro to provide a uniform prefix for all string constants created by
// o3d. We reserve this prefix so that we never overwrite a user
// created Transform, RenderNode, Param, Effect, State etc.
#define O3D_STRING_CONSTANT(value)              \
  (O3D_NAMESPACE O3D_NAMESPACE_SEPARATOR value)


// This macro declares the necessary functions for the type mechanism to work.
// It needs to be used in each of the definition of any class that derives from
// ObjectBase.
// CLASS is the class being defined, BASE is its base class.
#define O3D_OBJECT_BASE_DECL_CLASS(CLASS, BASE)                          \
 public:                                                                 \
  static const ObjectBase::Class *GetApparentClass() { return &class_; } \
  static const String GetApparentClassName() {                           \
    return class_.name();                                                \
  }                                                                      \
  virtual const ObjectBase::Class *GetClass() const {                    \
    return GetApparentClass();                                           \
  }                                                                      \
  virtual String GetClassName() const {                                  \
    return GetApparentClass()->name();                                   \
  }                                                                      \
 private:                                                                \
  static ObjectBase::Class class_;

// This macro defines the class descriptor for the type mechanism. It needs to
// be used once in the definition file of any class that derives from
// ObjectBase.
// CLASSNAME is the name to use to identify the class.
// CLASS is the class being defined.
// BASE is its base class.
#define O3D_OBJECT_BASE_DEFN_CLASS(CLASSNAME, CLASS, BASE) \
  ObjectBase::Class CLASS::class_ = { CLASSNAME, BASE::GetApparentClass() };

// This macro declares the necessary functions for the type mechanism to work.
// It needs to be used in each of the definition of any class that derives from
// ObjectBase.
// CLASS is the class being defined, BASE is its base class.
#define O3D_DECL_CLASS(CLASS, BASE) O3D_OBJECT_BASE_DECL_CLASS(CLASS, BASE)

// This macro defines the class descriptor for the type mechanism. It needs to
// be used once in the definition file of any class that derives from
// ObjectBase.
// CLASS is the class being defined, BASE is its base class.
#define O3D_DEFN_CLASS(CLASS, BASE) \
  O3D_OBJECT_BASE_DEFN_CLASS(O3D_STRING_CONSTANT(#CLASS), CLASS, BASE)

namespace o3d {

class ServiceLocator;

// class ObjectBase: base of O3D run-time objects.
// This class provides the basic functionality for all O3D objects, in
// particular so that the JavaScript interface is functional and safe:
// - unique id, as a safe reference to the instance. The id -> instance mapping
// is handled by the Client.
// - simple run-time typeinformation mechanism.  For this to work, the
// O3D_DECL_CLASS and O3D_DEFN_CLASS need to be used in each class
// deriving from ObjectBase.
//
// The type mechanism works as following. Say you have two classes:
//
//   class A: public ObjectBase {
//     O3D_DECL_CLASS(A, ObjectBase)
//   };
//
//   class B: public A {
//     O3D_DECL_CLASS(B, A)
//   };
//
// And then you have:
//
//   A *a = new A();
//   B *b = new B();
//   A *c = new B();
//   ObjectBase::Class *a_class = A::GetApparentClass();
//   ObjectBase::Class *b_class = B::GetApparentClass();
//
// Then:
//
// a->GetClass() will return a_class - meaning 'a' is a 'A'
// b->GetClass() will return b_class - meaning 'b' is a 'B'
// c->GetClass() will return b_class - meaning 'c' is a 'B', even though the
//   apparent type of 'c' is 'A *'.
// a->IsA(a_class) is true.
// a->IsA(b_class) is false.
// b->IsA(a_class) is true ('b' is a 'B' that derives from 'A')
// c->IsA(b_class) is true ('c' is a 'B', even though its apparent type is
//   'A *')
//
// You can also test types descriptors themselves:
//
// ObjectBase::ClassIsA(a_class, b_class) is false ('A' doesn't derive from 'B')
// ObjectBase::ClassIsA(b_class, a_class) is true ('B' derives from 'A')
class ObjectBase : public RefCounted {
 public:
  typedef SmartPointer<ObjectBase> Ref;

  // Structure describing the class. A single instance of this struct exists for
  // each class deriving from ObjectBase. Note: This is a struct with all public
  // members because we need to be able to statically define them but we have
  // accessors because we want it treated like a class by the code.
  struct Class {
   public:
    const Class* parent() const {
      return parent_;
    }

    const char* name() const {
      return name_;
    }

    const char* unqualified_name() const;

   public:
    // The name of the class.
    const char *name_;
    // The base class descriptor.
    const Class *parent_;
  };

  explicit ObjectBase(ServiceLocator* service_locator);
  virtual ~ObjectBase();

  // Return the owning client for this object.
  ServiceLocator* service_locator() const {
    return service_locator_;
  }

  // Returns the unique id of the instance.
  const Id id() const {
    return id_;
  }

  // Returns the class descriptor for this class.
  static const Class *GetApparentClass() {
    return &class_;
  }

  // Returns whether a class derives from a base class.
  static bool ClassIsA(const Class *derived, const Class *base);

  // Returns whether a class derives from a base class by class name
  static bool ClassIsAClassName(const Class* derived,
                                const String& class_name);

  // Returns the class descriptor for this instance.
  virtual const Class *GetClass() const {
    return GetApparentClass();
  }

  // Returns the class name for this instance.
  virtual String GetClassName() const {
    return GetApparentClass()->name();
  }

  // Returns whether this instance "is a" another type (its class derives from
  // the other class).
  bool IsA(const Class *base) const {
    return ClassIsA(GetClass(), base);
  }

  // Returns whether this instance "is a" another type by class name (its class
  // derives from the other class).
  bool IsAClassName(const String& class_name) {
    return ClassIsAClassName(GetClass(), class_name);
  }

  // A dynamic_cast for types derived from ObjectBase. Like dynamic_cast it will
  // return NULL if the cast fails. Also like dynamic_cast it's slow! Note:
  // Unlike dynamic_cast you don't specify a pointer as the type so
  //
  // Derived* d = ObjectBase::rtti_dynamic_cast<Derived>(base);  // correct
  // Derived* d = ObjectBase::rtti_dynamic_cast<Derived*>(base);  // wrong!
  template <typename T>
  static T* rtti_dynamic_cast(ObjectBase* object) {
    return (object && object->IsA(T::GetApparentClass())) ?
        static_cast<T*>(object) : 0;
  }

 private:
  Id id_;
  ServiceLocator* service_locator_;
  static Class class_;
};

inline Id GetObjectId(const ObjectBase* object) {
  return object == NULL ? 0 : object->id();
}

// Array container for ObjectBase pointers
typedef std::vector<ObjectBase*> ObjectBaseArray;

}  // namespace o3d

#endif  // O3D_CORE_CROSS_OBJECT_BASE_H__
