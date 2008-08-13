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

#ifndef NET_DISK_CACHE_RANKINGS_H__
#define NET_DISK_CACHE_RANKINGS_H__

#include <list>

#include "net/disk_cache/addr.h"
#include "net/disk_cache/mapped_file.h"
#include "net/disk_cache/storage_block.h"

namespace disk_cache {

class BackendImpl;

// Type of crashes generated for the unit tests.
enum RankCrashes {
  NO_CRASH = 0,
  INSERT_EMPTY_1,
  INSERT_EMPTY_2,
  INSERT_EMPTY_3,
  INSERT_ONE_1,
  INSERT_ONE_2,
  INSERT_ONE_3,
  INSERT_LOAD_1,
  INSERT_LOAD_2,
  REMOVE_ONE_1,
  REMOVE_ONE_2,
  REMOVE_ONE_3,
  REMOVE_ONE_4,
  REMOVE_HEAD_1,
  REMOVE_HEAD_2,
  REMOVE_HEAD_3,
  REMOVE_HEAD_4,
  REMOVE_TAIL_1,
  REMOVE_TAIL_2,
  REMOVE_TAIL_3,
  REMOVE_LOAD_1,
  REMOVE_LOAD_2,
  REMOVE_LOAD_3,
  MAX_CRASH
};

// This class handles the ranking information for the cache.
class Rankings {
 public:
  // This class provides a specialized version of scoped_ptr, that calls
  // Rankings whenever a CacheRankingsBlock is deleted, to keep track of cache
  // iterators that may go stale.
  class ScopedRankingsBlock : public scoped_ptr<CacheRankingsBlock> {
   public:
    explicit ScopedRankingsBlock(Rankings* rankings) : rankings_(rankings) {}
    ScopedRankingsBlock(Rankings* rankings, CacheRankingsBlock* node)
        : rankings_(rankings), scoped_ptr<CacheRankingsBlock>(node) {}

    ~ScopedRankingsBlock() {
      rankings_->FreeRankingsBlock(get());
    }

    // scoped_ptr::reset will delete the object.
    void reset(CacheRankingsBlock* p = NULL) {
      if (p != get())
        rankings_->FreeRankingsBlock(get());
      scoped_ptr<CacheRankingsBlock>::reset(p);
    }

   private:
    Rankings* rankings_;
    DISALLOW_EVIL_CONSTRUCTORS(ScopedRankingsBlock);
  };

  Rankings()
      : init_(false), head_(0), tail_(0) {}
  ~Rankings() {}

  bool Init(BackendImpl* backend);

  // Restores original state, leaving the object ready for initialization.
  void Reset();

  // Inserts a given entry at the head of the queue.
  void Insert(CacheRankingsBlock* node, bool modified);

  // Removes a given entry from the LRU list.
  void Remove(CacheRankingsBlock* node);

  // Moves a given entry to the head.
  void UpdateRank(CacheRankingsBlock* node, bool modified);

  // Iterates through the list.
  CacheRankingsBlock* GetNext(CacheRankingsBlock* node);
  CacheRankingsBlock* GetPrev(CacheRankingsBlock* node);
  void FreeRankingsBlock(CacheRankingsBlock* node);

  // Peforms a simple self-check of the list, and returns the number of items
  // or an error code (negative value).
  int SelfCheck();

  // Returns false if the entry is clearly invalid. from_list is true if the
  // node comes from the LRU list.
  bool SanityCheck(CacheRankingsBlock* node, bool from_list);

 private:
  typedef std::pair<CacheAddr, CacheRankingsBlock*> IteratorPair;
  typedef std::list<IteratorPair> IteratorList;

  Addr ReadHead();
  Addr ReadTail();
  void WriteHead();
  void WriteTail();

  // Gets the rankings information for a given rankings node.
  bool GetRanking(CacheRankingsBlock* rankings);

  // Finishes a list modification after a crash.
  void CompleteTransaction();
  void FinishInsert(CacheRankingsBlock* rankings);
  void RevertRemove(CacheRankingsBlock* rankings);

  // Returns false if this entry will not be recognized as dirty (called during
  // selfcheck).
  bool CheckEntry(CacheRankingsBlock* rankings);

  // Returns false if node is not properly linked.
  bool CheckLinks(CacheRankingsBlock* node, CacheRankingsBlock* prev,
                  CacheRankingsBlock* next);

  // Checks the links between two consecutive nodes.
  bool CheckSingleLink(CacheRankingsBlock* prev, CacheRankingsBlock* next);

  // Controls tracking of nodes used for enumerations.
  void TrackRankingsBlock(CacheRankingsBlock* node, bool start_tracking);

  // Updates the iterators whenever node is being changed.
  void UpdateIterators(CacheRankingsBlock* node);

  bool init_;
  Addr head_;
  Addr tail_;
  BlockFileHeader* header_;  // Header of the block-file used to store rankings.
  BackendImpl* backend_;
  IteratorList iterators_;

  DISALLOW_EVIL_CONSTRUCTORS(Rankings);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_RANKINGS_H__
