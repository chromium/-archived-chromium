// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/debugger_host_impl.h"

#include "base/json_reader.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/debugger/debugger_io.h"
#include "chrome/browser/debugger/debugger_wrapper.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/notification_service.h"

class TabContentsReference : public NotificationObserver {
 public:
  TabContentsReference(TabContents *c) : navigation_controller_(NULL) {
    navigation_controller_ = c->controller();

    NotificationService* service = NotificationService::current();
    DCHECK(service);
    service->AddObserver(this,
                         NotificationType::TAB_CLOSING,
                         Source<NavigationController>(navigation_controller_));
    observing_ = true;
  }

  virtual ~TabContentsReference() {
    StopObserving();
  }

  // NotificationObserver impl
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    StopObserving();
    navigation_controller_ = NULL;
  }

  TabContents* GetTabContents() {
    if (navigation_controller_) {
      return navigation_controller_->active_contents();
    } else {
      return NULL;
    }
  }
 private:
  void StopObserving() {
    if (observing_ && navigation_controller_) {
      NotificationService* service = NotificationService::current();
      DCHECK(service);
      service->RemoveObserver(
          this,
          NotificationType::TAB_CLOSING,
          Source<NavigationController>(navigation_controller_));
      observing_ = false;
    }
  }

  NavigationController* navigation_controller_;
  bool observing_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsReference);
};


DebuggerHostImpl::DebuggerHostImpl(DebuggerInputOutput* io)
    : io_(io),
      debugger_ready_(true) {
}

DebuggerHostImpl::~DebuggerHostImpl() {
  io_->Stop();
  io_ = NULL;
}

void DebuggerHostImpl::Start() {
  io_->Start(this);
}

void DebuggerHostImpl::Debug(TabContents* tab) {
  tab_reference_.reset(new TabContentsReference(tab));
}

void DebuggerHostImpl::DebugMessage(const std::wstring& msg) {

  Value* msg_value = JSONReader::Read(WideToUTF8(msg), false);
  if (!msg_value) {
    msg_value = Value::CreateStringValue(L"Message parse error!");
  }
  ListValue* argv = new ListValue;
  argv->Append(msg_value);
  io_->CallFunctionInPage(L"response", argv);
}

void DebuggerHostImpl::OnDebugAttach() {
  std::wstring title;
  const TabContents* t = GetTabContentsBeingDebugged();
  if (t) {
    title = UTF16ToWideHack(t->GetTitle());
  }

  ListValue* argv = new ListValue;
  argv->Append(Value::CreateStringValue(title));
  io_->CallFunctionInPage(L"on_attach", argv);
}

void DebuggerHostImpl::OnDebugDisconnect() {
  ListValue* argv = new ListValue;
  io_->CallFunctionInPage(L"on_disconnect", argv);
}

void DebuggerHostImpl::DidDisconnect() {
  // TODO(yurys): just send Detach from here?
  ListValue* argv = new ListValue;
  io_->CallFunctionInPage(L"exit", argv);
}

void DebuggerHostImpl::OnDebuggerHostMsg(const ListValue* args) {
  if (args->GetSize() < 1) {
    NOTREACHED();
    return;
  }
  std::wstring methodName;
  Value* value = NULL;
  if (!args->Get(0, &value) || !value->GetAsString(&methodName)) {
    NOTREACHED();
    return;
  }

  const TabContents* t = GetTabContentsBeingDebugged();
  if (t == NULL) {
    NOTREACHED();
    return;
  }
  const WebContents* web = t->AsWebContents();
  if (web == NULL) {
    NOTREACHED();
    return;
  }
  RenderViewHost* host = web->render_view_host();

  if (methodName == L"attach") {
    host->DebugAttach();
  } else if (methodName == L"detach") {
    host->DebugDetach();
  } else if (methodName == L"debugBreak") {
    std::wstring force;
    Value* value = NULL;
    if (!args->Get(1, &value) || !value->GetAsString(&force)) {
      NOTREACHED();
      return;
    }
    host->DebugBreak(force == L"true");
  } else if (methodName == L"sendToDebugger") {
    std::wstring cmd;
    Value* value = NULL;
    if (!args->Get(1, &value) || !value->GetAsString(&cmd)) {
      NOTREACHED();
      return;
    }
    host->DebugCommand(cmd);

  } else if (methodName == L"setDebuggerReady") {
    std::wstring ready;
    Value* value = NULL;
    if (!args->Get(1, &value) || !value->GetAsString(&ready)) {
      NOTREACHED();
      return;
    }
    io_->SetDebuggerReady(ready == L"true");
  } else if (methodName == L"setDebuggerBreak") {
    std::wstring brk;
    Value* value = NULL;
    if (!args->Get(1, &value) || !value->GetAsString(&brk)) {
      NOTREACHED();
      return;
    }
    io_->SetDebuggerBreak(brk == L"true");
  }
}

TabContents* DebuggerHostImpl::GetTabContentsBeingDebugged() const {
  if (tab_reference_ != NULL) {
    return tab_reference_->GetTabContents();
  } else {
    return NULL;
  }
}
