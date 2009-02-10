// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/debugger_node.h"

#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/debugger/debugger_shell.h"
#include "chrome/common/notification_service.h"

DebuggerNode::DebuggerNode() : data_(NULL), valid_(true), observing_(false) {
}

void DebuggerNode::Observe(NotificationType type,
                        const NotificationSource& source,
                        const NotificationDetails& details) {
  StopObserving();
  Invalidate();
}

DebuggerNode::~DebuggerNode() {
}


void DebuggerNode::StopObserving() {
  if (observing_ && valid_) {
    NotificationService* service = NotificationService::current();
    DCHECK(service);
    StopObserving(service);
    observing_ = false;
  }
  data_ = NULL;
}

void DebuggerNode::StopObserving(NotificationService *service) {
}

v8::Handle<v8::Value> DebuggerNode::IndexGetter(uint32_t index,
                                             const v8::AccessorInfo& info) {
  return v8::Undefined();
}

v8::Handle<v8::Value> DebuggerNode::PropGetter(v8::Handle<v8::String> prop,
                                            const v8::AccessorInfo& info) {
  return v8::Undefined();
}

v8::Handle<v8::Value> DebuggerNode::Function(const v8::Arguments& args) {
  return v8::Undefined();
}

v8::Handle<v8::Value> DebuggerNode::NewInstance() {
  DebuggerNodeWrapper *wrap = new DebuggerNodeWrapper(this);
  wrap->AddRef();
  v8::Local<v8::External> node = v8::External::New(wrap);
  // TODO(erikkay): cache these templates?
  v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New();
  if (IsFunction()) {
    templ->SetCallHandler(&DebuggerNode::NodeFunc, node);
    v8::Local<v8::Function> f = templ->GetFunction();
    return f;
  }
  v8::Local<v8::ObjectTemplate> instance = templ->InstanceTemplate();
  if (IsObject()) {
    instance->SetNamedPropertyHandler(&DebuggerNode::NodeGetter, 0, 0, 0, 0, node);
    // TODO(erikkay): verify that the interceptor does not have to be
    // behind the object
  }
  if (IsCollection()) {
    instance->SetIndexedPropertyHandler(&DebuggerNode::NodeIndex, 0, 0, 0, 0, node);
  }
  v8::Local<v8::Object> ret = instance->NewInstance();
  v8::Persistent<v8::Object> p = v8::Persistent<v8::Object>::New(ret);
  p.MakeWeak(wrap, &DebuggerShell::HandleWeakReference);
  return ret;
}

v8::Handle<v8::Value> DebuggerNode::NodeGetter(v8::Local<v8::String> prop,
                                            const v8::AccessorInfo& info) {
  DebuggerNodeWrapper* w =
    static_cast<DebuggerNodeWrapper*>(v8::External::Cast(*info.Data())->Value());
  DebuggerNode* n = w->node();
  if (n->IsValid() && n->IsObject()) {
    return n->PropGetter(prop, info);
  } else {
    return v8::Undefined();
  }
}

v8::Handle<v8::Value> DebuggerNode::NodeIndex(uint32_t index,
                                           const v8::AccessorInfo& info) {
  DebuggerNodeWrapper* w =
    static_cast<DebuggerNodeWrapper*>(v8::External::Cast(*info.Data())->Value());
  DebuggerNode* n = w->node();
  if (n->IsValid() && n->IsCollection()) {
    return n->IndexGetter(index, info);
  } else {
    return v8::Undefined();
  }
}

v8::Handle<v8::Value> DebuggerNode::NodeFunc(const v8::Arguments& args) {
  DebuggerNodeWrapper* w =
    static_cast<DebuggerNodeWrapper*>(v8::External::Cast(*args.Data())->Value());
  DebuggerNode* n = w->node();
  if (n->IsValid() && n->IsFunction()) {
    return n->Function(args);
  } else {
    return v8::Undefined();
  }
}



/////////////////////////////////////////////

ChromeNode::ChromeNode(DebuggerShell* debugger) {
  debugger_ = debugger;
}

ChromeNode::~ChromeNode() {
}

