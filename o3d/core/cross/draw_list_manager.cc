/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "core/cross/precompile.h"
#include "core/cross/draw_list_manager.h"
#include "core/cross/draw_list.h"

namespace o3d {

const InterfaceId DrawListManager::kInterfaceId =
    InterfaceTraits<DrawListManager>::kInterfaceId;

DrawListManager::DrawListManager(ServiceLocator* service_locator)
    : service_(service_locator, this) {}

unsigned int DrawListManager::RegisterDrawList(DrawList* draw_list) {
  // Find a free global id.
  unsigned int index = 0;
  DrawListIndexMap::iterator end(draw_list_map_.end());
  for (DrawListIndexMap::iterator iter(draw_list_map_.begin());
       iter != end;
       ++iter) {
    if (iter->first != index) {
      break;
    }
    ++index;
  }

  // Save the global id so we know it's used.
  draw_list_map_.insert(std::make_pair(index, draw_list));

  return index;
}

void DrawListManager::UnregisterDrawList(DrawList* draw_list) {
  // Remove it from the client's Pass List map.
  DrawListIndexMap::iterator iter = draw_list_map_.find(
      draw_list->global_index());
  DLOG_ASSERT(iter != draw_list_map_.end());
  draw_list_map_.erase(iter);
}

void DrawListManager::Reset() {
  DrawListIndexMap::iterator end(draw_list_map_.end());
  for (DrawListIndexMap::iterator iter(draw_list_map_.begin());
       iter != end;
       ++iter) {
    iter->second->Reset(Matrix4::identity(), Matrix4::identity());
  }
}
}  // namespace o3d
