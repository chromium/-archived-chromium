// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/mem_rankings.h"

#include "base/logging.h"
#include "net/disk_cache/mem_entry_impl.h"

namespace disk_cache {

MemRankings::~MemRankings() {
  DCHECK(!head_ && !tail_);
}

void MemRankings::Insert(MemEntryImpl* node) {
  if (head_)
    head_->set_prev(node);

  if (!tail_)
    tail_ = node;

  node->set_prev(NULL);
  node->set_next(head_);
  head_ = node;
}

void MemRankings::Remove(MemEntryImpl* node) {
  MemEntryImpl* prev = node->prev();
  MemEntryImpl* next = node->next();

  if (head_ == node)
    head_ = next;

  if (tail_ == node)
    tail_ = prev;

  if (prev)
    prev->set_next(next);

  if (next)
    next->set_prev(prev);

  node->set_next(NULL);
  node->set_prev(NULL);
}

void MemRankings::UpdateRank(MemEntryImpl* node) {
  Remove(node);
  Insert(node);
}

MemEntryImpl* MemRankings::GetNext(MemEntryImpl* node) {
  if (!node)
    return head_;

  return node->next();
}

MemEntryImpl* MemRankings::GetPrev(MemEntryImpl* node) {
  if (!node)
    return tail_;

  return node->prev();
}

}  // namespace disk_cache
