// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_dispatcher.h"

#include "base/process_util.h"
#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_bookmarks_module.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_page_actions_module.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/result_codes.h"

// FactoryRegistry -------------------------------------------------------------

namespace {

// Template for defining ExtensionFunctionFactory.
template<class T>
ExtensionFunction* NewExtensionFunction() {
  return new T();
}

// Contains a list of all known extension functions and allows clients to
// create instances of them.
class FactoryRegistry {
 public:
  static FactoryRegistry* instance();
  FactoryRegistry() { ResetFunctions(); }

  // Resets all functions to their default values.
  void ResetFunctions();

  // Adds all function names to 'names'.
  void GetAllNames(std::vector<std::string>* names);

  // Allows overriding of specific functions (e.g. for testing).  Functions
  // must be previously registered.  Returns true if successful.
  bool OverrideFunction(const std::string& name,
                        ExtensionFunctionFactory factory);

  // Factory method for the ExtensionFunction registered as 'name'.
  ExtensionFunction* NewFunction(const std::string& name);

 private:
  typedef std::map<std::string, ExtensionFunctionFactory> FactoryMap;
  FactoryMap factories_;
};

FactoryRegistry* FactoryRegistry::instance() {
  return Singleton<FactoryRegistry>::get();
}

void FactoryRegistry::ResetFunctions() {
  // Register all functions here.

  // Windows
  factories_["GetWindow"] = &NewExtensionFunction<GetWindowFunction>;
  factories_["GetCurrentWindow"] =
      &NewExtensionFunction<GetCurrentWindowFunction>;
  factories_["GetLastFocusedWindow"] =
      &NewExtensionFunction<GetLastFocusedWindowFunction>;
  factories_["GetAllWindows"] = &NewExtensionFunction<GetAllWindowsFunction>;
  factories_["CreateWindow"] = &NewExtensionFunction<CreateWindowFunction>;
  factories_["UpdateWindow"] = &NewExtensionFunction<UpdateWindowFunction>;
  factories_["RemoveWindow"] = &NewExtensionFunction<RemoveWindowFunction>;

  // Tabs
  factories_["GetTab"] = &NewExtensionFunction<GetTabFunction>;
  factories_["GetSelectedTab"] =
      &NewExtensionFunction<GetSelectedTabFunction>;
  factories_["GetAllTabsInWindow"] =
      &NewExtensionFunction<GetAllTabsInWindowFunction>;
  factories_["CreateTab"] = &NewExtensionFunction<CreateTabFunction>;
  factories_["UpdateTab"] = &NewExtensionFunction<UpdateTabFunction>;
  factories_["MoveTab"] = &NewExtensionFunction<MoveTabFunction>;
  factories_["RemoveTab"] = &NewExtensionFunction<RemoveTabFunction>;

  // Page Actions.
  factories_["EnablePageAction"] =
      &NewExtensionFunction<EnablePageActionFunction>;

  // Bookmarks.
  factories_["GetBookmarks"] = &NewExtensionFunction<GetBookmarksFunction>;
  factories_["GetBookmarkChildren"] =
      &NewExtensionFunction<GetBookmarkChildrenFunction>;
  factories_["GetBookmarkTree"] =
      &NewExtensionFunction<GetBookmarkTreeFunction>;
  factories_["SearchBookmarks"] =
      &NewExtensionFunction<SearchBookmarksFunction>;
  factories_["RemoveBookmark"] =
      &NewExtensionFunction<RemoveBookmarkFunction>;
  factories_["CreateBookmark"] =
      &NewExtensionFunction<CreateBookmarkFunction>;
  factories_["MoveBookmark"] = &NewExtensionFunction<MoveBookmarkFunction>;
  factories_["SetBookmarkTitle"] =
      &NewExtensionFunction<SetBookmarkTitleFunction>;
}

void FactoryRegistry::GetAllNames(std::vector<std::string>* names) {
  for (FactoryMap::iterator iter = factories_.begin();
       iter != factories_.end(); ++iter) {
    names->push_back(iter->first);
  }
}

bool FactoryRegistry::OverrideFunction(const std::string& name,
                                       ExtensionFunctionFactory factory) {
  FactoryMap::iterator iter = factories_.find(name);
  if (iter == factories_.end()) {
    return false;
  } else {
    iter->second = factory;
    return true;
  }
}

ExtensionFunction* FactoryRegistry::NewFunction(const std::string& name) {
  FactoryMap::iterator iter = factories_.find(name);
  DCHECK(iter != factories_.end());
  ExtensionFunction* function = iter->second();
  function->SetName(name);
  return function;
}

};  // namespace

// ExtensionFunctionDispatcher -------------------------------------------------

void ExtensionFunctionDispatcher::GetAllFunctionNames(
    std::vector<std::string>* names) {
  FactoryRegistry::instance()->GetAllNames(names);
}

bool ExtensionFunctionDispatcher::OverrideFunction(
    const std::string& name, ExtensionFunctionFactory factory) {
  return FactoryRegistry::instance()->OverrideFunction(name, factory);
}

void ExtensionFunctionDispatcher::ResetFunctions() {
  FactoryRegistry::instance()->ResetFunctions();
}

ExtensionFunctionDispatcher::ExtensionFunctionDispatcher(
    RenderViewHost* render_view_host,
    Delegate* delegate,
    const std::string& extension_id)
  : render_view_host_(render_view_host),
    delegate_(delegate),
    extension_id_(extension_id) {
  RenderProcessHost* process = render_view_host_->process();
  ExtensionMessageService* message_service =
      ExtensionMessageService::GetInstance(profile()->GetRequestContext());
  DCHECK(process);
  DCHECK(message_service);
  message_service->RegisterExtension(extension_id, process->pid());
}

Browser* ExtensionFunctionDispatcher::GetBrowser() {
  DCHECK(delegate_);

  Browser* retval = delegate_->GetBrowser();
  DCHECK(retval);
  return retval;
}

void ExtensionFunctionDispatcher::HandleRequest(const std::string& name,
                                                const std::string& args,
                                                int request_id,
                                                bool has_callback) {
  // TODO(aa): This will get a bit more complicated when we support functions
  // that live longer than the stack frame.
  scoped_ptr<ExtensionFunction> function(
      FactoryRegistry::instance()->NewFunction(name));
  function->set_dispatcher(this);
  function->SetArgs(args);
  function->set_request_id(request_id);
  function->set_has_callback(has_callback);
  function->Run();
}

void ExtensionFunctionDispatcher::SendResponse(ExtensionFunction* function,
                                               bool success) {
  render_view_host_->SendExtensionResponse(function->request_id(), success,
      function->GetResult(), function->GetError());
}

void ExtensionFunctionDispatcher::HandleBadMessage(ExtensionFunction* api) {
  LOG(ERROR) << "bad extension message " <<  // TODO(erikkay) name?
                " : terminating renderer.";
  if (RenderProcessHost::run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just crash.
    CHECK(false);
  } else {
    NOTREACHED();
    base::KillProcess(render_view_host_->process()->process().handle(),
                      ResultCodes::KILLED_BAD_MESSAGE, false);
  }
}

Profile* ExtensionFunctionDispatcher::profile() {
  return render_view_host_->process()->profile();
}
