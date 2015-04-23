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


// This file contains the declaration of NamedObject.

#ifndef O3D_CORE_CROSS_NAMED_OBJECT_H_
#define O3D_CORE_CROSS_NAMED_OBJECT_H_

#include "core/cross/object_base.h"
#include "core/cross/types.h"

namespace o3d {

class ServiceLocator;

// Base class of all objects that are identifiable by a name.
class NamedObjectBase: public ObjectBase {
 public:
  typedef SmartPointer<NamedObjectBase> Ref;

  explicit NamedObjectBase(ServiceLocator *service_locator);

  // \brief Returns the object's name
  virtual const String& name() const = 0;

 private:
  O3D_DECL_CLASS(NamedObjectBase, ObjectBase);
  DISALLOW_COPY_AND_ASSIGN(NamedObjectBase);
};

// A class of all objects that are identifiable by a name where the name is
// settable at any time. This as opposed to Param where the name is only
// settable once.
class NamedObject: public NamedObjectBase {
 public:
  typedef SmartPointer<NamedObject> Ref;

  explicit NamedObject(ServiceLocator *service_locator);

  // \brief Returns the object's name
  virtual const String& name() const;

  void set_name(const String& new_name) {
    name_ = new_name;
  }

 private:
  String name_;
  O3D_DECL_CLASS(NamedObject, NamedObjectBase);
  DISALLOW_COPY_AND_ASSIGN(NamedObject);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_NAMED_OBJECT_H_
