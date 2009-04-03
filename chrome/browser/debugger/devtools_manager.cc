// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_manager.h"

#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"

DevToolsManager::DevToolsManager() : web_contents_listeners_(NULL) {
}

DevToolsManager::~DevToolsManager() {
  DCHECK(!web_contents_listeners_.get()) <<
      "All devtools client hosts must alredy have been destroyed.";
  DCHECK(navcontroller_to_client_host_.empty());
  DCHECK(client_host_to_navcontroller_.empty());
}

void DevToolsManager::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(type == NotificationType::WEB_CONTENTS_DISCONNECTED ||
      type == NotificationType::WEB_CONTENTS_SWAPPED);

  if (type == NotificationType::WEB_CONTENTS_DISCONNECTED) {
    Source<WebContents> src(source);
    DevToolsClientHost* client_host = GetDevToolsClientHostFor(*src.ptr());
    if (!client_host) {
      return;
    }

    NavigationController* controller = src->controller();
    bool active = (controller->active_contents() == src.ptr());
    if (active) {
      // Active tab contents disconnecting from its renderer means that the tab
      // is closing.
      client_host->InspectedTabClosing();
      UnregisterDevToolsClientHost(client_host, controller);
    }
  } else if (type == NotificationType::WEB_CONTENTS_SWAPPED) {
    Source<WebContents> src(source);
    DevToolsClientHost* client_host = GetDevToolsClientHostFor(*src.ptr());
    if (client_host) {
      SendAttachToAgent(*src.ptr());
    }
  }
}

DevToolsClientHost* DevToolsManager::GetDevToolsClientHostFor(
    const WebContents& web_contents) {
  NavigationController* navigation_controller = web_contents.controller();
  ClientHostMap::const_iterator it =
      navcontroller_to_client_host_.find(navigation_controller);
  if (it != navcontroller_to_client_host_.end()) {
    return it->second;
  }
  return NULL;
}

void DevToolsManager::RegisterDevToolsClientHostFor(
    const WebContents& web_contents,
    DevToolsClientHost* client_host) {
  DCHECK(!GetDevToolsClientHostFor(web_contents));

  NavigationController* navigation_controller = web_contents.controller();
  navcontroller_to_client_host_[navigation_controller] = client_host;
  client_host_to_navcontroller_[client_host] = navigation_controller;
  client_host->set_close_listener(this);

  StartListening(navigation_controller);
  SendAttachToAgent(web_contents);
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
  TabContents* tc = nav_controller->active_contents();
  if (!tc) {
    return;
  }
  WebContents* wc = tc->AsWebContents();
  if (!wc) {
    return;
  }
  RenderViewHost* target_host = wc->render_view_host();
  if (!target_host) {
    return;
  }

  IPC::Message* m = new IPC::Message(message);
  m->set_routing_id(target_host->routing_id());
  target_host->Send(m);
}

void DevToolsManager::ForwardToDevToolsClient(const RenderViewHost& from,
                                              const IPC::Message& message) {
  WebContents* wc = from.delegate()->GetAsWebContents();
  if (!wc) {
    NOTREACHED();
    return;
  }
  DevToolsClientHost* target_host = GetDevToolsClientHostFor(*wc);
  if (!target_host) {
    NOTREACHED();
    return;
  }
  target_host->SendMessageToClient(message);
}

void DevToolsManager::OpenDevToolsWindow(WebContents* wc) {
  DevToolsClientHost* host = GetDevToolsClientHostFor(*wc);
  if (!host) {
    host = DevToolsWindow::Create();
    RegisterDevToolsClientHostFor(*wc, host);
  }
  DevToolsWindow* window = host->AsDevToolsWindow();
  if (window)
    window->Show();
}

void DevToolsManager::InspectElement(WebContents* wc, int x, int y) {
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

  TabContents* tab_contents = controller->active_contents();
  if (!tab_contents) {
    return;
  }
  WebContents* web_contents = tab_contents->AsWebContents();
  if (!web_contents) {
    return;
  }
  SendDetachToAgent(*web_contents);
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
  if (!web_contents_listeners_.get()) {
    web_contents_listeners_.reset(new NotificationRegistrar);
    web_contents_listeners_->Add(
        this,
        NotificationType::WEB_CONTENTS_DISCONNECTED,
        NotificationService::AllSources());
    web_contents_listeners_->Add(
        this,
        NotificationType::WEB_CONTENTS_SWAPPED,
        NotificationService::AllSources());
  }
}

void DevToolsManager::StopListening(
    NavigationController* navigation_controller) {
  DCHECK(web_contents_listeners_.get());
  if (navcontroller_to_client_host_.empty()) {
    DCHECK(client_host_to_navcontroller_.empty());
    web_contents_listeners_.reset();
  }
}

void DevToolsManager::SendAttachToAgent(const WebContents& wc) {
  RenderViewHost* target_host = wc.render_view_host();
  if (target_host) {
    IPC::Message* m = new DevToolsAgentMsg_Attach();
    m->set_routing_id(target_host->routing_id());
    target_host->Send(m);
  }
}

void DevToolsManager::SendDetachToAgent(const WebContents& wc) {
  RenderViewHost* target_host = wc.render_view_host();
  if (target_host) {
    IPC::Message* m = new DevToolsAgentMsg_Detach();
    m->set_routing_id(target_host->routing_id());
    target_host->Send(m);
  }
}
