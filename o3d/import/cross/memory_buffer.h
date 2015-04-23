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


// A MemoryBuffer object represents an array of type T.
// Please note that T is really intended to be used
// for integral types (char, int, float, double)
// It's very useful when used as a stack-based object or
// member variable, offering a cleaner, more direct alternative,
// in some cases, to using smart-pointers in conjunction with operator new
//
// Example usage:
//
// MemoryBuffer<int> buffer(1024);
// for (int i = 0; i < 1024; ++i) {
//   buffer[i] = i;
// }

#ifndef O3D_IMPORT_CROSS_MEMORY_BUFFER_H_
#define O3D_IMPORT_CROSS_MEMORY_BUFFER_H_

#include <string.h>
#include <stdlib.h>
#include <vector>

#include "base/basictypes.h"

// Allocates an array of type T

template <class T>
class MemoryBuffer {
 public:
  MemoryBuffer() {};
  explicit MemoryBuffer(size_t num_elems) : vector_(num_elems, 0) { }

  void Allocate(size_t num_elems) { AllocateClear(num_elems); }
  void AllocateClear(size_t num_elems) { vector_.assign(num_elems, 0); }
  void Deallocate() { vector_.clear(); }
  void Clear() { AllocateClear(vector_.size()); }  // sets to all zero values
  void Resize(size_t n) { vector_.resize(n); }
  size_t GetLength() { return vector_.size(); }
  operator T *() { return &vector_[0]; }

 private:
  std::vector<T> vector_;

  DISALLOW_COPY_AND_ASSIGN(MemoryBuffer);
};

#endif  // O3D_IMPORT_CROSS_MEMORY_BUFFER_H_
