// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_COMPTR_H_
#define BASE_SCOPED_COMPTR_H_

#include "base/logging.h"
#include "base/ref_counted.h"

#if defined(OS_WIN)
#include <unknwn.h>

// Utility template to prevent users of scoped_comptr from calling AddRef and/or
// Release() without going through the scoped_comptr class.
template <class Interface>
class BlockIUnknownMethods : public Interface {
 private:
  STDMETHOD(QueryInterface)(REFIID iid, void** object) = 0;
  STDMETHOD_(ULONG, AddRef)() = 0;
  STDMETHOD_(ULONG, Release)() = 0;
};

// A fairly minimalistic smart class for COM interface pointers.
// Uses scoped_refptr for the basic smart pointer functionality
// and adds a few IUnknown specific services.
template <class Interface, const IID* interface_id = &__uuidof(Interface)>
class scoped_comptr : public scoped_refptr<Interface> {
 public:
  typedef scoped_refptr<Interface> ParentClass;

  scoped_comptr() {
  }

  explicit scoped_comptr(Interface* p) : ParentClass(p) {
  }

  explicit scoped_comptr(const scoped_comptr<Interface, interface_id>& p)
      : ParentClass(p) {
  }

  ~scoped_comptr() {
    // We don't want the smart pointer class to be bigger than the pointer
    // it wraps.
    COMPILE_ASSERT(sizeof(scoped_comptr<Interface, interface_id>) ==
                   sizeof(Interface*), ScopedComPtrSize);
  }

  // Explicit Release() of the held object.  Useful for reuse of the
  // scoped_comptr instance.
  // Note that this function equates to IUnknown::Release and should not
  // be confused with e.g. scoped_ptr::release().
  void Release() {
    if (ptr_ != NULL) {
      ptr_->Release();
      ptr_ = NULL;
    }
  }

  // Sets the internal pointer to NULL and returns the held object without
  // releasing the reference.
  Interface* Detach() {
    Interface* p = ptr_;
    ptr_ = NULL;
    return p;
  }

  // Accepts an interface pointer that has already been addref-ed.
  void Attach(Interface* p) {
    DCHECK(ptr_ == NULL);
    ptr_ = p;
  }

  // Retrieves the pointer address.
  // Used to receive object pointers as out arguments (and take ownership).
  // The function DCHECKs on the current value being NULL.
  // Usage: Foo(p.Receive());
  Interface** Receive() {
    DCHECK(ptr_ == NULL) << "Object leak. Pointer must be NULL";
    return &ptr_;
  }

  template <class Query>
  HRESULT QueryInterface(Query** p) {
    DCHECK(p != NULL);
    DCHECK(ptr_ != NULL);
    // IUnknown already has a template version of QueryInterface
    // so the iid parameter is implicit here. The only thing this
    // function adds are the DCHECKs.
    return ptr_->QueryInterface(p);
  }

  // Queries |other| for the interface this object wraps and returns the
  // error code from the other->QueryInterface operation.
  HRESULT QueryFrom(IUnknown* object) {
    DCHECK(object != NULL);
    return object->QueryInterface(Receive());
  }

  // Convenience wrapper around CoCreateInstance
  HRESULT CreateInstance(const CLSID& clsid, IUnknown* outer = NULL,
                         DWORD context = CLSCTX_ALL) {
    DCHECK(ptr_ == NULL);
    HRESULT hr = ::CoCreateInstance(clsid, outer, context, *interface_id,
                                    reinterpret_cast<void**>(&ptr_));
    return hr;
  }

  // Checks if the identity of |other| and this object is the same.
  bool IsSameObject(IUnknown* other) {
    if (!other && !ptr_)
      return true;

    if (!other || !ptr_)
      return false;

    scoped_comptr<IUnknown> my_identity;
    QueryInterface(my_identity.Receive());

    scoped_comptr<IUnknown> other_identity;
    other->QueryInterface(other_identity.Receive());

    return static_cast<IUnknown*>(my_identity) ==
           static_cast<IUnknown*>(other_identity);
  }

  // Provides direct access to the interface.
  // Here we use a well known trick to make sure we block access to
  // IUknown methods so that something bad like this doesn't happen:
  //    scoped_comptr<IUnknown> p(Foo());
  //    p->Release();
  //    ... later the destructor runs, which will Release() again.
  // and to get the benefit of the DCHECKs we add to QueryInterface.
  // There's still a way to call these methods if you absolutely must
  // by statically casting the scoped_comptr instance to the wrapped interface
  // and then making the call... but generally that shouldn't be necessary.
  BlockIUnknownMethods<Interface>* operator->() const {
    DCHECK(ptr_ != NULL);
    return reinterpret_cast<BlockIUnknownMethods<Interface>*>(ptr_);
  }

  // static methods

  static const IID& iid() {
    return *interface_id;
  }
};

#endif  // #if defined(OS_WIN)

#endif  // BASE_SCOPED_COMPTR_H_
