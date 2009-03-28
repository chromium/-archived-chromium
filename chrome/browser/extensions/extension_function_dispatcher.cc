// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_dispatcher.h"

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/renderer_host/render_view_host.h"

// FactoryRegistry -------------------------------------------------------------

namespace {

// A pointer to a function that create an instance of an ExtensionFunction.
typedef ExtensionFunction* (*ExtensionFunctionFactory)();

// Contains a list of all known extension functions and allows clients to create
// instances of them.
class FactoryRegistry {
 public:
  static FactoryRegistry* instance();
  FactoryRegistry();
  void GetAllNames(std::vector<std::string>* names);
  ExtensionFunction* NewFunction(const std::string& name);

 private:
  typedef std::map<std::string, ExtensionFunctionFactory> FactoryMap;
  FactoryMap factories_;
};

// Template for defining ExtensionFunctionFactory.
template<class T>
ExtensionFunction* NewExtensionFunction() {
  return new T();
}

FactoryRegistry* FactoryRegistry::instance() {
  return Singleton<FactoryRegistry>::get();
}

FactoryRegistry::FactoryRegistry() {
  // Register all functions here.
  factories_["GetTabsForWindow"] =
      &NewExtensionFunction<GetTabsForWindowFunction>;
  factories_["CreateTab"] = &NewExtensionFunction<CreateTabFunction>;
}

void FactoryRegistry::GetAllNames(
    std::vector<std::string>* names) {
  for (FactoryMap::iterator iter = factories_.begin(); iter != factories_.end();
       ++iter) {
    names->push_back(iter->first);
  }
}

ExtensionFunction* FactoryRegistry::NewFunction(const std::string& name) {
  FactoryMap::iterator iter = factories_.find(name);
  DCHECK(iter != factories_.end());
  return iter->second();
}

};


// ExtensionFunctionDispatcher -------------------------------------------------

void ExtensionFunctionDispatcher::GetAllFunctionNames(
    std::vector<std::string>* names) {
  FactoryRegistry::instance()->GetAllNames(names);
}

ExtensionFunctionDispatcher::ExtensionFunctionDispatcher(
    RenderViewHost* render_view_host)
  : render_view_host_(render_view_host) {}

void ExtensionFunctionDispatcher::HandleRequest(const std::string& name,
                                                const std::string& args,
                                                int callback_id) {
  scoped_ptr<Value> value;
  if (!args.empty()) {
    JSONReader reader;
    value.reset(reader.JsonToValue(args, false, false));

    // Since we do the serialization in the v8 extension, we should always get
    // valid JSON.
    if (!value.get()) {
      DCHECK(false);
      return;
    }
  }

  // TODO(aa): This will get a bit more complicated when we support functions
  // that live longer than the stack frame.
  scoped_ptr<ExtensionFunction> function(
      FactoryRegistry::instance()->NewFunction(name));
  function->set_dispatcher(this);
  function->set_args(value.get());
  function->set_callback_id(callback_id);
  function->Run();
}

void ExtensionFunctionDispatcher::SendResponse(ExtensionFunction* function) {
  std::string json;

  // Some functions might not need to return any results.
  if (function->result())
    JSONWriter::Write(function->result(), false, &json);

  render_view_host_->SendExtensionResponse(function->callback_id(), json);
}
