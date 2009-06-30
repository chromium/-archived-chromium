// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_process_bindings.h"

#include "base/singleton.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/js_only_v8_extensions.h"
#include "chrome/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "webkit/glue/webframe.h"

using bindings_utils::GetStringResource;
using bindings_utils::ContextInfo;
using bindings_utils::ContextList;
using bindings_utils::GetContexts;
using bindings_utils::ExtensionBase;

namespace {

const char kExtensionName[] = "chrome/ExtensionProcessBindings";
const char* kExtensionDeps[] = {
  BaseJsV8Extension::kName,
  EventBindings::kName,
  JsonSchemaJsV8Extension::kName,
  RendererExtensionBindings::kName,
};

struct SingletonData {
  std::set<std::string> function_names_;
};

static std::set<std::string>* GetFunctionNameSet() {
  return &Singleton<SingletonData>()->function_names_;
}

class ExtensionImpl : public ExtensionBase {
 public:
  ExtensionImpl() : ExtensionBase(
      kExtensionName, GetStringResource<IDR_EXTENSION_PROCESS_BINDINGS_JS>(),
      arraysize(kExtensionDeps), kExtensionDeps) {}

  static void SetFunctionNames(const std::vector<std::string>& names) {
    std::set<std::string>* name_set = GetFunctionNameSet();
    for (size_t i = 0; i < names.size(); ++i) {
      name_set->insert(names[i]);
    }
  }

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    std::set<std::string>* names = GetFunctionNameSet();

    if (name->Equals(v8::String::New("GetViews"))) {
      return v8::FunctionTemplate::New(GetViews);
    } else if (names->find(*v8::String::AsciiValue(name)) != names->end()) {
      return v8::FunctionTemplate::New(ExtensionBase::StartRequest, name);
    }

    return ExtensionBase::GetNativeFunction(name);
  }

 private:
  static v8::Handle<v8::Value> GetViews(const v8::Arguments& args) {
    RenderView* renderview = bindings_utils::GetRenderViewForCurrentContext();
    DCHECK(renderview);
    GURL url = renderview->webview()->GetMainFrame()->GetURL();
    std::string extension_id = url.host();

    ContextList contexts =
        bindings_utils::GetContextsForExtension(extension_id);
    DCHECK(contexts.size() > 0);

    v8::Local<v8::Array> views = v8::Array::New(contexts.size());
    int index = 0;
    ContextList::const_iterator it = contexts.begin();
    for (; it != contexts.end(); ++it) {
      v8::Local<v8::Value> window = (*it)->context->Global()->Get(
          v8::String::New("window"));
      DCHECK(!window.IsEmpty());
      views->Set(v8::Integer::New(index), window);
      index++;
    }
    return views;
  }
};

}  // namespace

v8::Extension* ExtensionProcessBindings::Get() {
  return new ExtensionImpl();
}

void ExtensionProcessBindings::SetFunctionNames(
    const std::vector<std::string>& names) {
  ExtensionImpl::SetFunctionNames(names);
}
