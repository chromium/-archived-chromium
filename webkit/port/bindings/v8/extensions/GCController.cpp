// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "GCController.h"

namespace WebCore {

const char* kGCExtensionName = "v8/GCController";

v8::Extension* GCExtension::Get() {
    v8::Extension* extension = new v8::Extension(
        kGCExtensionName,
        "(function () {"
        "   var v8_gc;"
        "   if (gc) v8_gc = gc;"
        "   GCController = new Object();"
        "   GCController.collect ="
        "     function() {if (v8_gc) v8_gc(); };"
        " })()");
    return extension;
}

}

