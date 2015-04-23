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


// This file contains the implementation of ResourceMapBase.

#include "command_buffer/service/cross/resource.h"

namespace o3d {
namespace command_buffer {

// Assigns a resource to a resource ID, by setting it at the right location
// into the list, resizing the list if necessary, and destroying an existing
// resource if one existed already.
void ResourceMapBase::Assign(ResourceID id, Resource *resource) {
  if (id >= resources_.size()) {
    resources_.resize(id + 1, NULL);
  } else {
    Resource *&entry = resources_[id];
    if (entry) {
      delete entry;
      entry = NULL;
    }
  }
  DCHECK(resources_[id] == NULL);
  resources_[id] = resource;
}

// Destroys a resource contained in the map, setting its entry to NULL. If
// necessary, this will trim the list.
bool ResourceMapBase::Destroy(ResourceID id) {
  if (id >= resources_.size()) {
    return false;
  }
  Resource *&entry = resources_[id];
  if (entry) {
    delete entry;
    entry = NULL;

    // Removing the last element, we can trim the list.
    // TODO: this may not be optimal to do every time. Investigate if it
    // becomes an issue, and add a threshold before we resize.
    if (id == resources_.size() - 1) {
      size_t last_valid = resources_.max_size();
      for (unsigned int i = id; i < resources_.size(); --i) {
        if (resources_[i]) {
          last_valid = i;
          break;
        }
      }
      if (last_valid == resources_.max_size()) {
        resources_.clear();
      } else {
        resources_.resize(last_valid + 1);
      }
    }
    return true;
  }
  return false;
}

// Goes over all non-NULL entries in the list, destroying them, then clears the
// list.
void ResourceMapBase::DestroyAllResources() {
  for (Container::iterator i = resources_.begin(); i != resources_.end(); ++i) {
    if (*i) {
      delete *i;
    }
  }
  resources_.clear();
}

}  // namespace command_buffer
}  // namespace o3d
