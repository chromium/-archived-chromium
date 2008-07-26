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

#ifndef BASE_SINGLETON_INTERNAL_H__
#define BASE_SINGLETON_INTERNAL_H__

// Define the storage of the singleton pointer.
template <typename Type, typename DifferentiatingType, bool kVolatile>
class SingletonStorage {
 protected:
  // The actual pointer. Note that it is a volatile pointer.
  static Type* volatile instance_;
};

// Partial specialization.
template <typename Type, typename DifferentiatingType>
class SingletonStorage<Type, DifferentiatingType, false> {
 protected:
  // The pointer does not need to be volatile for use with pthread_once or
  // locked initialization.
  static Type* instance_;
};

template <typename Type, typename DifferentiatingType, bool kVolatile>
Type* volatile SingletonStorage<Type, DifferentiatingType,
                                kVolatile>::instance_ = NULL;

template <typename Type, typename DifferentiatingType>
Type* SingletonStorage<Type, DifferentiatingType, false>::instance_ = NULL;

template<bool kUseVolatile>
struct UseVolatileSingleton {
#ifdef WIN32
  static const bool value = kUseVolatile;
#else
  static const bool value = false;
#endif  // WIN32
};

#endif  // BASE_SINGLETON_INTERNAL_H__
