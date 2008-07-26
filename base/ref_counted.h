// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_REF_COUNTED_H__
#define BASE_REF_COUNTED_H__

#include "base/atomic.h"
#include "base/basictypes.h"
#include "base/logging.h"

namespace base {

//
// A base class for reference counted classes.  Otherwise, known as a cheap
// knock-off of WebKit's RefCounted<T> class.  To use this guy just extend your
// class from it like so:
//
//   class MyFoo : public base::RefCounted<MyFoo> {
//    ...
//   };
//
template <class T>
class RefCounted {
 public:
  RefCounted() : ref_count_(0) {
#ifndef NDEBUG
    in_dtor_ = false;
#endif
  }

  ~RefCounted() {
#ifndef NDEBUG
    DCHECK(in_dtor_) << "RefCounted object deleted without calling Release()";
#endif
  }

  void AddRef() {
#ifndef NDEBUG
    DCHECK(!in_dtor_);
#endif
    ++ref_count_;
  }

  void Release() {
#ifndef NDEBUG
    DCHECK(!in_dtor_);
#endif
    if (--ref_count_ == 0) {
#ifndef NDEBUG
      in_dtor_ = true;
#endif
      delete static_cast<T*>(this);
    }
  }

 private:
  int ref_count_;
#ifndef NDEBUG
  bool in_dtor_;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(RefCounted<T>);
};

//
// A thread-safe variant of RefCounted<T>
//
//   class MyFoo : public base::RefCountedThreadSafe<MyFoo> {
//    ...
//   };
//
template <class T>
class RefCountedThreadSafe {
 public:
  RefCountedThreadSafe() : ref_count_(0) {
#ifndef NDEBUG
    in_dtor_ = false;
#endif
  }

  ~RefCountedThreadSafe() {
#ifndef NDEBUG
    DCHECK(in_dtor_) << "RefCountedThreadSafe object deleted without " <<
                        "calling Release()";
#endif
  }

  void AddRef() {
#ifndef NDEBUG
    DCHECK(!in_dtor_);
#endif
    AtomicIncrement(&ref_count_);
  }

  void Release() {
#ifndef NDEBUG
    DCHECK(!in_dtor_);
#endif
    // We need to insert memory barriers to ensure that state written before
    // the reference count became 0 will be visible to a thread that has just
    // made the count 0.
    // TODO(wtc): Bug 1112286: use the barrier variant of AtomicDecrement.
    if (AtomicDecrement(&ref_count_) == 0) {
#ifndef NDEBUG
      in_dtor_ = true;
#endif
      delete static_cast<T*>(this);
    }
  }

 private:
  int32 ref_count_;
#ifndef NDEBUG
  bool in_dtor_;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(RefCountedThreadSafe<T>);
};

} // namespace base

//
// A smart pointer class for reference counted objects.  Use this class instead
// of calling AddRef and Release manually on a reference counted object to
// avoid common memory leaks caused by forgetting to Release an object
// reference.  Sample usage:
//
//   class MyFoo : public RefCounted<MyFoo> {
//    ...
//   };
//
//   void some_function() {
//     scoped_refptr<MyFoo> foo = new MyFoo();
//     foo->Method(param);
//     // |foo| is released when this function returns
//   }
//
//   void some_other_function() {
//     scoped_refptr<MyFoo> foo = new MyFoo();
//     ...
//     foo = NULL;  // explicitly releases |foo|
//     ...
//     if (foo)
//       foo->Method(param);
//   }
//
// The above examples show how scoped_refptr<T> acts like a pointer to T.
// Given two scoped_refptr<T> classes, it is also possible to exchange
// references between the two objects, like so:
//
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b.swap(a);
//     // now, |b| references the MyFoo object, and |a| references NULL.
//   }
//
// To make both |a| and |b| in the above example reference the same MyFoo
// object, simply use the assignment operator:
//
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b = a;
//     // now, |a| and |b| each own a reference to the same MyFoo object.
//   }
//
template <class T>
class scoped_refptr {
 public:
  scoped_refptr() : ptr_(NULL) {
  }

  scoped_refptr(T* p) : ptr_(p) {
    if (ptr_)
      ptr_->AddRef();
  }

  scoped_refptr(const scoped_refptr<T>& r) : ptr_(r.ptr_) {
    if (ptr_)
      ptr_->AddRef();
  }

  ~scoped_refptr() {
    if (ptr_)
      ptr_->Release();
  }

  T* get() const { return ptr_; }
  operator T*() const { return ptr_; }
  T* operator->() const { return ptr_; }

  scoped_refptr<T>& operator=(T* p) {
    // AddRef first so that self assignment should work
    if (p)
      p->AddRef();
    if (ptr_ )
      ptr_ ->Release();
    ptr_ = p;
    return *this;
  }

  scoped_refptr<T>& operator=(const scoped_refptr<T>& r) {
    return *this = r.ptr_;
  }

  void swap(T** pp) {
    T* p = ptr_;
    ptr_ = *pp;
    *pp = p;
  }

  void swap(scoped_refptr<T>& r) {
    swap(&r.ptr_);
  }

 private:
  T* ptr_;
};

#endif  // BASE_REF_COUNTED_H__
