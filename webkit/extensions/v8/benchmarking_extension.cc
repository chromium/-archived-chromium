// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Cache.h"
#include "webkit/extensions/v8/benchmarking_extension.h"
#include "webkit/glue/webkit_glue.h"

namespace extensions_v8 {

const char* kBenchmarkingExtensionName = "v8/Benchmarking";

class BenchmarkingWrapper : public v8::Extension {
 public:
  BenchmarkingWrapper() :
      v8::Extension(kBenchmarkingExtensionName,
        "if (typeof(chromium) == 'undefined') {"
        "  chromium = {};"
        "};"
        "if (typeof(chromium.benchmarking) == 'undefined') {"
        "  chromium.benchmarking = {};"
        "};"
        "chromium.benchmarking.clearCache = function() {"
        "  native function ClearCache();"
        "  ClearCache();"
        "};"
        "chromium.benchmarking.closeConnections = function() {"
        "  native function CloseConnections();"
        "  CloseConnections();"
        "};") {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("CloseConnections"))) {
      return v8::FunctionTemplate::New(CloseConnections);
    } else if (name->Equals(v8::String::New("ClearCache"))) {
      return v8::FunctionTemplate::New(ClearCache);
    }
    return v8::Handle<v8::FunctionTemplate>();
  }

  static v8::Handle<v8::Value> CloseConnections(const v8::Arguments& args) {
    webkit_glue::CloseIdleConnections();
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> ClearCache(const v8::Arguments& args) {
    // TODO(mbelshe): should be enable/disable?
    webkit_glue::SetCacheMode(false);

    // Disabling and re-enabling the cache forces it to flush.
    WebCore::cache()->setDisabled(true);
    WebCore::cache()->setDisabled(false);
    return v8::Undefined();
  }
};

v8::Extension* BenchmarkingExtension::Get() {
  return new BenchmarkingWrapper();
}

}  // namespace extensions_v8
