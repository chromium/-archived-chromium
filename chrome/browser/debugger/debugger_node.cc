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
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/debugger/debugger_shell.h"
#include "chrome/common/notification_service.h"

DebuggerNode::DebuggerNode() : data_(NULL), valid_(true) {
}

void DebuggerNode::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  registrar_.RemoveAll();
  data_ = NULL;
  Invalidate();
}

DebuggerNode::~DebuggerNode() {
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
    instance->SetNamedPropertyHandler(&DebuggerNode::NodeGetter, 0, 0, 0, 0,
                                      node);
    // TODO(erikkay): verify that the interceptor does not have to be
    // behind the object
  }
  if (IsCollection()) {
    instance->SetIndexedPropertyHandler(&DebuggerNode::NodeIndex, 0, 0, 0, 0,
                                        node);
  }
  v8::Local<v8::Object> ret = instance->NewInstance();
  v8::Persistent<v8::Object> p = v8::Persistent<v8::Object>::New(ret);
  p.MakeWeak(wrap, &DebuggerShell::HandleWeakReference);
  return ret;
}

v8::Handle<v8::Value> DebuggerNode::NodeGetter(v8::Local<v8::String> prop,
                                            const v8::AccessorInfo& info) {
  DebuggerNodeWrapper* w = static_cast<DebuggerNodeWrapper*>(v8::External::Cast(
        *info.Data())->Value());
  DebuggerNode* n = w->node();
  return (n->IsValid() && n->IsObject()) ? n->PropGetter(prop, info) :
      static_cast<v8::Handle<v8::Value> >(v8::Undefined());
}

v8::Handle<v8::Value> DebuggerNode::NodeIndex(uint32_t index,
                                           const v8::AccessorInfo& info) {
  DebuggerNodeWrapper* w = static_cast<DebuggerNodeWrapper*>(v8::External::Cast(
      *info.Data())->Value());
  DebuggerNode* n = w->node();
  return (n->IsValid() && n->IsCollection()) ? n->IndexGetter(index, info) :
      static_cast<v8::Handle<v8::Value> >(v8::Undefined());
}

v8::Handle<v8::Value> DebuggerNode::NodeFunc(const v8::Arguments& args) {
  DebuggerNodeWrapper* w = static_cast<DebuggerNodeWrapper*>(v8::External::Cast(
      *args.Data())->Value());
  DebuggerNode* n = w->node();
  return (n->IsValid() && n->IsFunction()) ? n->Function(args) :
      static_cast<v8::Handle<v8::Value> >(v8::Undefined());
}


/////////////////////////////////////////////

ChromeNode::ChromeNode(DebuggerShell* debugger) {
  debugger_ = debugger;
}

ChromeNode::~ChromeNode() {
}

v8::Handle<v8::Value> ChromeNode::PropGetter(v8::Handle<v8::String> prop,
                                             const v8::AccessorInfo& info) {
  if (prop->Equals(v8::String::New("pid")))
    return v8::Number::New(base::GetCurrentProcId());
  if (prop->Equals(v8::String::New("browser")))
    return BrowserListNode::BrowserList()->NewInstance();
  if (prop->Equals(v8::String::New("setDebuggerReady"))) {
    return (new FunctionNode<DebuggerShell>(DebuggerShell::SetDebuggerReady,
                                            debugger_))->NewInstance();
  }
  if (prop->Equals(v8::String::New("setDebuggerBreak"))) {
    return (new FunctionNode<DebuggerShell>(DebuggerShell::SetDebuggerBreak,
                                            debugger_))->NewInstance();
  }
  return static_cast<v8::Handle<v8::Value> >(
      prop->Equals(v8::String::New("foo")) ? v8::Undefined() : prop);
}

/////////////////////////////////////////////

BrowserNode::BrowserNode(Browser *b) {
  data_ = b;
  registrar_.Add(this, NotificationType::BROWSER_CLOSED, Source<Browser>(b));
}

BrowserNode* BrowserNode::BrowserAtIndex(int index) {
  if (index >= 0) {
    BrowserList::const_iterator iter = BrowserList::begin();
    for (; (iter != BrowserList::end()) && (index > 0); ++iter, --index)
      ;
    if (iter != BrowserList::end())
      return new BrowserNode(*iter);
  }
  return NULL;
}

BrowserNode::~BrowserNode() {
}

Browser* BrowserNode::GetBrowser() {
  return IsValid() ? static_cast<Browser*>(data_) : NULL;
}

