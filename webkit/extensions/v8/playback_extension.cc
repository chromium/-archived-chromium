// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/extensions/v8/playback_extension.h"

namespace extensions_v8 {

const char* kPlaybackExtensionName = "v8/PlaybackMode";

v8::Extension* PlaybackExtension::Get() {
  v8::Extension* extension = new v8::Extension(
      kPlaybackExtensionName,
      "(function () {"
      "  var orig_date = Date;"
      "  var x = 0;"
      "  var time_seed = 1204251968254;"
      "  Math.random = function() {"
      "    x += .1;"
      "    return (x % 1);"
      "  };"
      "  Date.__proto__.now = function() {"
      "    time_seed += 50;"
      "    return new orig_date(time_seed);"
      "  };"
      "  Date = function() {"
      "    return Date.now();"
      "  };"
      "})()");
    return extension;
}

}  // namespace extensions_v8
