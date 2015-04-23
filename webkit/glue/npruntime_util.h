// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_NPRUNTIME_UTIL_H_
#define WEBKIT_GLUE_NPRUNTIME_UTIL_H_

#include "third_party/npapi/bindings/npruntime.h"

class Pickle;

namespace WebKit {
class WebDragData;
}

namespace webkit_glue {

// Efficiently serialize/deserialize a NPIdentifier
bool SerializeNPIdentifier(NPIdentifier identifier, Pickle* pickle);
bool DeserializeNPIdentifier(const Pickle& pickle, void** pickle_iter,
                             NPIdentifier* identifier);

// Return true (success) if the given npobj is the current
// drag event in browser dispatch, and is accessible based on context execution
// frames and their security origins and WebKit clipboard access policy. If so,
// return the event id and the clipboard data (WebDragData).
bool GetDragData(NPObject* npobj, int* event_id, WebKit::WebDragData* data);

// Invoke the event access policy checks listed above with
// GetDragData().  No need for clipboard data or event_id outputs, just confirm
// the given npobj is the current & accessible drag event.
bool IsDragEvent(NPObject* npobj);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_NPRUNTIME_UTIL_H_
