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
// Parts of this module come from:
//  http://www.codeproject.com/KB/applications/visualleakdetector.aspx
//       by Dan Moulding.
//  http://www.codeproject.com/KB/threads/StackWalker.aspx
//       by Jochen Kalmbach

#ifndef MEMORY_WATCHER_CALL_STACK_H_
#define MEMORY_WATCHER_CALL_STACK_H_

#include <windows.h>
#include <dbghelp.h>
#include <functional>
#include <map>

#include "memory_watcher.h"
#include "base/lock.h"
#include "base/logging.h"

// The CallStack Class
// A stack where memory has been allocated.
class CallStack {
 public:
  // Initialize for tracing CallStacks.
  static bool Initialize();

  CallStack();
  virtual ~CallStack() {}

  // Get a hash for this CallStack.
  // Identical stack traces will have matching hashes.
  int32 hash() { return hash_; }

  // Get a unique ID for this CallStack.
  // No two CallStacks will ever have the same ID.  The ID is a monotonically
  // increasing number.  Newer CallStacks always have larger IDs.
  int32 id() { return id_; }

  // Retrieves the frame at the specified index.
  DWORD_PTR frame(int32 index) {
    DCHECK(index < frame_count_ && index >= 0);
    return frames_[index];
  }

  // Compares the CallStack to another CallStack
  // for equality. Two CallStacks are equal if they are the same size and if
  // every frame in each is identical to the corresponding frame in the other.
  bool IsEqual(const CallStack &target);

  // Convert the callstack to a string stored in output.
  void CallStack::ToString(std::string* output);

 private:
  // The maximum number of frames to trace.
  static const int kMaxTraceFrames = 32;

  // Pushes a frame's program counter onto the CallStack.
  void AddFrame(DWORD_PTR programcounter);

  // Traces the stack, starting from this function, up to kMaxTraceFrames
  // frames.
  bool GetStackTrace();

  // Functions for manipulating the frame list.
  void ClearFrames();

  int frame_count_;  // Current size (in frames)
  DWORD_PTR frames_[kMaxTraceFrames];
  int32 hash_;
  int32 id_;

  // Cache ProgramCounter -> Symbol lookups.
  // This cache is not thread safe.
  typedef std::map<int32, std::string, std::less<int32>,
                   PrivateHookAllocator<int32> > SymbolCache;
  static SymbolCache* symbol_cache_;

  DISALLOW_EVIL_CONSTRUCTORS(CallStack);
};

// An AllocationStack is a type of CallStack which represents
// a CallStack where memory has been allocated.  As such, in
// addition to the CallStack information, it also tracks the
// amount of memory allocated.
class AllocationStack : public CallStack {
 public:
  explicit AllocationStack(int32 alloc_size)
    : allocation_size_(alloc_size),
      next_(NULL),
      CallStack() {
  }

  // The size of the allocation.
  int32 allocation_size() { return allocation_size_; }

  // We maintain a freelist of the AllocationStacks.
  void* operator new(size_t s);
  void operator delete(void*p);

 private:
  int32 allocation_size_;  // The size of the allocation

  AllocationStack* next_;     // Pointer used when on the freelist.
  static AllocationStack* freelist_;
  static Lock freelist_lock_;

  DISALLOW_EVIL_CONSTRUCTORS(AllocationStack);
};

#endif  // MEMORY_WATCHER_CALL_STACK_H_
