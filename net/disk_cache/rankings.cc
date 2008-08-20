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

#include "net/disk_cache/rankings.h"

#include "base/histogram.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/entry_impl.h"
#include "net/disk_cache/errors.h"

// This is used by crash_cache.exe to generate unit test files.
disk_cache::RankCrashes g_rankings_crash = disk_cache::NO_CRASH;

namespace {

const int kHeadIndex = 0;
const int kTailIndex = 1;
const int kTransactionIndex = 2;
const int kOperationIndex = 3;

enum Operation {
  INSERT = 1,
  REMOVE
};

// This class provides a simple lock for the LRU list of rankings. Whenever an
// entry is to be inserted or removed from the list, a transaction object should
// be created to keep track of the operation. If the process crashes before
// finishing the operation, the transaction record (stored as part of the user
// data on the file header) can be used to finish the operation.
class Transaction {
 public:
  // addr is the cache addres of the node being inserted or removed. We want to
  // avoid having the compiler doing optimizations on when to read or write
  // from user_data because it is the basis of the crash detection. Maybe
  // volatile is not enough for that, but it should be a good hint.
  Transaction(volatile int32* user_data, disk_cache::Addr addr, Operation op);
  ~Transaction();
 private:
  volatile int32* user_data_;
  DISALLOW_EVIL_CONSTRUCTORS(Transaction);
};

Transaction::Transaction(volatile int32* user_data, disk_cache::Addr addr,
                         Operation op) : user_data_(user_data) {
  DCHECK(!user_data_[kTransactionIndex]);
  DCHECK(addr.is_initialized());
  user_data_[kOperationIndex] = op;
  user_data_[kTransactionIndex] = static_cast<int32>(addr.value());
}

Transaction::~Transaction() {
  DCHECK(user_data_[kTransactionIndex]);
  user_data_[kTransactionIndex] = 0;
  user_data_[kOperationIndex] = 0;
}

// Code locations that can generate crashes.
enum CrashLocation {
  ON_INSERT_1, ON_INSERT_2, ON_INSERT_3, ON_INSERT_4, ON_REMOVE_1, ON_REMOVE_2,
  ON_REMOVE_3, ON_REMOVE_4, ON_REMOVE_5, ON_REMOVE_6, ON_REMOVE_7, ON_REMOVE_8
};

// Generates a crash on debug builds, acording to the value of g_rankings_crash.
// This used by crash_cache.exe to generate unit-test files.
void GenerateCrash(CrashLocation location) {
#if defined(OS_WIN)
#ifndef NDEBUG
  if (disk_cache::NO_CRASH == g_rankings_crash)
    return;
  switch (location) {
    case ON_INSERT_1:
      switch (g_rankings_crash) {
        case disk_cache::INSERT_ONE_1:
        case disk_cache::INSERT_LOAD_1:
          TerminateProcess(GetCurrentProcess(), 0);
      }
      break;
    case ON_INSERT_2:
      if (disk_cache::INSERT_EMPTY_1 == g_rankings_crash)
          TerminateProcess(GetCurrentProcess(), 0);
      break;
    case ON_INSERT_3:
      switch (g_rankings_crash) {
        case disk_cache::INSERT_EMPTY_2:
        case disk_cache::INSERT_ONE_2:
        case disk_cache::INSERT_LOAD_2:
          TerminateProcess(GetCurrentProcess(), 0);
      }
      break;
    case ON_INSERT_4:
      switch (g_rankings_crash) {
        case disk_cache::INSERT_EMPTY_3:
        case disk_cache::INSERT_ONE_3:
          TerminateProcess(GetCurrentProcess(), 0);
      }
      break;
    case ON_REMOVE_1:
      switch (g_rankings_crash) {
        case disk_cache::REMOVE_ONE_1:
        case disk_cache::REMOVE_HEAD_1:
        case disk_cache::REMOVE_TAIL_1:
        case disk_cache::REMOVE_LOAD_1:
          TerminateProcess(GetCurrentProcess(), 0);
      }
      break;
    case ON_REMOVE_2:
      if (disk_cache::REMOVE_ONE_2 == g_rankings_crash)
          TerminateProcess(GetCurrentProcess(), 0);
      break;
    case ON_REMOVE_3:
      if (disk_cache::REMOVE_ONE_3 == g_rankings_crash)
          TerminateProcess(GetCurrentProcess(), 0);
      break;
    case ON_REMOVE_4:
      if (disk_cache::REMOVE_HEAD_2 == g_rankings_crash)
          TerminateProcess(GetCurrentProcess(), 0);
      break;
    case ON_REMOVE_5:
      if (disk_cache::REMOVE_TAIL_2 == g_rankings_crash)
          TerminateProcess(GetCurrentProcess(), 0);
      break;
    case ON_REMOVE_6:
      if (disk_cache::REMOVE_TAIL_3 == g_rankings_crash)
          TerminateProcess(GetCurrentProcess(), 0);
      break;
    case ON_REMOVE_7:
      switch (g_rankings_crash) {
        case disk_cache::REMOVE_ONE_4:
        case disk_cache::REMOVE_LOAD_2:
        case disk_cache::REMOVE_HEAD_3:
          TerminateProcess(GetCurrentProcess(), 0);
      }
      break;
    case ON_REMOVE_8:
      switch (g_rankings_crash) {
        case disk_cache::REMOVE_HEAD_4:
        case disk_cache::REMOVE_LOAD_3:
          TerminateProcess(GetCurrentProcess(), 0);
      }
      break;
    default:
      NOTREACHED();
      return;
  }
#endif  // NDEBUG
#endif  // OS_WIN
}

}  // namespace

