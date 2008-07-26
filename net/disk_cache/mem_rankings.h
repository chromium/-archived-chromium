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

// See net/disk_cache/disk_cache.h for the public interface.

#ifndef NET_DISK_CACHE_MEM_RANKINGS_H__
#define NET_DISK_CACHE_MEM_RANKINGS_H__

#include "base/basictypes.h"

namespace disk_cache {

class MemEntryImpl;

// This class handles the ranking information for the memory-only cache.
class MemRankings {
 public:
  MemRankings() : head_(NULL), tail_(NULL) {}
  ~MemRankings() {}

  // Inserts a given entry at the head of the queue.
  void Insert(MemEntryImpl* node);

  // Removes a given entry from the LRU list.
  void Remove(MemEntryImpl* node);

  // Moves a given entry to the head.
  void UpdateRank(MemEntryImpl* node);

  // Iterates through the list.
  MemEntryImpl* GetNext(MemEntryImpl* node);
  MemEntryImpl* GetPrev(MemEntryImpl* node);

 private:
  MemEntryImpl* head_;
  MemEntryImpl* tail_;

  DISALLOW_EVIL_CONSTRUCTORS(MemRankings);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_MEM_RANKINGS_H__
