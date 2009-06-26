// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef np_v8object_h
#define np_v8object_h

#include "bindings/npruntime.h"
#include <v8.h>

namespace WebCore {
    class DOMWindow;
}

extern NPClass* npScriptObjectClass;

// A V8NPObject is a NPObject which carries additional V8-specific
// information.  It is allocated and deallocated by AllocV8NPObject()
// and FreeV8NPObject() methods.
struct V8NPObject {
    NPObject object;
    v8::Persistent<v8::Object> v8Object;
    WebCore::DOMWindow* rootObject;
};

struct PrivateIdentifier {
    union {
        const NPUTF8* string;
        int32_t number;
    } value;
    bool isString;
};

NPObject* npCreateV8ScriptObject(NPP npp, v8::Handle<v8::Object>, WebCore::DOMWindow*);

#endif // np_v8object_h
