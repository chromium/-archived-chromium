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


// This file declares STL functional classes in a cross-compiler way.

#ifndef O3D_BASE_CROSS_STD_FUNCTIONAL_H_
#define O3D_BASE_CROSS_STD_FUNCTIONAL_H_

#include <build/build_config.h>

#if defined(COMPILER_GCC)
#include <ext/functional>
namespace o3d {
namespace base {
using __gnu_cxx::select1st;
using __gnu_cxx::select2nd;
}  // namespace base
}  // namespace o3d
#elif defined(COMPILER_MSVC)
#include <functional>
#include <utility>
namespace o3d {
namespace base {

template <class Pair>
class select1st : public std::unary_function<Pair, typename Pair::first_type> {
 public:
  const result_type &operator()(const argument_type &value) const {
    return value.first;
  }
};

template <class Pair>
class select2nd : public std::unary_function<Pair, typename Pair::second_type> {
 public:
  const result_type &operator()(const argument_type &value) const {
    return value.second;
  }
};

}  // namespace base
}  // namespace o3d
#else
#error Unsupported compiler
#endif

#endif  // O3D_BASE_CROSS_STD_FUNCTIONAL_H_
