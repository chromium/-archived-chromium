// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_manager.h"

#include "base/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "googleurl/src/gurl.h"

// static
DevToolsManager* DevToolsManager::GetInstance() {
  return g_browser_process->devtools_manager();
}

// static
void DevToolsManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kDevToolsOpenDocked, false);
}

DevToolsManager::DevToolsManager()
    : inspected_rvh_for_reopen_(NULL),
      in_initial_show_(false) {
}

DevToolsManager::~DevToolsManager() {
  DCHECK(inspected_rvh_to_client_host_.empty());
  DCHECK(client_host_to_inspected_rvh_.empty());
}

DevToolsClientHost* DevToolsManager::GetDevToolsClientHostFor(
    RenderViewHost* inspected_rvh) {
  InspectedRvhToClientHostMap::iterator it =
      inspected_rvh_to_client_host_.find(inspected_rvh);
  if (it != inspected_rvh_to_client_host_.end()) {
    return it->second;
  }
  return NULL;
}

void DevToolsManager::RegisterDevToolsClientHostFor(
    RenderViewHost* inspected_rvh,
    DevToolsClientHost* client_host) {
  DCHECK(!GetDevToolsClientHostFor(inspected_rvh));

  inspected_rvh_to_client_host_[inspected_rvh] = client_host;
  client_host_to_inspected_rvh_[client_host] = inspected_rvh;
  client_host->set_close_listener(this);

  SendAttachToAgent(inspected_rvh);
}

void DevToolsManager::ForwardToDevToolsAgent(
    RenderViewHost* client_rvh,
    const IPC::Message& message) {
  DevToolsClientHost* client_host = FindOnwerDevToolsClientHost(client_rvh);
  if (client_host) {
    ForwardToDevToolsAgent(client_host, message);
  }
}

void DevToolsManager::ForwardToDevToolsAgent(DevToolsClientHost* from,
                                             const IPC::Message& message) {
  RenderViewHost* inspected_rvh = GetInspectedRenderViewHost(from);
  if (!inspected_rvh) {
    // TODO(yurys): notify client that the agent is no longer available
    NOTREACHED();
    return;
  }

  IPC::Message* m = new IPC::Message(message);
  m->set_routing_id(inspected_rvh->routing_id());
  inspected_rvh->Send(m);
}

void DevToolsManager::ForwardToDevToolsClient(RenderViewHost* inspected_rvh,
                                              const IPC::Message& message) {
  DevToolsClientHost* client_host = GetDevToolsClientHostFor(inspected_rvh);
  if (!client_host) {
    // Client window was closed while there were messages
    // being sent to it.
    return;
  }
  client_host->SendMessageToClient(message);
}

void DevToolsManager::ActivateWindow(RenderViewHost* client_rvh) {
  DevToolsClientHost* client_host = FindOnwerDevToolsClientHost(client_rvh);
  if (!client_host) {
    return;
  }
  DevToolsWindow* window = client_host->AsDevToolsWindow();
  DCHECK(window);
  window->Activate();
}

void DevToolsManager::CloseWindow(RenderViewHost* client_rvh) {
  DevToolsClientHost* client_host = FindOnwerDevToolsClientHost(client_rvh);
  if (!client_host) {
    return;
  }
  RenderViewHost* inspected_rvh = GetInspectedRenderViewHost(client_host);
  DCHECK(inspected_rvh);
  SendDetachToAgent(inspected_rvh);

  UnregisterDevToolsClientHostFor(inspected_rvh);
}

void DevToolsManager::DockWindow(RenderViewHost* client_rvh) {
  ReopenWindow(client_rvh, true);
}

void DevToolsManager::UndockWindow(RenderViewHost* client_rvh) {
  ReopenWindow(client_rvh, false);
}

void DevToolsManager::OpenDevToolsWindow(RenderViewHost* inspected_rvh) {
  DevToolsClientHost* host = GetDevToolsClientHostFor(inspected_rvh);
  if (!host) {
    bool docked = inspected_rvh->process()->profile()->GetPrefs()->
        GetBoolean(prefs::kDevToolsOpenDocked);
    host = new DevToolsWindow(
        inspected_rvh->site_instance()->browsing_instance()->profile(),
        inspected_rvh,
        docked);
    RegisterDevToolsClientHostFor(inspected_rvh, host);
  }
  DevToolsWindow* window = host->AsDevToolsWindow();
  if (window) {
    in_initial_show_ = true;
    window->Show();
    in_initial_show_ = false;
  }
}

void DevToolsManager::InspectElement(RenderViewHost* inspected_rvh,
                                     int x,
                                     int y) {
  OpenDevToolsWindow(inspected_rvh);
  IPC::Message* m = new DevToolsAgentMsg_InspectElement(x, y);
  m->set_routing_id(inspected_rvh->routing_id());
  inspected_rvh->Send(m);
}

