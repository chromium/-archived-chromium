// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/global_descriptors_posix.h"

#include <vector>
#include <utility>

#include "base/logging.h"

namespace base {

int GlobalDescriptors::MaybeGet(Key key) const {
  for (Mapping::const_iterator
       i = descriptors_.begin(); i != descriptors_.end(); ++i) {
    if (i->first == key)
      return i->second;
  }

  // In order to make unittests pass, we define a default mapping from keys to
  // descriptors by adding a fixed offset:
  return kBaseDescriptor + key;
}

int GlobalDescriptors::Get(Key key) const {
  const int ret = MaybeGet(key);

  if (ret == -1)
    LOG(FATAL) << "Unknown global descriptor: " << key;
  return ret;
}

void GlobalDescriptors::Set(Key key, int fd) {
  for (Mapping::iterator
       i = descriptors_.begin(); i != descriptors_.end(); ++i) {
    if (i->first == key) {
      i->second = fd;
      return;
    }
  }

  descriptors_.push_back(std::make_pair(key, fd));
}

}  // namespace base
