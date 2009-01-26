// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Interval.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

const char* kIntervalExtensionName = "v8/Interval";

class IntervalExtensionWrapper : public v8::Extension {
public:
    IntervalExtensionWrapper() : 
        v8::Extension(kIntervalExtensionName,
          "native function HiResTime();"
          "function Interval() {"
          "  var start_ = 0;"
          "  var stop_ = 0;"
          "  this.start = function() {"
          "    start_ = HiResTime();"
          "  };"
          "  this.stop = function() {"
          "    stop_ = HiResTime();"
          "    if (start_ == 0)"
          "      stop_ = 0;"
          "  };"
          "  this.microseconds = function() {"
          "    if (stop_ == 0)"
          "      stop();"
          "    return Math.ceil((stop_ - start_) * 1000000);"
          "  };"
          "}") {};

    virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(v8::Handle<v8::String> name) {
        if (name->Equals(v8::String::New("HiResTime")))
            return v8::FunctionTemplate::New(HiResTime);
        return v8::Handle<v8::FunctionTemplate>();
    }

    static v8::Handle<v8::Value> HiResTime(const v8::Arguments& args) {
        return v8::Number::New(WTF::currentTime());
    }
};

v8::Extension*  IntervalExtension::Get() {
    return new IntervalExtensionWrapper();
}

}

