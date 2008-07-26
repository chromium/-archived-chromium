//
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

#include "classfactory.h"


GenericClassFactory::GenericClassFactory() :
  reference_count_(1)
{
  InterlockedIncrement(&object_count_);
}


GenericClassFactory::~GenericClassFactory() {
  InterlockedDecrement(&object_count_);
}


LONG GenericClassFactory::object_count_ = 0;


STDMETHODIMP GenericClassFactory::QueryInterface(REFIID riid, LPVOID* ppobject) {
  *ppobject = NULL;

  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_IClassFactory))
    *ppobject = static_cast<IClassFactory*>(this);
  else
    return E_NOINTERFACE;

  this->AddRef();
  return S_OK;
}


STDMETHODIMP_(ULONG) GenericClassFactory::AddRef() {
  return InterlockedIncrement(&reference_count_);
}


STDMETHODIMP_(ULONG) GenericClassFactory::Release() {
  if(0 == InterlockedDecrement(&reference_count_)) {
    delete this;
    return 0;
  }
  return reference_count_;
}


STDMETHODIMP GenericClassFactory::LockServer(BOOL) {
  return E_NOTIMPL;
}
