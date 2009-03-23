// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/common/notification_service.h"

namespace IPC {
class Message;
}

class DevToolsClientHost;
class NavigationController;
class NotificationRegistrar;
class RenderViewHost;
class WebContents;

// This class is a singleton that manages DevToolsClientHost instances and
// routes messages between developer tools clients and agents.
class DevToolsManager : public NotificationObserver,
                        public DevToolsClientHost::CloseListener {
 public:
  DevToolsManager();
  virtual ~DevToolsManager();

  // Returns DevToolsClientHost registered for |web_contents| or NULL if
  // there is no alive DevToolsClientHost registered for |web_contents|.
  DevToolsClientHost* GetDevToolsClientHostFor(const WebContents& web_contents);

  // Registers new DevToolsClientHost for |web_contents|. There must be no
  // other DevToolsClientHosts registered for the WebContents at the moment.
  void RegisterDevToolsClientHostFor(const WebContents& web_contents,
                                     DevToolsClientHost* client_host);

  void ForwardToDevToolsAgent(const RenderViewHost& client_rvh,
                              const IPC::Message& message);
  void ForwardToDevToolsAgent(const DevToolsClientHost& from,
                              const IPC::Message& message);
  void ForwardToDevToolsClient(const RenderViewHost& from,
                               const IPC::Message& message);

 private:
  // NotificationObserver override.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // DevToolsClientHost::CloseListener override.
  // This method will remove all references from the manager to the
  // DevToolsClientHost and unregister all listeners related to the
  // DevToolsClientHost.
  virtual void ClientHostClosing(DevToolsClientHost* host);

  // Returns NavigationController for the tab that is inspected by devtools
  // client hosted by DevToolsClientHost.
  NavigationController* GetDevToolsAgentNavigationController(
      const DevToolsClientHost& client_host);

  void StartListening(NavigationController* navigation_controller);
  void StopListening(NavigationController* navigation_controller);

  // This object is not NULL iff there is at least one registered
  // DevToolsClientHost.
  scoped_ptr<NotificationRegistrar> web_contents_listeners_;

  // These two maps are for tracking dependencies between inspected tabs and
  // their DevToolsClientHosts. They are usful for routing devtools messages
  // and allow us to have at most one devtools client host per tab. We use
  // NavigationController* as key because it survives crosee-site navigation in
  // cases when tab contents may change.
  //
  // DevToolsManager start listening to DevToolsClientHosts when they are put
  // into these maps and removes them when they are closing.
  typedef std::map<const NavigationController*,
                   DevToolsClientHost*> ClientHostMap;
  ClientHostMap navcontroller_to_client_host_;

  typedef std::map<const DevToolsClientHost*,
                   NavigationController*> NavControllerMap;
  NavControllerMap client_host_to_navcontroller_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsManager);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_
