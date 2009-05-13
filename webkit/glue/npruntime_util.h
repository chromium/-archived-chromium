// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_NPRUNTIME_UTIL_H_
#define WEBKIT_GLUE_NPRUNTIME_UTIL_H_

#include "third_party/npapi/bindings/npruntime.h"

class Pickle;

namespace webkit_glue {

// Efficiently serialize/deserialize a NPIdentifier
bool SerializeNPIdentifier(NPIdentifier identifier, Pickle* pickle);
bool DeserializeNPIdentifier(const Pickle& pickle, void** pickle_iter,
                             NPIdentifier* identifier);

// Validate drag and drop events access the data. TODO(noel): implement.
bool GetDragData(NPObject* event, bool add_data, NPVariant results[3]);
bool IsDragEvent(NPObject* event);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_NPRUNTIME_UTIL_H_
