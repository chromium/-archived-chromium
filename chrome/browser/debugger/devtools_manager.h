// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_

#include <map>

#include "base/ref_counted.h"
#include "chrome/browser/debugger/devtools_client_host.h"

namespace IPC {
class Message;
}

class GURL;
class PrefService;
class RenderViewHost;

// This class is a singleton that manages DevToolsClientHost instances and
// routes messages between developer tools clients and agents.
class DevToolsManager : public DevToolsClientHost::CloseListener,
                        public base::RefCounted<DevToolsManager> {
 public:
  static DevToolsManager* GetInstance();

  static void RegisterUserPrefs(PrefService* prefs);

  DevToolsManager();
  virtual ~DevToolsManager();

  // Returns DevToolsClientHost registered for |inspected_rvh| or NULL if
  // there is no alive DevToolsClientHost registered for |inspected_rvh|.
  DevToolsClientHost* GetDevToolsClientHostFor(RenderViewHost* inspected_rvh);

  // Registers new DevToolsClientHost for |inspected_rvh|. There must be no
  // other DevToolsClientHosts registered for the RenderViewHost at the moment.
  void RegisterDevToolsClientHostFor(RenderViewHost* inspected_rvh,
                                     DevToolsClientHost* client_host);
  void UnregisterDevToolsClientHostFor(RenderViewHost* inspected_rvh);

  void ForwardToDevToolsAgent(RenderViewHost* client_rvh,
                              const IPC::Message& message);
  void ForwardToDevToolsAgent(DevToolsClientHost* from,
                              const IPC::Message& message);
  void ForwardToDevToolsClient(RenderViewHost* inspected_rvh,
                               const IPC::Message& message);

  void ActivateWindow(RenderViewHost* client_rvn);
  void CloseWindow(RenderViewHost* client_rvn);
  void DockWindow(RenderViewHost* client_rvn);
  void UndockWindow(RenderViewHost* client_rvn);

  void OpenDevToolsWindow(RenderViewHost* inspected_rvh);

  // Starts element inspection in the devtools client.
  // Creates one by means of OpenDevToolsWindow if no client
  // exists.
  void InspectElement(RenderViewHost* inspected_rvh, int x, int y);

  // Sends 'Attach' message to the agent using |dest_rvh| in case
  // there is a DevToolsClientHost registered for the |inspected_rvh|.
  void OnNavigatingToPendingEntry(RenderViewHost* inspected_rvh,
                                  RenderViewHost* dest_rvh,
                                  const GURL& gurl);

private:
  // DevToolsClientHost::CloseListener override.
  // This method will remove all references from the manager to the
  // DevToolsClientHost and unregister all listeners related to the
  // DevToolsClientHost.
  virtual void ClientHostClosing(DevToolsClientHost* host);

  // Returns RenderViewHost for the tab that is inspected by devtools
  // client hosted by DevToolsClientHost.
  RenderViewHost* GetInspectedRenderViewHost(DevToolsClientHost* client_host);

  void SendAttachToAgent(RenderViewHost* inspected_rvh);
  void SendDetachToAgent(RenderViewHost* inspected_rvh);

  void ForceReopenWindow();

  DevToolsClientHost* FindOnwerDevToolsClientHost(RenderViewHost* client_rvh);

  void ReopenWindow(RenderViewHost* client_rvh, bool docked);

  // These two maps are for tracking dependencies between inspected tabs and
  // their DevToolsClientHosts. They are usful for routing devtools messages
  // and allow us to have at most one devtools client host per tab. We use
  // NavigationController* as key because it survives crosee-site navigation in
  // cases when tab contents may change.
  //
  // DevToolsManager start listening to DevToolsClientHosts when they are put
  // into these maps and removes them when they are closing.
  typedef std::map<RenderViewHost*, DevToolsClientHost*>
      InspectedRvhToClientHostMap;
  InspectedRvhToClientHostMap inspected_rvh_to_client_host_;

  typedef std::map<DevToolsClientHost*, RenderViewHost*>
      ClientHostToInspectedRvhMap;
  ClientHostToInspectedRvhMap client_host_to_inspected_rvh_;
  RenderViewHost* inspected_rvh_for_reopen_;
  bool in_initial_show_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsManager);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_
