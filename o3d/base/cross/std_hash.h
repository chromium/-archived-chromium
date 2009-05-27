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


// This file declares STL hash map classes in a cross-compiler way, providing a
// GCC-compatible (not MSVC) interface.

#ifndef O3D_BASE_CROSS_STD_HASH_H_
#define O3D_BASE_CROSS_STD_HASH_H_

#include <build/build_config.h>

#if defined(COMPILER_MSVC)
#include <hash_map>
#include <hash_set>
namespace o3d {
namespace base {

struct PortableHashBase {
  // These two public members are required by msvc.  4 and 8 are the
  // default values.
  static const size_t bucket_size = 4;
  static const size_t min_buckets = 8;
};

template <typename Key>
struct hash;

// These are missing from MSVC.
template<> struct hash<int> {
  size_t operator()(int n) const {
    return static_cast<size_t>(n);
  }
};

template<> struct hash<unsigned int> {
  size_t operator()(unsigned int n) const {
    return static_cast<size_t>(n);
  }
};

template<typename T> struct hash<T*> {
  size_t operator()(T* t) const {
    return reinterpret_cast<size_t>(t);
  }
};

// If the 3rd template parameter of the GNU interface (KeyEqual) is
// omitted, then we know that it's using the == operator, so we can
// safely use the < operator.
//
// If the third parameter is specified, then we get a compile time
// error, and we know we have to go back and add some #ifdefs.
template <typename Key, typename Hash>
struct HashAndLessOperator : PortableHashBase {
  bool operator()(const Key& a, const Key& b) const {
    return a < b;
  }
  size_t operator()(const Key& key) const {
    return hasher_(key);
  }
  Hash hasher_;
};

template <class Key, class Hash = hash<Key> >
class hash_set : public stdext::hash_set<Key, HashAndLessOperator<Key, Hash> > {
 public:
  hash_set() {}
  explicit hash_set(int buckets) {}
  typedef std::equal_to<Key> key_equal;
  size_type bucket_count() const {
    return size() / bucket_size;
  }
};

template <class Key, class Val, class Hash = hash<Key> >
class hash_map : public stdext::hash_map<
    Key, Val, HashAndLessOperator<Key, Hash> > {
 public:
  hash_map() {}
  explicit hash_map(int buckets) {}
  typedef std::equal_to<Key> key_equal;
  size_type bucket_count() const {
    return size() / bucket_size;
  }
};

template <class Key, class Hash = hash<Key> >
class hash_multiset : public stdext::hash_multiset<
    Key, HashAndLessOperator<Key, Hash> > {
 public:
  hash_multiset() {}
  explicit hash_multiset(int buckets) {}
  typedef std::equal_to<Key> key_equal;
  size_type bucket_count() const {
    return size() / bucket_size;
  }
};

template <class Key, class Val, class Hash = hash<Key> >
class hash_multimap : public stdext::hash_multimap<
    Key, Val, HashAndLessOperator<Key, Hash> > {
 public:
  hash_multimap() {}
  explicit hash_multimap(int buckets) {}
  typedef std::equal_to<Key> key_equal;
  size_type bucket_count() const {
    return size() / bucket_size;
  }
};

}  // namespace base
}  // namespace o3d
#elif defined COMPILER_GCC
#include <ext/hash_map>
#include <ext/hash_set>
#include <tr1/functional>
namespace __gnu_cxx {
template <class T> struct hash<T*> {
  size_t operator() (const T* x) const {
    return hash<size_t>()(reinterpret_cast<size_t>(x));
  }
};
}  // namespace __gnu_cxx

namespace o3d {
namespace base {
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_multimap;
using __gnu_cxx::hash_set;
using __gnu_cxx::hash_multiset;
using __gnu_cxx::hash;
}  // namespace base
}  // namespace o3d
#endif  // COMPILER_MSVC

#endif  // O3D_BASE_CROSS_STD_HASH_H_
