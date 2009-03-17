// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/common/notification_service.h"

namespace IPC {
class Message;
}

class DevToolsInstanceDescriptor;
class DevToolsInstanceDescriptorImpl;
class DevToolsWindow;
class DevToolsWindowFactory;
class NavigationController;
class NotificationRegistrar;
class RenderViewHost;
class WebContents;

// This class is a singleton that manages DevToolsWindow instances and routes
// messages between developer tools clients and agents.
class DevToolsManager : public NotificationObserver {
 public:
  // If the factory is NULL, this will create DevToolsWindow objects using
  // DevToolsWindow::Create static method.
  DevToolsManager(DevToolsWindowFactory* factory);
  virtual ~DevToolsManager();

  // Opend developer tools window for |web_contents|. If there is already
  // one it will be revealed otherwise a new instance will be created. The
  // devtools window is actually opened for the tab owning |web_contents|. If
  // navigation occurs in it causing change of contents in the tab the devtools
  // window will attach to the new contents. When the tab is closed the manager
  // will close related devtools window.
  void ShowDevToolsForWebContents(WebContents* web_contents);

  void ForwardToDevToolsAgent(RenderViewHost* from,
                              const IPC::Message& message);
  void ForwardToDevToolsClient(RenderViewHost* from,
                               const IPC::Message& message);

 private:
  // Creates a DevToolsWindow using devtools_window_factory_ or by calling
  // DevToolsWindow::Create, if the factory is NULL. All DevToolsWindows should
  // be created by means of this method.
  DevToolsWindow* CreateDevToolsWindow(DevToolsInstanceDescriptor* descriptor);

  // NotificationObserver override.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  friend class DevToolsInstanceDescriptorImpl;

  // This method is called by DevToolsInstanceDescriptorImpl when it's about
  // to be destroyed. It will remove all references from the manager to the
  // descriptor and unregister all listeners related to the descriptor.
  void RemoveDescriptor(DevToolsInstanceDescriptorImpl* descriptor);

  void StartListening(NavigationController* navigation_controller);
  void StopListening(NavigationController* navigation_controller);

  // This object is not NULL iff there is at least one open DevToolsWindow.
  scoped_ptr<NotificationRegistrar> web_contents_listeners_;

  // This maps is for tracking devtools instances opened for browser tabs. It
  // allows us to have at most one devtools window per tab.
  // We use NavigationController* as key because it survives crosee-site
  // navigation in cases when tab contents may change.
  //
  // This map doesn't own its values but DevToolsInstanceDescriptorImpl is
  // expected to call RemoveDescriptor before dying to remove itself from the
  // map.
  typedef std::map<NavigationController*,
                   DevToolsInstanceDescriptorImpl*> DescriptorMap;
  DescriptorMap navcontroller_to_descriptor_;
  DevToolsWindowFactory* devtools_window_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsManager);
};


// Incapsulates information about devtools window instance necessary for
// routing devtools messages and managing the window. It should be initialized
// by DevToolsWindow concrete implementation.
class DevToolsInstanceDescriptor {
 public:
  DevToolsInstanceDescriptor() {}

  virtual void SetDevToolsHost(RenderViewHost* render_view_host) = 0;
  virtual void SetDevToolsWindow(DevToolsWindow* window) = 0;

  // This method is called when DevToolsWindow is closing and the descriptor
  // becomes invalid. It will clean up DevToolsManager and delete this instance.
  virtual void Destroy() = 0;

 protected:
  // This method should be called from Destroy only.
  virtual ~DevToolsInstanceDescriptor() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsInstanceDescriptor);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_