v8::Handle<v8::Value> BrowserNode::PropGetter(v8::Handle<v8::String> prop,
                                              const v8::AccessorInfo& info) {
  Browser* b = GetBrowser();
  if (b != NULL) {
    if (prop->Equals(v8::String::New("title"))) {
      return v8::String::New(UTF16ToUTF8(
          b->GetSelectedTabContents()->GetTitle()).c_str());
    }
    if (prop->Equals(v8::String::New("tab")))
      return TabListNode::TabList(b)->NewInstance();
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
  return b ? b->NewInstance() :
      static_cast<v8::Handle<v8::Value> >(v8::Undefined());
}

/////////////////////////////////////////////

TabListNode::TabListNode(Browser* b) {
  data_ = b;
  registrar_.Add(this, NotificationType::BROWSER_CLOSED, Source<Browser>(b));
}

TabListNode::~TabListNode() {
}

TabListNode* TabListNode::TabList(Browser* b) {
  return new TabListNode(b);
}

Browser* TabListNode::GetBrowser() {
  return IsValid() ? static_cast<Browser*>(data_) : NULL;
}

v8::Handle<v8::Value> TabListNode::IndexGetter(uint32_t index,
                                               const v8::AccessorInfo& info) {
  Browser* b = GetBrowser();
  if (b != NULL) {
    TabContents* tab_contents = b->GetTabContentsAt(index);
    if (tab_contents)
      return (new TabNode(tab_contents))->NewInstance();
  }
  return v8::Undefined();
}

/////////////////////////////////////////////

TabNode::TabNode(TabContents* c) {
  NavigationController* controller = &c->controller();
  data_ = controller;
  registrar_.Add(this, NotificationType::TAB_CLOSING,
                 Source<NavigationController>(controller));
}

TabNode::~TabNode() {
}

TabContents* TabNode::GetTab() {
  return IsValid() ?
      static_cast<NavigationController*>(data_)->tab_contents() : NULL;
}

v8::Handle<v8::Value> TabNode::SendToDebugger(const v8::Arguments& args,
                                              TabContents* tab) {
  RenderViewHost* host = tab->render_view_host();
  if (args.Length() == 1) {
    std::wstring cmd;
    v8::Handle<v8::Value> obj = args[0];
    DebuggerShell::ObjectToString(obj, &cmd);
    host->DebugCommand(cmd);
  }
  return v8::Undefined();
}

v8::Handle<v8::Value> TabNode::Attach(const v8::Arguments& args,
                                      TabContents* tab) {
  RenderViewHost* host = tab->render_view_host();
  host->DebugAttach();
  return v8::Int32::New(host->process()->process().pid());
}

v8::Handle<v8::Value> TabNode::Detach(const v8::Arguments& args,
                                      TabContents* tab) {
  RenderViewHost* host = tab->render_view_host();
  host->DebugDetach();
  return v8::Int32::New(host->process()->process().pid());
}

v8::Handle<v8::Value> TabNode::Break(const v8::Arguments& args,
                                     TabContents* tab) {
  tab->render_view_host()->DebugBreak((args.Length() >= 1) ?
      args[0]->BooleanValue() : false);
  return v8::Undefined();
}

v8::Handle<v8::Value> TabNode::PropGetter(v8::Handle<v8::String> prop,
                                          const v8::AccessorInfo& info) {
  TabContents* tab = GetTab();
  if (tab != NULL) {
    if (prop->Equals(v8::String::New("title")))
      return v8::String::New(UTF16ToUTF8(tab->GetTitle()).c_str());
    if (prop->Equals(v8::String::New("attach"))) {
      return (new FunctionNode<TabContents>(TabNode::Attach,
                                            tab))->NewInstance();
    }
    if (prop->Equals(v8::String::New("detach"))) {
      return (new FunctionNode<TabContents>(TabNode::Detach,
                                            tab))->NewInstance();
    }
    if (prop->Equals(v8::String::New("sendToDebugger"))) {
      return (new FunctionNode<TabContents>(TabNode::SendToDebugger,
                                            tab))->NewInstance();
    }
    if (prop->Equals(v8::String::New("debugBreak"))) {
      return (new FunctionNode<TabContents>(TabNode::Break,
                                            tab))->NewInstance();
    }
  }
  return v8::Undefined();
}


//////////////////////////////////

template<class T>
v8::Handle<v8::Value> FunctionNode<T>::Function(const v8::Arguments &args) {
  return function_(args, data_);
}