v8::Handle<v8::Value> ChromeNode::PropGetter(v8::Handle<v8::String> prop,
                                             const v8::AccessorInfo& info) {
  if (prop->Equals(v8::String::New("pid"))) {
    return v8::Number::New(base::GetCurrentProcId());
  } else if (prop->Equals(v8::String::New("browser"))) {
    BrowserListNode *node = BrowserListNode::BrowserList();
    return node->NewInstance();
  } else if (prop->Equals(v8::String::New("setDebuggerReady"))) {
    FunctionNode<DebuggerShell>* f =
      new FunctionNode<DebuggerShell>(DebuggerShell::SetDebuggerReady, debugger_);
    return f->NewInstance();
  } else if (prop->Equals(v8::String::New("setDebuggerBreak"))) {
    FunctionNode<DebuggerShell>* f =
      new FunctionNode<DebuggerShell>(DebuggerShell::SetDebuggerBreak, debugger_);
    return f->NewInstance();
  } else if (prop->Equals(v8::String::New("foo"))) {
    return v8::Undefined();
  } else {
    return prop;
  }
}

void ChromeNode::StopObserving(NotificationService *service) {
}

/////////////////////////////////////////////

BrowserNode::BrowserNode(Browser *b) {
  data_ = b;

  NotificationService* service = NotificationService::current();
  DCHECK(service);
  service->AddObserver(
      this, NotificationType::BROWSER_CLOSED, Source<Browser>(b));
  observing_ = true;
}

void BrowserNode::StopObserving(NotificationService *service) {
  Browser *b = static_cast<Browser*>(data_);
  service->RemoveObserver(
      this, NotificationType::BROWSER_CLOSED, Source<Browser>(b));
}

BrowserNode* BrowserNode::BrowserAtIndex(int index) {
  if (index >= 0) {
    BrowserList::const_iterator iter = BrowserList::begin();

    for (; (iter != BrowserList::end()) && (index > 0); ++iter, --index);

    if (iter != BrowserList::end()) {
      return new BrowserNode(*iter);
    }
  }
  return NULL;
}

BrowserNode::~BrowserNode() {
}

Browser* BrowserNode::GetBrowser() {
  if (IsValid()) {
    return static_cast<Browser *>(data_);
  } else {
    return NULL;
  }
}

v8::Handle<v8::Value> BrowserNode::PropGetter(v8::Handle<v8::String> prop,
                                              const v8::AccessorInfo& info) {
  Browser *b = GetBrowser();
  if (b != NULL) {
    if (prop->Equals(v8::String::New("title"))) {
      const TabContents *t = b->GetSelectedTabContents();
      std::wstring title = t->GetTitle();
      std::string title2 = WideToUTF8(title);
      return v8::String::New(title2.c_str());
    } else if (prop->Equals(v8::String::New("tab"))) {
      TabListNode* node = TabListNode::TabList(b);
      return node->NewInstance();
    }
  }
  return v8::Undefined();
}

/////////////////////////////////////////////

BrowserListNode* BrowserListNode::BrowserList() {
  // TODO(erikkay): cache
  return new BrowserListNode();
}

BrowserListNode::BrowserListNode() {
}

BrowserListNode::~BrowserListNode() {
}

v8::Handle<v8::Value> BrowserListNode::IndexGetter(
    uint32_t index,
    const v8::AccessorInfo& info) {
  BrowserNode* b = BrowserNode::BrowserAtIndex(index);
  if (!b) {
    return v8::Undefined();
  }
  return b->NewInstance();
}

void BrowserListNode::StopObserving(NotificationService *service) {
}

/////////////////////////////////////////////

TabListNode::TabListNode(Browser* b) {
  data_ = b;

  NotificationService* service = NotificationService::current();
  DCHECK(service);
  service->AddObserver(
      this, NotificationType::BROWSER_CLOSED, Source<Browser>(b));
  observing_ = true;
}

TabListNode::~TabListNode() {
}

TabListNode* TabListNode::TabList(Browser* b) {
  return new TabListNode(b);
}

Browser* TabListNode::GetBrowser() {
  if (IsValid()) {
    return static_cast<Browser *>(data_);
  } else {
    return NULL;
  }
}

void TabListNode::StopObserving(NotificationService *service) {
  Browser *b = static_cast<Browser*>(data_);
  service->RemoveObserver(
      this, NotificationType::BROWSER_CLOSED, Source<Browser>(b));
}

