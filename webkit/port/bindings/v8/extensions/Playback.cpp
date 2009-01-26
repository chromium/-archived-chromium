// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Playback.h"

namespace WebCore {

const char* kPlaybackExtensionName = "v8/PlaybackMode";

v8::Extension* PlaybackExtension::Get() {
    v8::Extension* extension = new v8::Extension(
        kPlaybackExtensionName,
        "(function () {"
        "  var orig_date = Date;"
        "  Math.random = function() {"
        "    return 0.5;"
        "  };"
        "  Date.__proto__.now = function() {"
        "    return new orig_date(1204251968254);"
        "  };"
        "  Date = function() {"
        "    return Date.now();"
        "  };"
        "})()");
    return extension;
}

}

