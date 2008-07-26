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

#ifndef CHROME_BROWSER_COMMON_SCOPED_VECTOR_H__
#define CHROME_BROWSER_COMMON_SCOPED_VECTOR_H__

#include <vector>

#include "chrome/common/stl_util-inl.h"

// ScopedVector wraps a vector deleting the elements from its
// destructor.
template <class T>
class ScopedVector {
 public:
  typedef typename std::vector<T*>::iterator iterator;
  typedef typename std::vector<T*>::const_iterator const_iterator;

  ScopedVector() {}
  ~ScopedVector() { reset(); }

  std::vector<T*>* operator->() { return &v; }
  const std::vector<T*>* operator->() const { return &v; }
  T* operator[](size_t i) { return v[i]; }
  const T* operator[](size_t i) const { return v[i]; }

  bool empty() const { return v.empty(); }
  size_t size() const { return v.size(); }

  iterator begin() { return v.begin(); }
  const_iterator begin() const { return v.begin(); }
  iterator end() { return v.end(); }
  const_iterator end() const { return v.end(); }

  void push_back(T* elem) { v.push_back(elem); }

  std::vector<T*>& get() { return v; }
  const std::vector<T*>& get() const { return v; }
  void swap(ScopedVector<T>& other) { v.swap(other.v); }
  void release(std::vector<T*>* out) { out->swap(v); v.clear(); }

  void reset() { STLDeleteElements(&v); }

 private:
  std::vector<T*> v;

  DISALLOW_EVIL_CONSTRUCTORS(ScopedVector);
};

#endif // CHROME_BROWSER_COMMON_SCOPED_VECTOR_H__
