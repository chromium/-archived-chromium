// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/entity_map.h"

#include "base/hash_tables.h"

namespace webkit_glue {

// Note that this file is also included by HTMLTokenizer.cpp so we are getting
// two copies of the data in memory.  We can fix this by changing the script
// that generated the array to create a static const that is its length, but
// this is low priority since the data is less than 4K.
#include "HTMLEntityNames.c"

typedef base::hash_map<char16, const char*> EntityMapType;

class EntityMapData {
 public:
  EntityMapData(const Entity* entity_codes, int entity_codes_length,
                bool user_number_entity_apos)
      : entity_codes_(entity_codes),
        entity_codes_length_(entity_codes_length),
        user_number_entity_apos_(user_number_entity_apos),
        map_(NULL) {
  }
  ~EntityMapData() { delete map_; }
  const EntityMapType* GetEntityMapData();

 private:
  // Data structure which saves all pairs of Unicode character and its
  // corresponding entity notation.
  const Entity* entity_codes_;
  const int entity_codes_length_;
  // IE does not support '&apos;'(html entity name for symbol ') as HTML entity
  // (but it does support &apos; as xml entity), so if user_number_entity_apos_
  // is true, we will use entity number 0x0027 instead of using '&apos;' when
  // doing serialization work.
  const bool user_number_entity_apos_;
  // Map the Unicode character to corresponding entity notation.
  EntityMapType* map_;

  DISALLOW_EVIL_CONSTRUCTORS(EntityMapData);
};

const EntityMapType* EntityMapData::GetEntityMapData() {
  if (!map_) {
    // lazily create the entity map.
    map_ = new EntityMapType;
    const Entity* entity_code = &entity_codes_[0];
    for (int i = 0; i < entity_codes_length_; ++i, ++entity_code) {
      // For consistency, use lower case for entity codes that have both.
      EntityMapType::const_iterator it = map_->find(entity_code->code);
      if (it != map_->end() &&
          StringToLowerASCII(std::string(entity_code->name)) == it->second)
        continue;

      (*map_)[entity_code->code] = entity_code->name;
    }
    if (user_number_entity_apos_)
      (*map_)[0x0027] = "#39";
  }
  return map_;
}

static const Entity xml_built_in_entity_codes[] = {
  {"lt", 0x003c},
  {"gt", 0x003e},
  {"amp", 0x0026},
  {"apos", 0x0027},
  {"quot", 0x0022}
};

const char* EntityMap::GetEntityNameByCode(char16 code, bool is_html) {
  static EntityMapData html_entity_map_singleton(
      wordlist, sizeof(wordlist) / sizeof(Entity), true);
  static EntityMapData xml_entity_map_singleton(
      xml_built_in_entity_codes, arraysize(xml_built_in_entity_codes), false);

  const EntityMapType* entity_map;
  if (is_html)
    entity_map = html_entity_map_singleton.GetEntityMapData();
  else
    entity_map = xml_entity_map_singleton.GetEntityMapData();

  // Check entity name according to unicode.
  EntityMapType::const_iterator i = entity_map->find(code);
  if (i == entity_map->end())
    // Not found, return NULL.
    return NULL;
  else
    // Found, return entity notation.
    return i->second;
}

}  // namespace webkit_glue
