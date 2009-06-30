// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_
#define CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_

#include "app/resource_bundle.h"
#include "base/linked_ptr.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "v8/include/v8.h"

#include <list>
#include <string>

class RenderView;
class WebFrame;

namespace bindings_utils {

// This is a base class for chrome extension bindings.  Common features that
// are shared by different modules go here.
class ExtensionBase : public v8::Extension {
 public:
  ExtensionBase(const char* name,
                const char* source,
                int dep_count,
                const char** deps)
      : v8::Extension(name, source, dep_count, deps) {}

  // Derived classes should call this at the end of their implementation in
  // order to expose common native functions, like GetChromeHidden, to the
  // v8 extension.
  virtual v8::Handle<v8::FunctionTemplate>
      GetNativeFunction(v8::Handle<v8::String> name);

 protected:
  // Returns a hidden variable for use by the bindings that is unreachable
  // by the page.
  static v8::Handle<v8::Value> GetChromeHidden(const v8::Arguments& args);

  // Starts an API request to the browser, with an optional callback.  The
  // callback will be dispatched to EventBindings::HandleResponse.
  static v8::Handle<v8::Value> StartRequest(const v8::Arguments& args);
};

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

// Contains information about a single javascript context.
struct ContextInfo {
  v8::Persistent<v8::Context> context;
  std::string extension_id;  // empty if the context is not an extension

  ContextInfo(v8::Persistent<v8::Context> context,
              const std::string& extension_id)
      : context(context), extension_id(extension_id) {}
};
typedef std::list< linked_ptr<ContextInfo> > ContextList;

// Returns a mutable reference to the ContextList.
ContextList& GetContexts();

// Returns a (copied) list of contexts that have the given extension_id.
ContextList GetContextsForExtension(const std::string& extension_id);

// Returns the ContextInfo item that has the given context.
ContextList::iterator FindContext(v8::Handle<v8::Context> context);

// Contains info relevant to a pending API request.
struct PendingRequest {
 public :
  PendingRequest(v8::Persistent<v8::Context> context, const std::string& name)
      : context(context), name(name) {
  }
  v8::Persistent<v8::Context> context;
  std::string name;
};
typedef std::map<int, linked_ptr<PendingRequest> > PendingRequestMap;

// Returns a mutable reference to the PendingRequestMap.
PendingRequestMap& GetPendingRequestMap();

// Returns the current RenderView, based on which V8 context is current.  It is
// an error to call this when not in a V8 context.
RenderView* GetRenderViewForCurrentContext();

// Call the named javascript function with the given arguments in a context.
// The function name should be reachable from the chromeHidden object, and can
// be a sub-property like "Port.dispatchOnMessage".
void CallFunctionInContext(v8::Handle<v8::Context> context,
                           const std::string& function_name, int argc,
                           v8::Handle<v8::Value>* argv);

}  // namespace bindings_utils

#endif  // CHROME_RENDERER_EXTENSIONS_BINDINGS_UTILS_H_
