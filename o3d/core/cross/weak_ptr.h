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


// An intrusive weak pointer implementation.

#ifndef O3D_CORE_CROSS_WEAK_PTR_H_
#define O3D_CORE_CROSS_WEAK_PTR_H_

#include "base/logging.h"
#include "core/cross/smart_ptr.h"

namespace o3d {

// A WeakPointer is a pointer that could go NULL if the object it is pointing
// to is freed/destroyed/released. That means every time you want to access the
// think the WeakPointer is pointing to you must call Get() and check for NULL.
//
// To use this WeakPointer, first, in the class you want it to point to add a
// WeakPointerManager field and initialize it in your constructor. Then provide
// a function GetWeakPointer() to get a WeakPointer to your class. Example.
//
// class MyClass {
//  public:
//   typedef WeakPointer<MyClass> WeakPointerType;
//
//   MyClass() : weak_pointer_manager_(this) {
//   }
//
//   WeakPointerType GetWeakPointer() const {
//     weak_pointer_manager_.GetWeakPointer();
//   }
//
//  private:
//   WeakPointerType::WeakPointerManager weak_pointer_manager_;
// };
template <class C>
class WeakPointer {
 public:
  typedef C* Pointer;
  typedef C& Reference;
  typedef C ClassType;

  WeakPointer() : handle_(typename WeakPointerHandle::Ref(NULL)) {
  }

  // This copy constructor is not marked explicit on purpose, so that
  // we can copy weak pointers implicitly.
  WeakPointer(const WeakPointer<C>& source)  // NOLINT
      : handle_(source.handle_) {
  }

  WeakPointer<C>& operator=(const WeakPointer<C>& rhs) {
    if (this != &rhs) {
      handle_ = rhs.handle_;
    }
    return *this;
  }

  // Gets the object this weak pointer is pointing to.
  Pointer Get() const {
    return handle_.IsNull() ? NULL : handle_->GetRaw();
  }

  // This class manages a WeakPointer for the object the WeakPointer is pointing
  // to. If you forget to call Init you'll get an assert on destruction. It also
  // enforces calling Reset on the weak pointer handle at destruction.
  class WeakPointerManager {
   public:
    typedef C* Pointer;
    typedef C& Reference;
    typedef C ClassType;
    typedef WeakPointer<C> WeakPointerType;
    typedef typename WeakPointerType::WeakPointerHandle HandleType;

    explicit WeakPointerManager(Pointer data)
        : handle_(typename HandleType::Ref(new HandleType(data))) {
    }

    ~WeakPointerManager() {
      handle_->Reset();
    }

    WeakPointerType GetWeakPointer() const {
      return handle_->GetWeakPointer();
    }

   private:
    typename HandleType::Ref handle_;
  };

 private:
  // This class holds the pointer to the actual object all weak pointers
  // are pointing to. It is ref counted so when the last weak pointer
  // goes away this handle will go away.
  class WeakPointerHandle : public RefCounted {
   public:
    typedef SmartPointer<WeakPointerHandle> Ref;

    explicit WeakPointerHandle(Pointer data)
        : data_(data) {
    }

    void Reset() {
      data_ = NULL;
    }

    Pointer GetRaw() const {
      return data_;
    }

    WeakPointer<C> GetWeakPointer() const {
      return WeakPointer<C>(this);
    }

   private:
    Pointer data_;

    DISALLOW_COPY_AND_ASSIGN(WeakPointerHandle);
  };

  explicit WeakPointer(const WeakPointerHandle* handle)
      : handle_(typename WeakPointerHandle::Ref(
          const_cast<WeakPointerHandle*>(handle))) {
  }

  typename WeakPointerHandle::Ref handle_;
};

// Provide a convenience equality test operator on SmartPointer objects.
template <class C>
inline bool operator==(const WeakPointer<C>& lhs, const WeakPointer<C>& rhs) {
  return lhs.Get() == rhs.Get();
}

}  // namespace o3d

#endif  // O3D_CORE_CROSS_WEAK_PTR_H_
