// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/js_only_v8_extensions.h"

#include "chrome/renderer/extensions/bindings_utils.h"
#include "grit/webkit_resources.h"

// BaseJsV8Extension
const char* BaseJsV8Extension::kName = "chrome/base";
v8::Extension* BaseJsV8Extension::Get() {
  return new v8::Extension(kName, GetStringResource<IDR_DEVTOOLS_BASE_JS>(),
                           0, NULL);
}

// JsonJsV8Extension
const char* JsonJsV8Extension::kName = "chrome/json";
v8::Extension* JsonJsV8Extension::Get() {
  static const char* deps[] = {
    BaseJsV8Extension::kName
  };
  return new v8::Extension(kName, GetStringResource<IDR_DEVTOOLS_JSON_JS>(),
                           arraysize(deps), deps);
}
