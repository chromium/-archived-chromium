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


// This file contains the declaration of class VisitorBase.

#ifndef O3D_CORE_CROSS_VISITOR_BASE_H_
#define O3D_CORE_CROSS_VISITOR_BASE_H_

#include <map>

#include "base/basictypes.h"
#include "core/cross/object_base.h"

namespace o3d {

// Interface implemented by all visitor classes.
class IVisitor {
 public:
  IVisitor() {
  }

  virtual ~IVisitor() {
  }

  // Calls the appropriate visitor function enabled for the runtime
  // type of visited object.
  virtual void Accept(ObjectBase* visited) = 0;

  // Returns whether a visitor function has been registered for the
  // given class.
  virtual bool IsHandled(const ObjectBase::Class* clazz) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(IVisitor);
};

// Immediate base class of all visitor classes. The type of the derived
// class must be specified as the Visitor template parameter. Used like:
//
// class MyVisitor : public VisitorBase<MyVisitor> {
// }
template <typename Visitor>
class VisitorBase : public IVisitor {
  struct ForwarderBase {
    virtual void Forward(VisitorBase<Visitor>* visitor,
                         ObjectBase* visited) = 0;
    virtual ~ForwarderBase() {
    }
  };

  template <typename VisitedClass>
  struct Forwarder : ForwarderBase {
    explicit Forwarder(void (Visitor::*function)(VisitedClass* visited))
        : function_(function) {
    }
    virtual void Forward(VisitorBase<Visitor>* visitor,
                         ObjectBase* visited) {
      (static_cast<Visitor*>(visitor)->*function_)(
          static_cast<VisitedClass*>(visited));
    }
    void (Visitor::*function_)(VisitedClass* visited);
  };

  typedef typename std::map<const ObjectBase::Class*, ForwarderBase*>
      ForwarderMap;

 public:
  VisitorBase() {
  }

  virtual ~VisitorBase() {
    for (typename ForwarderMap::iterator it = forwarders_.begin();
         it != forwarders_.end(); ++it) {
      delete it->second;
    }
  }

  // Enables the given function to be called for class VisitedClass.
  template <typename VisitedClass>
  void Enable(void (Visitor::*function)(VisitedClass* visited)) {
    forwarders_.insert(typename ForwarderMap::value_type(
        VisitedClass::GetApparentClass(),
        new Forwarder<VisitedClass>(function)));
  }

  // Calls the appropriate visitor function enabled for the runtime
  // type of visited object.
  virtual void Accept(ObjectBase* visited) {
    if (visited == NULL)
      return;

    const ObjectBase::Class* current_class = visited->GetClass();
    typename ForwarderMap::iterator it;
    while (current_class != NULL) {
      it = forwarders_.find(current_class);
      if (it != forwarders_.end()) {
        it->second->Forward(this, visited);
        break;
      }
      current_class = current_class->parent();
    }
  }

  // Returns whether a visitor function has been registered for the
  // given class.
  virtual bool IsHandled(const ObjectBase::Class* clazz) {
    const ObjectBase::Class* current_class = clazz;
    typename ForwarderMap::iterator it;
    while (current_class != NULL) {
      it = forwarders_.find(current_class);
      if (it != forwarders_.end()) {
        return true;
      }
      current_class = current_class->parent();
    }
    return false;
  }

 private:
  ForwarderMap forwarders_;
  DISALLOW_COPY_AND_ASSIGN(VisitorBase<Visitor>);
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_VISITOR_BASE_H_
