// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/loadtimes_extension_bindings.h"

#include "base/time.h"
#include "v8/include/v8.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webdatasource.h"

namespace extensions_v8 {

static const char* kLoadTimesExtensionName = "v8/LoadTimes";

class LoadTimesExtensionWrapper : public v8::Extension {
 public:
  // Creates an extension which adds a new function, chromium.GetLoadTimes()
  // This function returns an object containing the following members:
  // requestTime: The time the request to load the page was received
  // loadTime: The time the renderer started the load process
  // finishDocumentLoadTime: The time the document itself was loaded
  //                         (this is before the onload() method is fired)
  // finishLoadTime: The time all loading is done, after the onload()
  //                 method and all resources
  // navigationType: A string describing what user action initiated the load
  LoadTimesExtensionWrapper() :
    v8::Extension(kLoadTimesExtensionName,
      "var chromium;"
      "if (!chromium)"
      "  chromium = {};"
      "chromium.GetLoadTimes = function() {"
      "  native function GetLoadTimes();"
      "  return GetLoadTimes();"
      "}") {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("GetLoadTimes"))) {
      return v8::FunctionTemplate::New(GetLoadTimes);
    }
    return v8::Handle<v8::FunctionTemplate>();
  }

  static const char *GetNavigationType(WebNavigationType nav_type) {
    switch (nav_type) {
      case WebNavigationTypeLinkClicked: return "LinkClicked";
      case WebNavigationTypeFormSubmitted: return "FormSubmitted";
      case WebNavigationTypeBackForward: return "BackForward";
      case WebNavigationTypeReload: return "Reload";
      case WebNavigationTypeFormResubmitted: return "Resubmitted";
      case WebNavigationTypeOther: return "Other";
    }
    return "";
  }

  static v8::Handle<v8::Value> GetLoadTimes(const v8::Arguments& args) {
    WebFrame* win_frame = WebFrame::RetrieveActiveFrame();
    if (win_frame) {
      WebDataSource* data_source = win_frame->GetDataSource();
      if (data_source) {
        v8::Local<v8::Object> load_times = v8::Object::New();
        load_times->Set(
            v8::String::New("requestTime"),
            v8::Number::New(data_source->GetRequestTime().ToDoubleT()));
        load_times->Set(
            v8::String::New("startLoadTime"),
            v8::Number::New(data_source->GetStartLoadTime().ToDoubleT()));
        load_times->Set(
            v8::String::New("finishDocumentLoadTime"),
            v8::Number::New(
                data_source->GetFinishDocumentLoadTime().ToDoubleT()));
        load_times->Set(
            v8::String::New("finishLoadTime"),
            v8::Number::New(data_source->GetFinishLoadTime().ToDoubleT()));
        load_times->Set(
            v8::String::New("navigationType"),
            v8::String::New(
                GetNavigationType(data_source->GetNavigationType())));
        return load_times;
      }
    }
    return v8::Null();
  }
};

v8::Extension* LoadTimesExtension::Get() {
  return new LoadTimesExtensionWrapper();
}

}  // namespace extensions_v8