namespace disk_cache {

bool Rankings::Init(BackendImpl* backend) {
  DCHECK(!init_);
  if (init_)
    return false;

  backend_ = backend;
  MappedFile* file = backend_->File(Addr(RANKINGS, 0, 0, 0));

  header_ = reinterpret_cast<BlockFileHeader*>(file->buffer());

  head_ = ReadHead();
  tail_ = ReadTail();

  if (header_->user[kTransactionIndex])
    CompleteTransaction();

  init_ = true;
  return true;
}

void Rankings::Reset() {
  init_ = false;
  head_.set_value(0);
  tail_.set_value(0);
  header_ = NULL;
}

bool Rankings::GetRanking(CacheRankingsBlock* rankings) {
  Time start = Time::Now();
  if (!rankings->address().is_initialized())
    return false;

  if (!rankings->Load())
    return false;

  if (!SanityCheck(rankings, true)) {
    backend_->CriticalError(ERR_INVALID_LINKS);
    return false;
  }

  if (!rankings->Data()->pointer) {
    backend_->OnEvent(Stats::GET_RANKINGS);
    return true;
  }

  backend_->OnEvent(Stats::OPEN_RANKINGS);

  if (backend_->GetCurrentEntryId() != rankings->Data()->dirty) {
    // We cannot trust this entry, but we cannot initiate a cleanup from this
    // point (we may be in the middle of a cleanup already). Just get rid of
    // the invalid pointer and continue; the entry will be deleted when detected
    // from a regular open/create path.
    rankings->Data()->pointer = NULL;
    return true;
  }

  EntryImpl* cache_entry =
      reinterpret_cast<EntryImpl*>(rankings->Data()->pointer);
  rankings->SetData(cache_entry->rankings()->Data());
  UMA_HISTOGRAM_TIMES(L"DiskCache.GetRankings", Time::Now() - start);
  return true;
}

void Rankings::Insert(CacheRankingsBlock* node, bool modified) {
  Trace("Insert 0x%x", node->address().value());
  DCHECK(node->HasData());
  Transaction lock(header_->user, node->address(), INSERT);
  CacheRankingsBlock head(backend_->File(head_), head_);
  if (head_.is_initialized()) {
    if (!GetRanking(&head))
      return;

    if (head.Data()->prev != head_.value() &&  // Normal path.
        head.Data()->prev != node->address().value()) {  // FinishInsert().
      backend_->CriticalError(ERR_INVALID_LINKS);
      return;
    }

    head.Data()->prev = node->address().value();
    head.Store();
    GenerateCrash(ON_INSERT_1);
    UpdateIterators(&head);
  }

  node->Data()->next = head_.value();
  node->Data()->prev = node->address().value();
  head_.set_value(node->address().value());

  if (!tail_.is_initialized() || tail_.value() == node->address().value()) {
    tail_.set_value(node->address().value());
    node->Data()->next = tail_.value();
    WriteTail();
    GenerateCrash(ON_INSERT_2);
  }

  Time now = Time::Now();
  node->Data()->last_used = now.ToInternalValue();
  if (modified)
    node->Data()->last_modified = now.ToInternalValue();
  node->Store();
  GenerateCrash(ON_INSERT_3);

  // The last thing to do is move our head to point to a node already stored.
  WriteHead();
  GenerateCrash(ON_INSERT_4);
}

// If a, b and r are elements on the list, and we want to remove r, the possible
// states for the objects if a crash happens are (where y(x, z) means for object
// y, prev is x and next is z):
// A. One element:
//    1. r(r, r), head(r), tail(r)                    initial state
//    2. r(r, r), head(0), tail(r)                    WriteHead()
//    3. r(r, r), head(0), tail(0)                    WriteTail()
//    4. r(0, 0), head(0), tail(0)                    next.Store()
//
// B. Remove a random element:
//    1. a(x, r), r(a, b), b(r, y), head(x), tail(y)  initial state
//    2. a(x, r), r(a, b), b(a, y), head(x), tail(y)  next.Store()
//    3. a(x, b), r(a, b), b(a, y), head(x), tail(y)  prev.Store()
//    4. a(x, b), r(0, 0), b(a, y), head(x), tail(y)  node.Store()
//
// C. Remove head:
//    1. r(r, b), b(r, y), head(r), tail(y)           initial state
//    2. r(r, b), b(r, y), head(b), tail(y)           WriteHead()
//    3. r(r, b), b(b, y), head(b), tail(y)           next.Store()
//    4. r(0, 0), b(b, y), head(b), tail(y)           prev.Store()
//
// D. Remove tail:
//    1. a(x, r), r(a, r), head(x), tail(r)           initial state
//    2. a(x, r), r(a, r), head(x), tail(a)           WriteTail()
//    3. a(x, a), r(a, r), head(x), tail(a)           prev.Store()
//    4. a(x, a), r(0, 0), head(x), tail(a)           next.Store()
void Rankings::Remove(CacheRankingsBlock* node) {
  Trace("Remove 0x%x (0x%x 0x%x)", node->address().value(), node->Data()->next,
        node->Data()->prev);
  DCHECK(node->HasData());
  Addr next_addr(node->Data()->next);
  Addr prev_addr(node->Data()->prev);
  if (!next_addr.is_initialized() || next_addr.is_separate_file() ||
      !prev_addr.is_initialized() || prev_addr.is_separate_file()) {
    LOG(WARNING) << "Invalid rankings info.";
    return;
  }

  CacheRankingsBlock next(backend_->File(next_addr), next_addr);
  CacheRankingsBlock prev(backend_->File(prev_addr), prev_addr);
  if (!GetRanking(&next) || !GetRanking(&prev))
    return;

  if (!CheckLinks(node, &prev, &next))
    return;

  Transaction lock(header_->user, node->address(), REMOVE);
  prev.Data()->next = next.address().value();
  next.Data()->prev = prev.address().value();
  GenerateCrash(ON_REMOVE_1);

  CacheAddr node_value = node->address().value();
  if (node_value == head_.value() || node_value == tail_.value()) {
    if (head_.value() == tail_.value()) {
      head_.set_value(0);
      tail_.set_value(0);

      WriteHead();
      GenerateCrash(ON_REMOVE_2);
      WriteTail();
      GenerateCrash(ON_REMOVE_3);
    } else if (node_value == head_.value()) {
      head_.set_value(next.address().value());
      next.Data()->prev = next.address().value();

      WriteHead();
      GenerateCrash(ON_REMOVE_4);
    } else if (node_value == tail_.value()) {
      tail_.set_value(prev.address().value());
      prev.Data()->next = prev.address().value();

      WriteTail();
      GenerateCrash(ON_REMOVE_5);

      // Store the new tail to make sure we can undo the operation if we crash.
      prev.Store();
      GenerateCrash(ON_REMOVE_6);
    }
  }

  // Nodes out of the list can be identified by invalid pointers.
  node->Data()->next = 0;
  node->Data()->prev = 0;

  // The last thing to get to disk is the node itself, so before that there is
  // enough info to recover.
  next.Store();
  GenerateCrash(ON_REMOVE_7);
  prev.Store();
  GenerateCrash(ON_REMOVE_8);
  node->Store();
  UpdateIterators(&next);
  UpdateIterators(&prev);
}

// A crash in between Remove and Insert will lead to a dirty entry not on the
// list. We want to avoid that case as much as we can (as while waiting for IO),
// but the net effect is just an assert on debug when attempting to remove the
// entry. Otherwise we'll need reentrant transactions, which is an overkill.
void Rankings::UpdateRank(CacheRankingsBlock* node, bool modified) {
  Time start = Time::Now();
  Remove(node);
  Insert(node, modified);
  UMA_HISTOGRAM_TIMES(L"DiskCache.UpdateRank", Time::Now() - start);
}

void Rankings::CompleteTransaction() {
  Addr node_addr(static_cast<CacheAddr>(header_->user[kTransactionIndex]));
  if (!node_addr.is_initialized() || node_addr.is_separate_file()) {
    NOTREACHED();
    LOG(ERROR) << "Invalid rankings info.";
    return;
  }

  Trace("CompleteTransaction 0x%x", node_addr.value());

  CacheRankingsBlock node(backend_->File(node_addr), node_addr);
  if (!node.Load())
    return;

  node.Data()->pointer = NULL;
  node.Store();

  // We want to leave the node inside the list. The entry must me marked as
  // dirty, and will be removed later. Otherwise, we'll get assertions when
  // attempting to remove the dirty entry.
  if (INSERT == header_->user[kOperationIndex]) {
    Trace("FinishInsert h:0x%x t:0x%x", head_.value(), tail_.value());
    FinishInsert(&node);
  } else if (REMOVE == header_->user[kOperationIndex]) {
    Trace("RevertRemove h:0x%x t:0x%x", head_.value(), tail_.value());
    RevertRemove(&node);
  } else {
    NOTREACHED();
    LOG(ERROR) << "Invalid operation to recover.";
  }
}

void Rankings::FinishInsert(CacheRankingsBlock* node) {
  header_->user[kTransactionIndex] = 0;
  header_->user[kOperationIndex] = 0;
  if (head_.value() != node->address().value()) {
    if (tail_.value() == node->address().value()) {
      // This part will be skipped by the logic of Insert.
      node->Data()->next = tail_.value();
    }

    Insert(node, true);
  }

  // Tell the backend about this entry.
  backend_->RecoveredEntry(node);
}

void Rankings::RevertRemove(CacheRankingsBlock* node) {
  Addr next_addr(node->Data()->next);
  Addr prev_addr(node->Data()->prev);
  if (!next_addr.is_initialized() || !prev_addr.is_initialized()) {
    // The operation actually finished. Nothing to do.
    header_->user[kTransactionIndex] = 0;
    return;
  }
  if (next_addr.is_separate_file() || prev_addr.is_separate_file()) {
    NOTREACHED();
    LOG(WARNING) << "Invalid rankings info.";
    header_->user[kTransactionIndex] = 0;
    return;
  }

  CacheRankingsBlock next(backend_->File(next_addr), next_addr);
  CacheRankingsBlock prev(backend_->File(prev_addr), prev_addr);
  if (!next.Load() || !prev.Load())
    return;

  CacheAddr node_value = node->address().value();
  DCHECK(prev.Data()->next == node_value ||
         prev.Data()->next == prev_addr.value() ||
         prev.Data()->next == next.address().value());
  DCHECK(next.Data()->prev == node_value ||
         next.Data()->prev == next_addr.value() ||
         next.Data()->prev == prev.address().value());

  if (node_value != prev_addr.value())
    prev.Data()->next = node_value;
  if (node_value != next_addr.value())
    next.Data()->prev = node_value;

  if (!head_.is_initialized() || !tail_.is_initialized()) {
    head_.set_value(node_value);
    tail_.set_value(node_value);
    WriteHead();
    WriteTail();
  } else if (head_.value() == next.address().value()) {
    head_.set_value(node_value);
    prev.Data()->next = next.address().value();
    WriteHead();
  } else if (tail_.value() == prev.address().value()) {
    tail_.set_value(node_value);
    next.Data()->prev = prev.address().value();
    WriteTail();
  }

  next.Store();
  prev.Store();
  header_->user[kTransactionIndex] = 0;
  header_->user[kOperationIndex] = 0;
}

CacheRankingsBlock* Rankings::GetNext(CacheRankingsBlock* node) {
  ScopedRankingsBlock next(this);
  if (!node) {
    if (!head_.is_initialized())
      return NULL;
    next.reset(new CacheRankingsBlock(backend_->File(head_), head_));
  } else {
    if (!tail_.is_initialized())
      return NULL;
    if (tail_.value() == node->address().value())
      return NULL;
    Addr address(node->Data()->next);
    next.reset(new CacheRankingsBlock(backend_->File(address), address));
  }

  TrackRankingsBlock(next.get(), true);

  if (!GetRanking(next.get()))
    return NULL;

  if (node && !CheckSingleLink(node, next.get()))
    return NULL;

  return next.release();
}

CacheRankingsBlock* Rankings::GetPrev(CacheRankingsBlock* node) {
  ScopedRankingsBlock prev(this);
  if (!node) {
    if (!tail_.is_initialized())
      return NULL;
    prev.reset(new CacheRankingsBlock(backend_->File(tail_), tail_));
  } else {
    if (!head_.is_initialized())
      return NULL;
    if (head_.value() == node->address().value())
      return NULL;
    Addr address(node->Data()->prev);
    prev.reset(new CacheRankingsBlock(backend_->File(address), address));
  }

  TrackRankingsBlock(prev.get(), true);

  if (!GetRanking(prev.get()))
    return NULL;

  if (node && !CheckSingleLink(prev.get(), node))
    return NULL;

  return prev.release();
}

void Rankings::FreeRankingsBlock(CacheRankingsBlock* node) {
  TrackRankingsBlock(node, false);
}

int Rankings::SelfCheck() {
  if (!head_.is_initialized()) {
    if (!tail_.is_initialized())
      return 0;
    return ERR_INVALID_TAIL;
  }
  if (!tail_.is_initialized())
    return ERR_INVALID_HEAD;

  if (tail_.is_separate_file())
    return ERR_INVALID_TAIL;

  if (head_.is_separate_file())
    return ERR_INVALID_HEAD;

  int num_items = 0;
  Addr address(head_.value());
  Addr prev(head_.value());
  scoped_ptr<CacheRankingsBlock> node;
  do {
    node.reset(new CacheRankingsBlock(backend_->File(address), address));
    node->Load();
    if (node->Data()->prev != prev.value())
      return ERR_INVALID_PREV;
    if (!CheckEntry(node.get()))
      return ERR_INVALID_ENTRY;

    prev.set_value(address.value());
    address.set_value(node->Data()->next);
    if (!address.is_initialized() || address.is_separate_file())
      return ERR_INVALID_NEXT;

    num_items++;
  } while (node->address().value() != address.value());
  return num_items;
}

bool Rankings::SanityCheck(CacheRankingsBlock* node, bool from_list) {
  const RankingsNode* data = node->Data();
  if (!data->contents)
    return false;

  // It may have never been inserted.
  if (from_list && (!data->last_used || !data->last_modified))
    return false;

  if ((!data->next && data->prev) || (data->next && !data->prev))
    return false;

  // Both pointers on zero is a node out of the list.
  if (!data->next && !data->prev && from_list)
    return false;

  if ((node->address().value() == data->prev) && (head_.value() != data->prev))
    return false;

  if ((node->address().value() == data->next) && (tail_.value() != data->next))
    return false;

  return true;
}

Addr Rankings::ReadHead() {
  CacheAddr head = static_cast<CacheAddr>(header_->user[kHeadIndex]);
  return Addr(head);
}

Addr Rankings::ReadTail() {
  CacheAddr tail = static_cast<CacheAddr>(header_->user[kTailIndex]);
  return Addr(tail);
}

void Rankings::WriteHead() {
  header_->user[kHeadIndex] = static_cast<int32>(head_.value());
}

void Rankings::WriteTail() {
  header_->user[kTailIndex] = static_cast<int32>(tail_.value());
}

bool Rankings::CheckEntry(CacheRankingsBlock* rankings) {
  if (!rankings->Data()->pointer)
    return true;

  // If this entry is not dirty, it is a serious problem.
  return backend_->GetCurrentEntryId() != rankings->Data()->dirty;
}

bool Rankings::CheckLinks(CacheRankingsBlock* node, CacheRankingsBlock* prev,
                          CacheRankingsBlock* next) {
  if ((prev->Data()->next != node->address().value() &&
       head_.value() != node->address().value()) ||
      (next->Data()->prev != node->address().value() &&
       tail_.value() != node->address().value())) {
    LOG(ERROR) << "Inconsistent LRU.";

    if (prev->Data()->next == next->address().value() &&
        next->Data()->prev == prev->address().value()) {
      // The list is actually ok, node is wrong.
      node->Data()->next = 0;
      node->Data()->prev = 0;
      node->Store();
      return false;
    }
    backend_->CriticalError(ERR_INVALID_LINKS);
    return false;
  }

  return true;
}

bool Rankings::CheckSingleLink(CacheRankingsBlock* prev,
                               CacheRankingsBlock* next) {
  if (prev->Data()->next != next->address().value() ||
      next->Data()->prev != prev->address().value()) {
    LOG(ERROR) << "Inconsistent LRU.";

    backend_->CriticalError(ERR_INVALID_LINKS);
    return false;
  }

  return true;
}

void Rankings::TrackRankingsBlock(CacheRankingsBlock* node,
                                  bool start_tracking) {
  if (!node)
    return;

  IteratorPair current(node->address().value(), node);

  if (start_tracking)
    iterators_.push_back(current);
  else
    iterators_.remove(current);
}

// We expect to have just a few iterators at any given time, maybe two or three,
// But we could have more than one pointing at the same mode. We walk the list
// of cache iterators and update all that are pointing to the given node.
void Rankings::UpdateIterators(CacheRankingsBlock* node) {
  CacheAddr address = node->address().value();
  for (IteratorList::iterator it = iterators_.begin(); it != iterators_.end();
       ++it) {
    if (it->first == address) {
      CacheRankingsBlock* other = it->second;
      other->Data()->next = node->Data()->next;
      other->Data()->prev = node->Data()->prev;
    }
  }
}

}  // namespace disk_cache
