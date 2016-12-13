// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/loadtimes_extension_bindings.h"

#include <math.h>

#include "base/time.h"
#include "chrome/renderer/navigation_state.h"
#include "v8/include/v8.h"
#include "webkit/glue/webframe.h"

using WebKit::WebDataSource;
using WebKit::WebNavigationType;

// Values for CSI "tran" property
const int kTransitionLink = 0;
const int kTransitionForwardBack = 6;
const int kTransitionOther = 15;
const int kTransitionReload = 16;

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
      "var chrome;"
      "if (!chrome)"
      "  chrome = {};"
      "chrome.loadTimes = function() {"
      "  native function GetLoadTimes();"
      "  return GetLoadTimes();"
      "};"
      "chrome.csi = function() {"
      "  native function GetCSI();"
      "  return GetCSI();"
      "}") {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("GetLoadTimes"))) {
      return v8::FunctionTemplate::New(GetLoadTimes);
    } else if (name->Equals(v8::String::New("GetCSI"))) {
      return v8::FunctionTemplate::New(GetCSI);
    }
    return v8::Handle<v8::FunctionTemplate>();
  }

  static const char *GetNavigationType(WebNavigationType nav_type) {
    switch (nav_type) {
      case WebKit::WebNavigationTypeLinkClicked:
        return "LinkClicked";
      case WebKit::WebNavigationTypeFormSubmitted:
        return "FormSubmitted";
      case WebKit::WebNavigationTypeBackForward:
        return "BackForward";
      case WebKit::WebNavigationTypeReload:
        return "Reload";
      case WebKit::WebNavigationTypeFormResubmitted:
        return "Resubmitted";
      case WebKit::WebNavigationTypeOther:
        return "Other";
    }
    return "";
  }

  static int GetCSITransitionType(WebNavigationType nav_type) {
    switch (nav_type) {
      case WebKit::WebNavigationTypeLinkClicked:
      case WebKit::WebNavigationTypeFormSubmitted:
      case WebKit::WebNavigationTypeFormResubmitted:
        return kTransitionLink;
      case WebKit::WebNavigationTypeBackForward:
        return kTransitionForwardBack;
      case WebKit::WebNavigationTypeReload:
        return kTransitionReload;
      case WebKit::WebNavigationTypeOther:
        return kTransitionOther;
    }
    return kTransitionOther;
  }

  static v8::Handle<v8::Value> GetLoadTimes(const v8::Arguments& args) {
    WebFrame* frame = WebFrame::RetrieveFrameForEnteredContext();
    if (frame) {
      WebDataSource* data_source = frame->GetDataSource();
      if (data_source) {
        NavigationState* navigation_state =
            NavigationState::FromDataSource(data_source);
        v8::Local<v8::Object> load_times = v8::Object::New();
        load_times->Set(
            v8::String::New("requestTime"),
            v8::Number::New(navigation_state->request_time().ToDoubleT()));
        load_times->Set(
            v8::String::New("startLoadTime"),
            v8::Number::New(navigation_state->start_load_time().ToDoubleT()));
        load_times->Set(
            v8::String::New("commitLoadTime"),
            v8::Number::New(navigation_state->commit_load_time().ToDoubleT()));
        load_times->Set(
            v8::String::New("finishDocumentLoadTime"),
            v8::Number::New(
                navigation_state->finish_document_load_time().ToDoubleT()));
        load_times->Set(
            v8::String::New("finishLoadTime"),
            v8::Number::New(navigation_state->finish_load_time().ToDoubleT()));
        load_times->Set(
            v8::String::New("firstPaintTime"),
            v8::Number::New(navigation_state->first_paint_time().ToDoubleT()));
        load_times->Set(
            v8::String::New("firstPaintAfterLoadTime"),
            v8::Number::New(
                navigation_state->first_paint_after_load_time().ToDoubleT()));
        load_times->Set(
            v8::String::New("navigationType"),
            v8::String::New(GetNavigationType(data_source->navigationType())));

        return load_times;
      }
    }
    return v8::Null();
  }

  static v8::Handle<v8::Value> GetCSI(const v8::Arguments& args) {
    WebFrame* frame = WebFrame::RetrieveFrameForEnteredContext();
    if (frame) {
      WebDataSource* data_source = frame->GetDataSource();
      if (data_source) {
        NavigationState* navigation_state =
            NavigationState::FromDataSource(data_source);
        v8::Local<v8::Object> csi = v8::Object::New();
        base::Time now = base::Time::Now();
        base::Time start = navigation_state->request_time().is_null() ?
            navigation_state->start_load_time() :
            navigation_state->request_time();
        base::Time onload = navigation_state->finish_document_load_time();
        base::TimeDelta page = now - start;
        csi->Set(
            v8::String::New("startE"),
            v8::Number::New(floor(start.ToDoubleT() * 1000)));
        csi->Set(
            v8::String::New("onloadT"),
            v8::Number::New(floor(onload.ToDoubleT() * 1000)));
        csi->Set(
          v8::String::New("pageT"),
          v8::Number::New(page.InMillisecondsF()));
        csi->Set(
            v8::String::New("tran"),
            v8::Number::New(
                GetCSITransitionType(data_source->navigationType())));

        return csi;
      }
    }
    return v8::Null();
  }
};

v8::Extension* LoadTimesExtension::Get() {
  return new LoadTimesExtensionWrapper();
}

}  // namespace extensions_v8
