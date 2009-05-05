// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_manager.h"

#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"
#include "googleurl/src/gurl.h"

DevToolsManager::DevToolsManager() : tab_contents_listeners_(NULL) {
}

DevToolsManager::~DevToolsManager() {
  DCHECK(!tab_contents_listeners_.get()) <<
      "All devtools client hosts must alredy have been destroyed.";
  DCHECK(navcontroller_to_client_host_.empty());
  DCHECK(client_host_to_navcontroller_.empty());
}

void DevToolsManager::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DISCONNECTED);

  if (type == NotificationType::TAB_CONTENTS_DISCONNECTED) {
    Source<TabContents> src(source);
    DevToolsClientHost* client_host = GetDevToolsClientHostFor(*src.ptr());
    if (!client_host) {
      return;
    }

    // Active tab contents disconnecting from its renderer means that the tab
    // is closing.
    client_host->InspectedTabClosing();
    UnregisterDevToolsClientHost(client_host, &src->controller());
  }
}

DevToolsClientHost* DevToolsManager::GetDevToolsClientHostFor(
    const TabContents& tab_contents) {
  const NavigationController& navigation_controller = tab_contents.controller();
  ClientHostMap::const_iterator it =
      navcontroller_to_client_host_.find(&navigation_controller);
  if (it != navcontroller_to_client_host_.end()) {
    return it->second;
  }
  return NULL;
}

void DevToolsManager::RegisterDevToolsClientHostFor(
    TabContents& tab_contents,
    DevToolsClientHost* client_host) {
  DCHECK(!GetDevToolsClientHostFor(tab_contents));

  NavigationController* navigation_controller = &tab_contents.controller();
  navcontroller_to_client_host_[navigation_controller] = client_host;
  client_host_to_navcontroller_[client_host] = navigation_controller;
  client_host->set_close_listener(this);

  StartListening(navigation_controller);
  SendAttachToAgent(tab_contents.render_view_host());
}

void DevToolsManager::ForwardToDevToolsAgent(
    const RenderViewHost& client_rvh,
    const IPC::Message& message) {
    for (ClientHostMap::const_iterator it =
             navcontroller_to_client_host_.begin();
       it != navcontroller_to_client_host_.end();
       ++it) {
    DevToolsWindow* win = it->second->AsDevToolsWindow();
    if (!win) {
      continue;
    }
    if (win->HasRenderViewHost(client_rvh)) {
      ForwardToDevToolsAgent(*win, message);
      return;
    }
  }
}

void DevToolsManager::ForwardToDevToolsAgent(const DevToolsClientHost& from,
                                             const IPC::Message& message) {
  NavigationController* nav_controller =
      GetDevToolsAgentNavigationController(from);
  if (!nav_controller) {
    NOTREACHED();
    return;
  }

  // TODO(yurys): notify client that the agent is no longer available
  TabContents* tc = nav_controller->tab_contents();
  if (!tc) {
    return;
  }
  RenderViewHost* target_host = tc->render_view_host();
  if (!target_host) {
    return;
  }

  IPC::Message* m = new IPC::Message(message);
  m->set_routing_id(target_host->routing_id());
  target_host->Send(m);
}

void DevToolsManager::ForwardToDevToolsClient(const RenderViewHost& from,
                                              const IPC::Message& message) {
  TabContents* tc = from.delegate()->GetAsTabContents();
  DevToolsClientHost* target_host = GetDevToolsClientHostFor(*tc);
  if (!target_host) {
    // Client window was closed while there were messages
    // being sent to it.
    return;
  }
  target_host->SendMessageToClient(message);
}

void DevToolsManager::OpenDevToolsWindow(TabContents* wc) {
  DevToolsClientHost* host = GetDevToolsClientHostFor(*wc);
  if (!host) {
    host = DevToolsWindow::Create();
    RegisterDevToolsClientHostFor(*wc, host);
  }
  DevToolsWindow* window = host->AsDevToolsWindow();
  if (window)
    window->Show();
}

void DevToolsManager::InspectElement(TabContents* wc, int x, int y) {
  OpenDevToolsWindow(wc);
  RenderViewHost* target_host = wc->render_view_host();
  if (!target_host) {
    return;
  }
  IPC::Message* m = new DevToolsAgentMsg_InspectElement(x, y);
  m->set_routing_id(target_host->routing_id());
  target_host->Send(m);
}

void DevToolsManager::ClientHostClosing(DevToolsClientHost* host) {
  NavigationController* controller = GetDevToolsAgentNavigationController(
      *host);
  if (!controller) {
    return;
  }

  TabContents* tab_contents = controller->tab_contents();
  if (!tab_contents) {
    return;
  }
  SendDetachToAgent(tab_contents->render_view_host());
  UnregisterDevToolsClientHost(host, controller);
}

NavigationController* DevToolsManager::GetDevToolsAgentNavigationController(
    const DevToolsClientHost& client_host) {
  NavControllerMap::const_iterator it =
      client_host_to_navcontroller_.find(&client_host);
  if (it != client_host_to_navcontroller_.end()) {
    return it->second;
  }
  return NULL;
}

void DevToolsManager::UnregisterDevToolsClientHost(
      DevToolsClientHost* client_host,
      NavigationController* controller) {
  navcontroller_to_client_host_.erase(controller);
  client_host_to_navcontroller_.erase(client_host);
  StopListening(controller);
}

void DevToolsManager::StartListening(
    NavigationController* navigation_controller) {
  // TODO(yurys): add render host change listener
  if (!tab_contents_listeners_.get()) {
    tab_contents_listeners_.reset(new NotificationRegistrar);
    tab_contents_listeners_->Add(
        this,
        NotificationType::TAB_CONTENTS_DISCONNECTED,
        NotificationService::AllSources());
  }
}

void DevToolsManager::StopListening(
    NavigationController* navigation_controller) {
  DCHECK(tab_contents_listeners_.get());
  if (navcontroller_to_client_host_.empty()) {
    DCHECK(client_host_to_navcontroller_.empty());
    tab_contents_listeners_.reset();
  }
}

void DevToolsManager::OnNavigatingToPendingEntry(const TabContents& wc,
                                                 RenderViewHost* target_host) {
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(wc);
  if (client_host) {
    const NavigationEntry& entry = *wc.controller().pending_entry();
    client_host->SetInspectedTabUrl(entry.url().possibly_invalid_spec());
    SendAttachToAgent(target_host);
  }
}

void DevToolsManager::SendAttachToAgent(RenderViewHost* target_host) {
  if (target_host) {
    IPC::Message* m = new DevToolsAgentMsg_Attach();
    m->set_routing_id(target_host->routing_id());
    target_host->Send(m);
  }
}

void DevToolsManager::SendDetachToAgent(RenderViewHost* target_host) {
  if (target_host) {
    IPC::Message* m = new DevToolsAgentMsg_Detach();
    m->set_routing_id(target_host->routing_id());
    target_host->Send(m);
  }
}
