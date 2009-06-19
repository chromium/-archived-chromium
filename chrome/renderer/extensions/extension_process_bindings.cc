// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_process_bindings.h"

#include "base/singleton.h"
#include "base/stl_util-inl.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/js_only_v8_extensions.h"
#include "chrome/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "webkit/api/public/WebScriptSource.h"
#include "webkit/glue/webframe.h"

using WebKit::WebScriptSource;
using WebKit::WebString;

namespace {

const char kExtensionName[] = "chrome/ExtensionProcessBindings";
const char* kExtensionDeps[] = {
  BaseJsV8Extension::kName,
  EventBindings::kName,
  JsonSchemaJsV8Extension::kName,
  RendererExtensionBindings::kName,
};

// Types for storage of per-renderer-singleton data structure that maps
// |extension_id| -> <List of v8 Contexts for the "views" of that extension>
typedef std::list< v8::Persistent<v8::Context> > ContextList;
typedef std::map<std::string, ContextList> ExtensionIdContextsMap;

// Contains info relevant to a pending request.
struct CallContext {
 public :
  CallContext(v8::Persistent<v8::Context> context, const std::string& name)
      : context(context), name(name) {
  }
  v8::Persistent<v8::Context> context;
  std::string name;
};
typedef std::map<int, CallContext*> PendingRequestMap;

struct SingletonData {
  std::set<std::string> function_names_;
  ExtensionIdContextsMap contexts;
  PendingRequestMap pending_requests;

  ~SingletonData() {
    STLDeleteContainerPairSecondPointers(pending_requests.begin(),
                                         pending_requests.end());
  }
};

static std::set<std::string>* GetFunctionNameSet() {
  return &Singleton<SingletonData>()->function_names_;
}

static ContextList& GetRegisteredContexts(std::string extension_id) {
  return Singleton<SingletonData>::get()->contexts[extension_id];
}

static PendingRequestMap& GetPendingRequestMap() {
  return Singleton<SingletonData>::get()->pending_requests;
}

class ExtensionImpl : public v8::Extension {
 public:
  ExtensionImpl() : v8::Extension(
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

    if (name->Equals(v8::String::New("GetNextRequestId")))
      return v8::FunctionTemplate::New(GetNextRequestId);
    else if (name->Equals(v8::String::New("RegisterExtension")))
      return v8::FunctionTemplate::New(RegisterExtension);
    else if (name->Equals(v8::String::New("UnregisterExtension")))
      return v8::FunctionTemplate::New(UnregisterExtension);
    else if (name->Equals(v8::String::New("GetViews")))
      return v8::FunctionTemplate::New(GetViews);
    else if (names->find(*v8::String::AsciiValue(name)) != names->end())
      return v8::FunctionTemplate::New(StartRequest, name);

    return v8::Handle<v8::FunctionTemplate>();
  }

 private:
  static v8::Handle<v8::Value> RegisterExtension(const v8::Arguments& args) {
    RenderView* renderview = GetRenderViewForCurrentContext();
    DCHECK(renderview);
    GURL url = renderview->webview()->GetMainFrame()->GetURL();
    DCHECK(url.scheme() == chrome::kExtensionScheme);

    v8::Persistent<v8::Context> current_context =
        v8::Persistent<v8::Context>::New(v8::Context::GetCurrent());
    DCHECK(!current_context.IsEmpty());

    std::string extension_id = url.host();
    GetRegisteredContexts(extension_id).push_back(current_context);
    return v8::String::New(extension_id.c_str());
  }

  static v8::Handle<v8::Value> UnregisterExtension(const v8::Arguments& args) {
    DCHECK_EQ(args.Length(), 1);
    DCHECK(args[0]->IsString());

    v8::Local<v8::Context> current_context = v8::Context::GetCurrent();
    DCHECK(!current_context.IsEmpty());

    // Remove all pending requests for this context.
    PendingRequestMap& pending_requests = GetPendingRequestMap();
    for (PendingRequestMap::iterator it = pending_requests.begin();
         it != pending_requests.end(); ) {
      PendingRequestMap::iterator current = it++;
      if (current->second->context == current_context) {
        current->second->context.Dispose();
        current->second->context.Clear();
        delete current->second;
        pending_requests.erase(current);
      }
    }

    std::string extension_id(*v8::String::Utf8Value(args[0]));
    ContextList& contexts = GetRegisteredContexts(extension_id);
    ContextList::iterator it = std::find(contexts.begin(), contexts.end(),
                                         current_context);
    if (it == contexts.end()) {
      NOTREACHED();
      return v8::Undefined();
    }

    it->Dispose();
    it->Clear();
    contexts.erase(it);

    return v8::Undefined();
  }

  static v8::Handle<v8::Value> GetViews(const v8::Arguments& args) {
    RenderView* renderview = GetRenderViewForCurrentContext();
    DCHECK(renderview);
    GURL url = renderview->webview()->GetMainFrame()->GetURL();
    std::string extension_id = url.host();

    ContextList& contexts = GetRegisteredContexts(extension_id);
    DCHECK(contexts.size() > 0);

    v8::Local<v8::Array> views = v8::Array::New(contexts.size());
    int index = 0;
    ContextList::const_iterator it = contexts.begin();
    for (; it != contexts.end(); ++it) {
      v8::Local<v8::Value> window = (*it)->Global()->Get(
          v8::String::New("window"));
      DCHECK(!window.IsEmpty());
      views->Set(v8::Integer::New(index), window);
      index++;
    }
    return views;
  }

  static v8::Handle<v8::Value> GetNextRequestId(const v8::Arguments& args) {
    static int next_request_id = 0;
    return v8::Integer::New(next_request_id++);
  }

  static v8::Handle<v8::Value> StartRequest(const v8::Arguments& args) {
    // Get the current RenderView so that we can send a routed IPC message from
    // the correct source.
    RenderView* renderview = GetRenderViewForCurrentContext();
    if (!renderview)
      return v8::Undefined();

    if (args.Length() != 3 || !args[0]->IsString() || !args[1]->IsInt32() ||
        !args[2]->IsBoolean())
      return v8::Undefined();

    std::string name = *v8::String::AsciiValue(args.Data());
    std::string json_args = *v8::String::Utf8Value(args[0]);
    int request_id = args[1]->Int32Value();
    bool has_callback = args[2]->BooleanValue();

    v8::Persistent<v8::Context> current_context =
        v8::Persistent<v8::Context>::New(v8::Context::GetCurrent());
    DCHECK(!current_context.IsEmpty());
    GetPendingRequestMap()[request_id] =
        new CallContext(current_context, *v8::String::AsciiValue(args.Data()));

    renderview->SendExtensionRequest(name, json_args, request_id, has_callback);

    return v8::Undefined();
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

void ExtensionProcessBindings::RegisterExtensionContext(WebFrame* frame) {
  frame->ExecuteScript(WebScriptSource(WebString::fromUTF8(
      "chrome.self.register_();")));
}

void ExtensionProcessBindings::HandleResponse(int request_id, bool success,
                                              const std::string& response,
                                              const std::string& error) {
  CallContext* call = GetPendingRequestMap()[request_id];
  if (!call)
    return;  // The frame went away.

  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[5];
  argv[0] = v8::Integer::New(request_id);
  argv[1] = v8::String::New(call->name.c_str());
  argv[2] = v8::Boolean::New(success);
  argv[3] = v8::String::New(response.c_str());
  argv[4] = v8::String::New(error.c_str());
  CallFunctionInContext(call->context, "chrome.handleResponse_",
                        arraysize(argv), argv);

  GetPendingRequestMap().erase(request_id);
  delete call;
}
