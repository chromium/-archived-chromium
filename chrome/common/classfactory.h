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

// ClassFactory<class>
// is a simple class factory object for the parameterized class.
//
#ifndef _CLASSFACTORY_H_
#define _CLASSFACTORY_H_

#include <unknwn.h>

// GenericClassFactory
// provides the basic COM plumbing to implement IClassFactory, and
// maintains a static count on the number of these objects in existence.
// It remains for subclasses to implement CreateInstance.
class GenericClassFactory : public IClassFactory {
public:
  GenericClassFactory();
  ~GenericClassFactory();

  //IUnknown methods
  STDMETHOD(QueryInterface)(REFIID iid, LPVOID* ppvObject);
  STDMETHOD_(ULONG, AddRef)();
  STDMETHOD_(ULONG, Release)();

  //IClassFactory methods
  STDMETHOD(CreateInstance)(LPUNKNOWN pUnkOuter, REFIID riid, LPVOID* ppvObject) = 0;
  STDMETHOD(LockServer)(BOOL fLock);

  // generally handy for DllUnloadNow -- count of existing descendant objects
  static LONG GetObjectCount() { return object_count_; }

protected:
  LONG reference_count_; // mind the reference counting for this object
  static LONG object_count_; // count of all these objects
};


// OneClassFactory<T>
// Knows how to be a factory for T's
template <class T>
class OneClassFactory : public GenericClassFactory
{
public:
  //IClassFactory methods
  STDMETHOD(CreateInstance)(LPUNKNOWN pUnkOuter, REFIID riid, void** ppvObject);
};


template <class T>
STDMETHODIMP OneClassFactory<T>::CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, void** result) {
  *result = NULL;

  if(pUnkOuter != NULL)
    return CLASS_E_NOAGGREGATION;

  T* const obj = new T();
  if(!obj)
    return E_OUTOFMEMORY;

  obj->AddRef();
  HRESULT const hr = obj->QueryInterface(riid, result);
  obj->Release();

  return hr;
}

#endif
