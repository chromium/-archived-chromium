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

// Functions used to debug memory usage, leaks, and other memory issues.
// All methods are effectively no-ops unless this program is being run through
// a supported memory tool (currently, only Purify)

#ifndef BASE_MEMORY_DEBUG_H_

namespace base {

class MemoryDebug {
public:
  // Since MIU messages are a lot of data, and we don't always want this data,
  // we have a global switch.  If disabled, *MemoryInUse are no-ops.
  static void SetMemoryInUseEnabled(bool enabled);

  // Dump information about all memory in use.
  static void DumpAllMemoryInUse();
  // Dump information about new memory in use since the last
  // call to DumpAllMemoryInUse() or DumpNewMemoryInUse().
  static void DumpNewMemoryInUse();

  // Dump information about all current memory leaks.
  static void DumpAllLeaks();
  // Dump information about new memory leaks since the last
  // call to DumpAllLeaks() or DumpNewLeaks()
  static void DumpNewLeaks();

  // Mark |size| bytes of memory as initialized, so it doesn't produce any UMRs
  // or UMCs.
  static void MarkAsInitialized(void* addr, size_t size);

private:
  static bool memory_in_use_;
};

} // namespace base

#endif