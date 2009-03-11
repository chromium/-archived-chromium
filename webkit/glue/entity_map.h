// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_ENTITY_MAP_H__
#define WEBKIT_GLUE_ENTITY_MAP_H__

#include <string>

#include "base/basictypes.h"
#include "base/string_util.h"

namespace webkit_glue {

class EntityMap {
 public:
  // Check whether specified unicode has corresponding html or xml built-in
  // entity name. If yes, return the entity notation, if not then return NULL.
  // Parameter is_html indicates check the code in html entity map or in xml
  // entity map. THIS FUNCTION IS NOT THREADSAFE.
  static const char* GetEntityNameByCode(char16 code, bool is_html);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(EntityMap);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_ENTITY_MAP_H__
