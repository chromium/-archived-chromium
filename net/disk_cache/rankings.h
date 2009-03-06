// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface.

#ifndef NET_DISK_CACHE_RANKINGS_H_
#define NET_DISK_CACHE_RANKINGS_H_

#include <list>

#include "base/scoped_ptr.h"
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
  // Possible lists of entries.
  enum List {
    NO_USE = 0,   // List of entries that have not been reused.
    LOW_USE,      // List of entries with low reuse.
    HIGH_USE,     // List of entries with high reuse.
    RESERVED,     // Reserved for future use.
    DELETED,      // List of recently deleted or doomed entries.
    LAST_ELEMENT
  };

  // This class provides a specialized version of scoped_ptr, that calls
  // Rankings whenever a CacheRankingsBlock is deleted, to keep track of cache
  // iterators that may go stale.
  class ScopedRankingsBlock : public scoped_ptr<CacheRankingsBlock> {
   public:
    ScopedRankingsBlock() : rankings_(NULL) {}
    explicit ScopedRankingsBlock(Rankings* rankings) : rankings_(rankings) {}
    ScopedRankingsBlock(Rankings* rankings, CacheRankingsBlock* node)
        : scoped_ptr<CacheRankingsBlock>(node), rankings_(rankings) {}

    ~ScopedRankingsBlock() {
      rankings_->FreeRankingsBlock(get());
    }

    void set_rankings(Rankings* rankings) {
      rankings_ = rankings;
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

  // If we have multiple lists, we have to iterate through all at the same time.
  // This structure keeps track of where we are on the iteration.
  struct Iterator {
    List list;                     // Which entry was returned to the user.
    CacheRankingsBlock* nodes[3];  // Nodes on the first three lists.
    Rankings* my_rankings;
    Iterator(Rankings* rankings) {
      memset(this, 0, sizeof(Iterator));
      my_rankings = rankings;
    }
    ~Iterator() {
      for (int i = 0; i < 3; i++)
        ScopedRankingsBlock(my_rankings, nodes[i]);
    }
  };

  Rankings() : init_(false) {}
  ~Rankings() {}

  bool Init(BackendImpl* backend, bool count_lists);

  // Restores original state, leaving the object ready for initialization.
  void Reset();

  // Inserts a given entry at the head of the queue.
  void Insert(CacheRankingsBlock* node, bool modified, List list);

  // Removes a given entry from the LRU list.
  void Remove(CacheRankingsBlock* node, List list);

  // Moves a given entry to the head.
  void UpdateRank(CacheRankingsBlock* node, bool modified, List list);

  // Iterates through the list.
  CacheRankingsBlock* GetNext(CacheRankingsBlock* node, List list);
  CacheRankingsBlock* GetPrev(CacheRankingsBlock* node, List list);
  void FreeRankingsBlock(CacheRankingsBlock* node);

  // Peforms a simple self-check of the lists, and returns the number of items
  // or an error code (negative value).
  int SelfCheck();

  // Returns false if the entry is clearly invalid. from_list is true if the
  // node comes from the LRU list.
  bool SanityCheck(CacheRankingsBlock* node, bool from_list);

 private:
  typedef std::pair<CacheAddr, CacheRankingsBlock*> IteratorPair;
  typedef std::list<IteratorPair> IteratorList;

  void ReadHeads();
  void ReadTails();
  void WriteHead(List list);
  void WriteTail(List list);

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
                  CacheRankingsBlock* next, List list);

  // Checks the links between two consecutive nodes.
  bool CheckSingleLink(CacheRankingsBlock* prev, CacheRankingsBlock* next);

  // Peforms a simple check of the list, and returns the number of items or an
  // error code (negative value).
  int CheckList(List list);

  // Returns true if addr is the head or tail of any list.
  bool IsHead(CacheAddr addr);
  bool IsTail(CacheAddr addr);

  // Controls tracking of nodes used for enumerations.
  void TrackRankingsBlock(CacheRankingsBlock* node, bool start_tracking);

  // Updates the iterators whenever node is being changed.
  void UpdateIterators(CacheRankingsBlock* node);

  // Keeps track of the number of entries on a list.
  void IncrementCounter(List list);
  void DecrementCounter(List list);

  bool init_;
  bool count_lists_;
  Addr heads_[LAST_ELEMENT];
  Addr tails_[LAST_ELEMENT];
  BackendImpl* backend_;
  LruData* control_data_;  // Data related to the LRU lists.
  IteratorList iterators_;

  DISALLOW_COPY_AND_ASSIGN(Rankings);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_RANKINGS_H_