v8::Handle<v8::Value> TabListNode::IndexGetter(uint32_t index,
                                               const v8::AccessorInfo& info) {
  Browser* b = GetBrowser();
  if (b != NULL) {
    TabContents* tab_contents = b->GetTabContentsAt(index);
    if (tab_contents) {
      TabNode* node = new TabNode(tab_contents);
      return node->NewInstance();
    }
  }
  return v8::Undefined();
}

/////////////////////////////////////////////

TabNode::TabNode(TabContents *c) {
  data_ = c->controller();

  NotificationService* service = NotificationService::current();
  DCHECK(service);
  service->AddObserver(this, NotificationType::TAB_CLOSING,
                       Source<NavigationController>(c->controller()));
  observing_ = true;
}

TabNode::~TabNode() {
}

void TabNode::StopObserving(NotificationService *service) {
  NavigationController *c = static_cast<NavigationController*>(data_);
  service->RemoveObserver(this, NotificationType::TAB_CLOSING,
                          Source<NavigationController>(c));
}

TabContents* TabNode::GetTab() {
  if (IsValid()) {
    return static_cast<NavigationController*>(data_)->active_contents();
  } else {
    return NULL;
  }
}

v8::Handle<v8::Value> TabNode::SendToDebugger(const v8::Arguments& args,
                                              WebContents* web) {
  RenderViewHost* host = web->render_view_host();
  if (args.Length() == 1) {
    std::wstring cmd;
    v8::Handle<v8::Value> obj;
    obj = args[0];
    DebuggerShell::ObjectToString(obj, &cmd);
    host->DebugCommand(cmd);
  }
  return v8::Undefined();
}

v8::Handle<v8::Value> TabNode::Attach(const v8::Arguments& args,
                                      WebContents* web) {
  RenderViewHost* host = web->render_view_host();
  host->DebugAttach();
  RenderProcessHost* proc = host->process();
  return v8::Int32::New(proc->process().pid());
}

v8::Handle<v8::Value> TabNode::Detach(const v8::Arguments& args,
                                      WebContents* web) {
  RenderViewHost* host = web->render_view_host();
  host->DebugDetach();
  RenderProcessHost* proc = host->process();
  return v8::Int32::New(proc->process().pid());
}

v8::Handle<v8::Value> TabNode::Break(const v8::Arguments& args,
                                     WebContents* web) {
  RenderViewHost* host = web->render_view_host();
  bool force = false;
  if (args.Length() >= 1) {
    force = args[0]->BooleanValue();
  }
  host->DebugBreak(force);
  return v8::Undefined();
}

v8::Handle<v8::Value> TabNode::PropGetter(v8::Handle<v8::String> prop,
                                          const v8::AccessorInfo& info) {
  TabContents* t = GetTab();
  if (t != NULL) {
    WebContents* web = t->AsWebContents();
    if (prop->Equals(v8::String::New("title"))) {
      std::wstring title = t->GetTitle();
      std::string title2 = WideToUTF8(title);
      return v8::String::New(title2.c_str());
    } else if (web) {
      if (prop->Equals(v8::String::New("attach"))) {
        FunctionNode<WebContents>* f =
            new FunctionNode<WebContents>(TabNode::Attach, web);
        return f->NewInstance();
      } else if (prop->Equals(v8::String::New("detach"))) {
        FunctionNode<WebContents>* f =
            new FunctionNode<WebContents>(TabNode::Detach, web);
        return f->NewInstance();
      } else if (prop->Equals(v8::String::New("sendToDebugger"))) {
        FunctionNode<WebContents>* f =
            new FunctionNode<WebContents>(TabNode::SendToDebugger, web);
        return f->NewInstance();
      } else if (prop->Equals(v8::String::New("debugBreak"))) {
        FunctionNode<WebContents>* f =
            new FunctionNode<WebContents>(TabNode::Break, web);
        return f->NewInstance();
      }
    }
  }
  return v8::Undefined();
}


//////////////////////////////////

template<class T>
v8::Handle<v8::Value> FunctionNode<T>::Function(const v8::Arguments &args) {
  return function_(args, data_);
}

