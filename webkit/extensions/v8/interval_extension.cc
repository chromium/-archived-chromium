// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/extensions/v8/interval_extension.h"
#include "base/time.h"

namespace extensions_v8 {

const char* kIntervalExtensionName = "v8/Interval";

class IntervalExtensionWrapper : public v8::Extension {
 public:
  IntervalExtensionWrapper()
      : v8::Extension(
            kIntervalExtensionName,
            "var chromium;"
            "if (!chromium)"
            "  chromium = {};"
            "chromium.Interval = function() {"
            "  var start_ = 0;"
            "  var stop_ = 0;"
            "  native function HiResTime();"
            "  this.start = function() {"
            "    stop_ = 0;"
            "    start_ = HiResTime();"
            "  };"
            "  this.stop = function() {"
            "    stop_ = HiResTime();"
            "    if (start_ == 0)"
            "      stop_ = 0;"
            "  };"
            "  this.microseconds = function() {"
            "    var stop = stop_;"
            "    if (stop == 0 && start_ != 0)"
            "      stop = HiResTime();"
            "    return Math.ceil((stop - start_) * 1000000);"
            "  };"
            "}") {}

    virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
        v8::Handle<v8::String> name) {
      if (name->Equals(v8::String::New("HiResTime"))) {
        return v8::FunctionTemplate::New(HiResTime);
      }
      return v8::Handle<v8::FunctionTemplate>();
    }

    static v8::Handle<v8::Value> HiResTime(const v8::Arguments& args) {
      return v8::Number::New(base::Time::Now().ToDoubleT());
    }
};

v8::Extension*  IntervalExtension::Get() {
  return new IntervalExtensionWrapper();
}

}  // namespace extensions_v8

