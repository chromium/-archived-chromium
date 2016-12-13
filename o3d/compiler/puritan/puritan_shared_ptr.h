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

//
// A simple reference counted pointer implementation. It is a subset
// of the boost/tr1 shared_ptr class, which is expected to be part of
// the next C++ standard.  See section 20.6.6 [util.smartptr] of the
// draft standard for a full description of the standard interface.
//
// The following standard features have been omitted from this implementation:
//   - no custom deallocators - uses delete
//   - no support for enable_shared_from_this
//   - no support for smart pointer casts
//   - no support for features that rely on variadic templates
//   - not exception-safe
//   - no overloaded comparison operators (e.g. operator<). They're
//     convenient, but they can be explicitly defined outside the class.
//

#ifndef UTIL_GTL_PURITAN_SHARED_PTR_H__
#define UTIL_GTL_PURITAN_SHARED_PTR_H__

#include <algorithm>  // for swap

template <typename T> class shared_ptr;
template <typename T> class weak_ptr;

// This class is an internal implementation detail for shared_ptr.
//
class SharedPtrControlBlock {
  template <typename T> friend class shared_ptr;
  template <typename T> friend class weak_ptr;
 private:
  SharedPtrControlBlock() : refcount_(1), weak_count_(1) { }
  int refcount_;
  int weak_count_;
};

template <typename T>
class shared_ptr {
  template <typename U> friend class weak_ptr;
 public:
  typedef T element_type;

  explicit shared_ptr(T* ptr = NULL)
      : ptr_(ptr),
        control_block_(ptr != NULL ? new SharedPtrControlBlock : NULL) {
  }

  // Copy constructor: makes this object a copy of ptr
  template <typename U>
  shared_ptr(const shared_ptr<U>& ptr)
      : ptr_(NULL),
        control_block_(NULL) {
    Initialize(ptr);
  }
  // Need non-templated version to prevent the compiler-generated default
  shared_ptr(const shared_ptr<T>& ptr)
      : ptr_(NULL),
        control_block_(NULL) {
    Initialize(ptr);
  }

  // Assignment operator. Replaces the existing shared_ptr with ptr.
  template <typename U>
  shared_ptr<T>& operator=(const shared_ptr<U>& ptr) {
    if (ptr_ != ptr.ptr_) {
      shared_ptr<T> me(ptr);   // will hold our previous state to be destroyed.
      swap(me);
    }
    return *this;
  }

  // Need non-templated version to prevent the compiler-generated default
  shared_ptr<T>& operator=(const shared_ptr<T>& ptr) {
    if (ptr_ != ptr.ptr_) {
      shared_ptr<T> me(ptr);   // will hold our previous state to be destroyed.
      swap(me);
    }
    return *this;
  }

  ~shared_ptr() {
    if (ptr_ != NULL) {
      if (--control_block_->refcount_ == 0) {
        delete ptr_;

        // weak_count_ is defined as the number of weak_ptrs that observe
        // ptr_, plus 1 if refcount_ is nonzero.
        if (--control_block_->weak_count_ == 0) {
          delete control_block_;
        }
      }
    }
  }

  // Replaces underlying raw pointer with the one passed in.  The reference
  // count is set to one (or zero if the pointer is NULL) for the pointer
  // being passed in and decremented for the one being replaced.
  void reset(T* p = NULL) {
    if (p != ptr_) {
      shared_ptr<T> tmp(p);
      tmp.swap(*this);
    }
  }

  // Exchanges the contents of this with the contents of r.  This function
  // supports more efficient swapping since it eliminates the need for a
  // temporary shared_ptr object.
  void swap(shared_ptr<T>& r) {
    std::swap(ptr_, r.ptr_);
    std::swap(control_block_, r.control_block_);
  }

  // The following function is useful for gaining access to the underlying
  // pointer when a shared_ptr remains in scope so the reference-count is
  // known to be > 0 (e.g. for parameter passing).
  T* get() const {
    return ptr_;
  }

  T& operator*() const {
    return *ptr_;
  }

  T* operator->() const {
    return ptr_;
  }

  long use_count() const {
    return control_block_ ? control_block_->refcount_ : 1;
  }

  bool unique() const {
    return use_count() == 1;
  }

