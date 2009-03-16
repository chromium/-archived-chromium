// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_manager.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/render_view_host_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"


class DevToolsInstanceDescriptorImpl : public DevToolsInstanceDescriptor {
 public:
  explicit DevToolsInstanceDescriptorImpl(
      NavigationController* navigation_controller)
      : navigation_controller_(navigation_controller),
        devtools_host_(NULL),
        devtools_window_(NULL) {
  }
  virtual ~DevToolsInstanceDescriptorImpl() {}

  virtual void SetDevToolsHost(RenderViewHost* render_view_host) {
    devtools_host_ = render_view_host;
  }

  virtual void SetDevToolsWindow(DevToolsWindow* window) {
    devtools_window_ = window;
  }

  virtual void Destroy() {
    DevToolsManager* manager = g_browser_process->devtools_manager();
    DCHECK(manager);
    if (manager) {
      manager->RemoveDescriptor(this);
    }
    delete this;
  }

  RenderViewHost* devtools_host() const {
    return devtools_host_;
  }

  DevToolsWindow* devtools_window() const {
    return devtools_window_;
  }

  NavigationController* navigation_controller() const {
    return navigation_controller_;
  }

 private:
  NavigationController* navigation_controller_;
  RenderViewHost* devtools_host_;
  DevToolsWindow* devtools_window_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsInstanceDescriptorImpl);
};

DevToolsManager::DevToolsManager() : web_contents_listeners_(NULL) {
}

DevToolsManager::~DevToolsManager() {
  DCHECK(!web_contents_listeners_.get()) <<
      "All devtools windows must alredy have been closed.";
}

void DevToolsManager::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(type == NotificationType::WEB_CONTENTS_DISCONNECTED);
  Source<WebContents> src(source);
  NavigationController* controller = src->controller();
  DescriptorMap::const_iterator it =
      navcontroller_to_descriptor_.find(controller);
  if (it == navcontroller_to_descriptor_.end()) {
    return;
  }
  bool active = (controller->active_contents() == src.ptr());
  if (!active) {
    return;
  }
  // Active tab contents disconnecting from its renderer means that the tab
  // is closing so we are closing devtools as well.
  it->second->devtools_window()->Close();
}

void DevToolsManager::ShowDevToolsForWebContents(WebContents* web_contents) {
  NavigationController* navigation_controller = web_contents->controller();

  DevToolsWindow* window(NULL);
  DevToolsInstanceDescriptorImpl* desc(NULL);
  DescriptorMap::const_iterator it =
      navcontroller_to_descriptor_.find(navigation_controller);
  if (it != navcontroller_to_descriptor_.end()) {
    desc = it->second;
    window = desc->devtools_window();
  } else {
    desc = new DevToolsInstanceDescriptorImpl(navigation_controller);
    navcontroller_to_descriptor_[navigation_controller] = desc;

    StartListening(navigation_controller);

    window = DevToolsWindow::Create(desc);
  }

  window->Show();
}

void DevToolsManager::ForwardToDevToolsAgent(RenderViewHost* from,
                                             const IPC::Message& message) {
  NavigationController* nav_controller(NULL);
  for (DescriptorMap::const_iterator it = navcontroller_to_descriptor_.begin();
       it != navcontroller_to_descriptor_.end();
       ++it) {
    if (it->second->devtools_host() == from) {
      nav_controller = it->second->navigation_controller();
      break;
    }
  }

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

void DevToolsManager::ForwardToDevToolsClient(RenderViewHost* from,
                                              const IPC::Message& message) {
  WebContents* wc = from->delegate()->GetAsWebContents();
  if (!wc) {
    NOTREACHED();
    return;
  }

  NavigationController* navigation_controller = wc->controller();

  DescriptorMap::const_iterator it =
      navcontroller_to_descriptor_.find(navigation_controller);
  if (it == navcontroller_to_descriptor_.end()) {
    NOTREACHED();
    return;
  }

  RenderViewHost* target_host = it->second->devtools_host();
  IPC::Message* m =  new IPC::Message(message);
  m->set_routing_id(target_host->routing_id());
  target_host->Send(m);
}

void DevToolsManager::RemoveDescriptor(
    DevToolsInstanceDescriptorImpl* descriptor) {
  NavigationController* navigation_controller =
      descriptor->navigation_controller();
  // This should be done before StopListening as the latter checks number of
  // alive devtools instances.
  navcontroller_to_descriptor_.erase(navigation_controller);
  StopListening(navigation_controller);
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
  }
}

void DevToolsManager::StopListening(
    NavigationController* navigation_controller) {
  DCHECK(web_contents_listeners_.get());
  if (navcontroller_to_descriptor_.empty()) {
    web_contents_listeners_.reset();
  }
}
