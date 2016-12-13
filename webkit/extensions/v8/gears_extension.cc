// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/extensions/v8/gears_extension.h"

namespace extensions_v8 {

const char* kGearsExtensionName = "v8/Gears";

// Note: when a page touches the "google.gears.factory" object, this script
// touches the DOM.  We expect the DOM to be available at that time.
const char* kGearsExtensionScript =
    "var google;"
    "if (!google)"
    "  google = {};"
    "if (!google.gears)"
    "  google.gears = {};"
    "(function() {"
    "  var factory = null;"
    "  google.gears.__defineGetter__('factory', function() {"
    "    if (!factory) {"
    "      factory = document.createElement('object');"
    "      factory.width = 0;"
    "      factory.height = 0;"
    "      factory.style.visibility = 'hidden';"
    "      factory.type = 'application/x-googlegears';"
    "      document.documentElement.appendChild(factory);"
    "    }"
    "    return factory;"
    "  });"
    "})();";

class GearsExtensionWrapper : public v8::Extension {
 public:
  GearsExtensionWrapper()
      : v8::Extension(kGearsExtensionName, kGearsExtensionScript) {}
};

v8::Extension* GearsExtension::Get() {
    return new GearsExtensionWrapper();
}

}  // namespace extensions_v8