 private:
  // If r is non-empty, initialize *this to share ownership with r,
  // increasing the underlying reference count.
  // If r is empty, *this remains empty.
  // Requires: this is empty, namely this->ptr_ == NULL.
  template <typename U>
  void Initialize(const shared_ptr<U>& r) {
    if (r.control_block_ != NULL) {
      ++r.control_block_->refcount_;

      ptr_ = r.ptr_;
      control_block_ = r.control_block_;
    }
  }

  T* ptr_;
  SharedPtrControlBlock* control_block_;

  template <typename U>
  friend class shared_ptr;
};

// Matches the interface of std::swap as an aid to generic programming.
template <typename T> void swap(shared_ptr<T>& r, shared_ptr<T>& s) {
  r.swap(s);
}

// See comments at the top of the file for a description of why this
// class exists, and the draft C++ standard (as of October 2007 the
// latest draft is N2461) for the detailed specification.
template <typename T>
class weak_ptr {
  template <typename U> friend class weak_ptr;
 public:
  typedef T element_type;

  // Create an empty (i.e. already expired) weak_ptr.
  weak_ptr() : ptr_(NULL), control_block_(NULL) { }

  // Create a weak_ptr that observes the same object that ptr points
  // to.  Note that there is no race condition here: we know that the
  // control block can't disappear while we're looking at it because
  // it is owned by at least one shared_ptr, ptr.
  template <typename U> weak_ptr(const shared_ptr<U>& ptr) {
    CopyFrom(ptr.ptr_, ptr.control_block_);
  }

  // Copy a weak_ptr. The object it points to might disappear, but we
  // don't care: we're only working with the control block, and it can't
  // disappear while we're looking at because it's owned by at least one
  // weak_ptr, ptr.
  template <typename U> weak_ptr(const weak_ptr<U>& ptr) {
    CopyFrom(ptr.ptr_, ptr.control_block_);
  }

  // Need non-templated version to prevent default copy constructor
  weak_ptr(const weak_ptr& ptr) {
    CopyFrom(ptr.ptr_, ptr.control_block_);
  }

  // Destroy the weak_ptr. If no shared_ptr owns the control block, and if
  // we are the last weak_ptr to own it, then it can be deleted. Note that
  // weak_count_ is defined as the number of weak_ptrs sharing this control
  // block, plus 1 if there are any shared_ptrs. We therefore know that it's
  // safe to delete the control block when weak_count_ reaches 0, without
  // having to perform any additional tests.
  ~weak_ptr() {
    if (control_block_ != NULL &&
        (--control_block_->weak_count_) == 0) {
      delete control_block_;
    }
  }

  weak_ptr& operator=(const weak_ptr& ptr) {
    if (&ptr != this) {
      weak_ptr tmp(ptr);
      tmp.swap(*this);
    }
    return *this;
  }
  template <typename U> weak_ptr& operator=(const weak_ptr<U>& ptr) {
    weak_ptr tmp(ptr);
    tmp.swap(*this);
    return *this;
  }
  template <typename U> weak_ptr& operator=(const shared_ptr<U>& ptr) {
    weak_ptr tmp(ptr);
    tmp.swap(*this);
    return *this;
  }

  void swap(weak_ptr& ptr) {
    ::swap(ptr_, ptr.ptr_);
    ::swap(control_block_, ptr.control_block_);
  }

  void reset() {
    weak_ptr tmp;
    tmp.swap(*this);
  }

  // Return the number of shared_ptrs that own the object we are observing.
  // Note that this number can be 0 (if this pointer has expired).
  long use_count() const {
    return control_block_ != NULL ? control_block_->refcount_ : 0;
  }

  bool expired() const { return use_count() == 0; }

 private:
  void CopyFrom(T* ptr, SharedPtrControlBlock* control_block) {
    ptr_ = ptr;
    control_block_ = control_block;
    if (control_block_ != NULL)
      ++control_block_->weak_count_;
  }

 private:
  element_type* ptr_;
  SharedPtrControlBlock* control_block_;
};

template <typename T> void swap(weak_ptr<T>& r, weak_ptr<T>& s) {
  r.swap(s);
}

#endif  // UTIL_GTL_PURITAN_SHARED_PTR_H__
