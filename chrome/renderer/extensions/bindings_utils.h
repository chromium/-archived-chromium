// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_
#define CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_

#include "base/singleton.h"
#include "chrome/common/resource_bundle.h"

#include <string>

template<int kResourceId>
struct StringResourceTemplate {
  StringResourceTemplate()
      : resource(ResourceBundle::GetSharedInstance().GetRawDataResource(
            kResourceId).as_string()) {
  }
  std::string resource;
};

template<int kResourceId>
const char* GetStringResource() {
  return
      Singleton< StringResourceTemplate<kResourceId> >::get()->resource.c_str();
}

#endif  // CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_
