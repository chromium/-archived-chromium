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


#ifndef O3D_CORE_CROSS_DRAW_LIST_MANAGER_H_
#define O3D_CORE_CROSS_DRAW_LIST_MANAGER_H_

#include <map>

#include "core/cross/service_implementation.h"

namespace o3d {

class DrawList;

class DrawListManager {
 public:
  static const InterfaceId kInterfaceId;

  explicit DrawListManager(ServiceLocator* service_locator);

  // Gets the lowest numbered unused DrawList index and adds it to the list of
  // used indices. These indices are used as an optimization at render time.
  // Then it registers the DrawList in the client's DrawList DB.
  // This is an internal function.
  // Parameters:
  //   draw_list: Pass list to register.
  // Returns:
  //   a global pass list index.
  unsigned int RegisterDrawList(DrawList* draw_list);

  // Removes the DrawList from the client's DrawList DB and frees its global
  // pass index that is no longer being used.
  // This is an internal function
  // Parameters:
  //   draw_list: Pass list to register.
  void UnregisterDrawList(DrawList* draw_list);

  // Clear all the pass lists so they don't hold pointers across frames.
  void Reset();

 private:
  ServiceImplementation<DrawListManager> service_;

  // Keep a map of DrawLists to indices for optimizaion and so we can clear them
  // each frame. A raw pointer is safe because the constructor/destructor for
  // DrawList keeps this array valid.
  typedef std::map<unsigned int, DrawList*> DrawListIndexMap;
  DrawListIndexMap draw_list_map_;
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_DRAW_LIST_MANAGER_H_
