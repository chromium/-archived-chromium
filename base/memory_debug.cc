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

#ifdef PURIFY
// this #define is used to prevent people from directly using pure.h
// instead of memory_debug.h
#define PURIFY_PRIVATE_INCLUDE
#include "base/third_party/purify/pure.h"
#endif

#include "base/memory_debug.h"

namespace base {

bool MemoryDebug::memory_in_use_ = false;

void MemoryDebug::SetMemoryInUseEnabled(bool enabled) {
  memory_in_use_ = enabled;
}

void MemoryDebug::DumpAllMemoryInUse() {
#ifdef PURIFY
  if (memory_in_use_)
    PurifyAllInuse();
#endif
}

void MemoryDebug::DumpNewMemoryInUse() {
#ifdef PURIFY
  if (memory_in_use_)
    PurifyNewInuse();
#endif
}

void MemoryDebug::DumpAllLeaks() {
#ifdef PURIFY
  PurifyAllLeaks();
#endif
}

void MemoryDebug::DumpNewLeaks() {
#ifdef PURIFY
  PurifyNewLeaks();
#endif
}

void MemoryDebug::MarkAsInitialized(void* addr, size_t size) {
#ifdef PURIFY
  PurifyMarkAsInitialized(addr, size);
#endif
}

} // namespace base