void DevToolsManager::ClientHostClosing(DevToolsClientHost* host) {
  RenderViewHost* inspected_rvh = GetInspectedRenderViewHost(host);
  if (!inspected_rvh) {
    return;
  }
  SendDetachToAgent(inspected_rvh);

  inspected_rvh_to_client_host_.erase(inspected_rvh);
  client_host_to_inspected_rvh_.erase(host);
}

RenderViewHost* DevToolsManager::GetInspectedRenderViewHost(
    DevToolsClientHost* client_host) {
  ClientHostToInspectedRvhMap::iterator it =
      client_host_to_inspected_rvh_.find(client_host);
  if (it != client_host_to_inspected_rvh_.end()) {
    return it->second;
  }
  return NULL;
}

void DevToolsManager::UnregisterDevToolsClientHostFor(
      RenderViewHost* inspected_rvh) {
  DevToolsClientHost* host = GetDevToolsClientHostFor(inspected_rvh);
  if (!host) {
    return;
  }
  inspected_rvh_to_client_host_.erase(inspected_rvh);
  client_host_to_inspected_rvh_.erase(host);
  if (inspected_rvh_for_reopen_ == inspected_rvh) {
    inspected_rvh_for_reopen_ = NULL;
  }

  // Issue tab closing event post unbound.
  host->InspectedTabClosing();
}

void DevToolsManager::OnNavigatingToPendingEntry(RenderViewHost* rvh,
                                                 RenderViewHost* dest_rvh,
                                                 const GURL& gurl) {
  if (in_initial_show_) {
    // Mute this even in case it is caused by the initial show routines.
    return;
  }
  DevToolsClientHost* client_host =
      GetDevToolsClientHostFor(rvh);
  if (client_host) {
    // Navigating to URL in the inspected window.
    inspected_rvh_to_client_host_.erase(rvh);
    inspected_rvh_to_client_host_[dest_rvh] = client_host;
    client_host_to_inspected_rvh_[client_host] = dest_rvh;
    SendAttachToAgent(dest_rvh);
    return;
  }

  // Iterate over client hosts and if there is one that has render view host
  // changing, reopen entire client window (this must be caused by the user
  // manually refreshing its content).
  for (ClientHostToInspectedRvhMap::iterator it =
           client_host_to_inspected_rvh_.begin();
       it != client_host_to_inspected_rvh_.end(); ++it) {
    DevToolsWindow* window = it->first->AsDevToolsWindow();
    if (window && window->GetRenderViewHost() == rvh) {
      inspected_rvh_for_reopen_ = it->second;
      MessageLoop::current()->PostTask(FROM_HERE,
          NewRunnableMethod(this,
                            &DevToolsManager::ForceReopenWindow));
      return;
    }
  }
}

void DevToolsManager::SendAttachToAgent(RenderViewHost* inspected_rvh) {
  if (inspected_rvh) {
    IPC::Message* m = new DevToolsAgentMsg_Attach();
    m->set_routing_id(inspected_rvh->routing_id());
    inspected_rvh->Send(m);
  }
}

void DevToolsManager::SendDetachToAgent(RenderViewHost* inspected_rvh) {
  if (inspected_rvh) {
    IPC::Message* m = new DevToolsAgentMsg_Detach();
    m->set_routing_id(inspected_rvh->routing_id());
    inspected_rvh->Send(m);
  }
}

void DevToolsManager::ForceReopenWindow() {
  if (inspected_rvh_for_reopen_) {
    RenderViewHost* inspected_rvn = inspected_rvh_for_reopen_;
    SendDetachToAgent(inspected_rvn);
    UnregisterDevToolsClientHostFor(inspected_rvn);
    OpenDevToolsWindow(inspected_rvn);
  }
}

DevToolsClientHost* DevToolsManager::FindOnwerDevToolsClientHost(
    RenderViewHost* client_rvh) {
  for (InspectedRvhToClientHostMap::iterator it =
           inspected_rvh_to_client_host_.begin();
       it != inspected_rvh_to_client_host_.end();
       ++it) {
    DevToolsWindow* win = it->second->AsDevToolsWindow();
    if (!win) {
      continue;
    }
    if (client_rvh == win->GetRenderViewHost()) {
      return it->second;
    }
  }
  return NULL;
}

void DevToolsManager::ReopenWindow(RenderViewHost* client_rvh, bool docked) {
  DevToolsClientHost* client_host = FindOnwerDevToolsClientHost(client_rvh);
  if (!client_host) {
    return;
  }
  RenderViewHost* inspected_rvh = GetInspectedRenderViewHost(client_host);
  DCHECK(inspected_rvh);
  inspected_rvh->process()->profile()->GetPrefs()->SetBoolean(
      prefs::kDevToolsOpenDocked, docked);

  DevToolsWindow* window = client_host->AsDevToolsWindow();
  DCHECK(window);
  window->SetDocked(docked);
}
