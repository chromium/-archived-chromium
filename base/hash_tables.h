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
//

//
// Deal with the differences between Microsoft and GNU implemenations
// of hash_map. Allows all platforms to use |base::hash_map| and
// |base::hash_set|.
//  eg: 
//   base::hash_map<int> my_map;
//   base::hash_set<int> my_set;
//

#ifndef BASE_HASH_TABLES_H__
#define BASE_HASH_TABLES_H__

#ifdef WIN32
#include <hash_map>
#include <hash_set>
namespace base {
using stdext::hash_map;
using stdext::hash_set;
}
#else
#include <ext/hash_map>
#include <ext/hash_set>
namespace base {
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_set;
}

// Implement string hash functions so that strings of various flavors can
// be used as keys in STL maps and sets.
namespace __gnu_cxx {

inline size_t stl_hash_wstring(const wchar_t* s) {
  unsigned long h = 0;
  for ( ; *s; ++s)
    h = 5 * h + *s;
  return size_t(h);
}

template<>
struct hash<wchar_t*> {
  size_t operator()(const wchar_t* s) const {
    return stl_hash_wstring(s); 
  }
};

template<>
struct hash<const wchar_t*> {
  size_t operator()(const wchar_t* s) const {
    return stl_hash_wstring(s); 
  }
};

template<>
struct hash<std::wstring> {
  size_t operator()(const std::wstring& s) const {
    return stl_hash_wstring(s.c_str()); 
  }
};

template<>
struct hash<const std::wstring> {
  size_t operator()(const std::wstring& s) const {
    return stl_hash_wstring(s.c_str());
  }
};

template<>
struct hash<std::string> {
  size_t operator()(const std::string& s) const {
    return __stl_hash_string(s.c_str());
  }
};

template<>
struct hash<const std::string> {
  size_t operator()(const std::string& s) const {
    return __stl_hash_string(s.c_str());
  }
};  

}

#endif

#endif  // BASE_HASH_TABLES_H__
