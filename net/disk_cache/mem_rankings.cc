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

#include "net/disk_cache/mem_rankings.h"

#include "net/disk_cache/mem_entry_impl.h"

namespace disk_cache {

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
