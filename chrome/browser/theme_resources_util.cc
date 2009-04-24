// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/theme_resources_util.h"

#include "base/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "grit/theme_resources_map.h"

#include <utility>

namespace {

// A wrapper class that holds a hash_map between resource strings and resource
// ids.  This is done so we can use base::LazyInstance which takes care of
// thread safety in initializing the hash_map for us.
class ThemeMap {
 public:
  typedef base::hash_map<std::string, int> StringIntMap;

  ThemeMap() {
    for (size_t i = 0; i < kThemeResourcesSize; ++i) {
      id_map_[kThemeResources[i].name] = kThemeResources[i].value;
    }
  }

  int GetId(const std::string& resource_name) {
    StringIntMap::const_iterator it = id_map_.find(resource_name);
    if (it == id_map_.end())
      return -1;
    return it->second;
  }

 private:
  StringIntMap id_map_;
};

static base::LazyInstance<ThemeMap> g_theme_ids(base::LINKER_INITIALIZED);

}  // namespace

int ThemeResourcesUtil::GetId(const std::string& resource_name) {
  return g_theme_ids.Get().GetId(resource_name);
}
