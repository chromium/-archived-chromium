// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_
#define CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_

#include "app/resource_bundle.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "v8/include/v8.h"

#include <string>

class RenderView;

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

// Returns the current RenderView, based on which V8 context is current.  It is
// an error to call this when not in a V8 context.
RenderView* GetRenderViewForCurrentContext();

// Call the named javascript function with the given arguments in a context.
// The function name should be reachable from the global object, and can be a
// sub-property like "chrome.handleResponse_".
void CallFunctionInContext(v8::Handle<v8::Context> context,
                           const std::string& function_name, int argc,
                           v8::Handle<v8::Value>* argv);

#endif  // CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_
