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


#ifndef O3D_CORE_CROSS_SEMANTIC_MANAGER_H_
#define O3D_CORE_CROSS_SEMANTIC_MANAGER_H_

#include <build/build_config.h>
#include <map>

#include "core/cross/types.h"
#include "core/cross/service_implementation.h"
#include "core/cross/object_base.h"
#include "core/cross/param_object.h"

namespace o3d {

// Provides Classes associated with semantic names.
class SemanticManager {
 public:
  static const InterfaceId kInterfaceId;

  explicit SemanticManager(ServiceLocator* service_locator);
  ~SemanticManager();

  // Gets the SAS param object.
  ParamObject* sas_param_object() {
    return sas_param_object_.Get();
  }

  // Looks up an SAS transform semantic by name, and returns the class type.
  const ObjectBase::Class* LookupSemantic(const String& semantic_name) const;

 private:
  ServiceImplementation<SemanticManager> service_;

  // A case-insensitive string comparator.
  struct lesscasecmp {
    bool operator() (const String& a, const String& b) const {
      // TODO : This is doing ASCII compare - not UTF8!
#ifdef OS_WIN
      return(_stricmp(a.c_str(), b.c_str()) < 0);
#else
      return(strcasecmp(a.c_str(), b.c_str()) < 0);
#endif
    }
  };

  // Object to hold one of each Sas param to make it easy
  // to cache them without adding them to any other param object.
  ParamObject::Ref sas_param_object_;

  // This is a a case-insensitive map between strings and Param semantics.
  typedef std::map<String, const ObjectBase::Class*, lesscasecmp> SasMap;
  SasMap sas_map_;
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_SEMANTIC_MANAGER_H_
