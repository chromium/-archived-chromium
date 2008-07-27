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

#include "webkit/glue/entity_map.h"

#include <hash_map>

#include "HTMLEntityCodes.c"

#include "base/string_util.h"

namespace webkit_glue {

typedef stdext::hash_map<wchar_t, const char*> EntityMapType;

class EntityMapData {
 public:
  EntityMapData(const EntityCode* entity_codes, int entity_codes_length,
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
  const EntityCode* entity_codes_;
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
    const EntityCode* entity_code = &entity_codes_[0];
    for (int i = 0; i < entity_codes_length_; ++i, ++entity_code)
      (*map_)[entity_code->code] = entity_code->name;
    if (user_number_entity_apos_)
      (*map_)[0x0027] = "&#39;";
  }
  return map_;
}

static const EntityCode xml_built_in_entity_codes[] = {
  {0x003c, "&lt;"},
  {0x003e, "&gt;"},
  {0x0026, "&amp;"},
  {0x0027, "&apos;"},
  {0x0022, "&quot;"}
};

static const int xml_entity_codes_length =
    arraysize(xml_built_in_entity_codes);

static EntityMapData html_entity_map_singleton(entity_codes,
                                               entity_codes_length,
                                               true);
static EntityMapData xml_entity_map_singleton(xml_built_in_entity_codes,
                                              xml_entity_codes_length,
                                              false);

const char* EntityMap::GetEntityNameByCode(wchar_t code, bool is_html) {
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
