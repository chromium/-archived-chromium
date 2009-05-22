// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACCESSIBILITY_MANAGER_H_
#define CHROME_BROWSER_BROWSER_ACCESSIBILITY_MANAGER_H_

#include <map>

#include "base/singleton.h"
#include "chrome/common/notification_registrar.h"
#include "webkit/glue/webaccessibility.h"

class BrowserAccessibility;
class RenderProcessHost;
class RenderWidgetHost;

using webkit_glue::WebAccessibility;

////////////////////////////////////////////////////////////////////////////////
//
// BrowserAccessibilityManager
//
// Used to manage instance creation and memory handling for browser side
// accessibility. A singleton class. It implements NotificationObserver to
// ensure that a termination of a renderer process gets propagated to the
// active BrowserAccessibility instances calling into it. Each such instance
// will upon such an event be set to an inactive state, failing calls from the
// assistive technology gracefully.
////////////////////////////////////////////////////////////////////////////////
class BrowserAccessibilityManager : public NotificationObserver {
 public:
  // Gets the singleton BrowserAccessibilityManager object. The first time this
  // method is called, a CacheManagerHost object is constructed and returned.
  // Subsequent calls will return the same object.
  static BrowserAccessibilityManager* GetInstance();

  // Creates an instance of BrowserAccessibility, initializes it and sets the
  // |acc_obj_id|, which is used for IPC communication, and |instance_id|,
  // which is used to identify the mapping between accessibility instance and
  // RenderProcess.
  STDMETHODIMP CreateAccessibilityInstance(REFIID iid,
                                           int acc_obj_id,
                                           int routing_id,
                                           int process_id,
                                           HWND parent_hwnd,
                                           void** interface_ptr);

  // Composes and sends a message for requesting needed accessibility
  // information.
  bool RequestAccessibilityInfo(WebAccessibility::InParams* in,
                                int routing_id,
                                int process_id);

  // Notifies assistive technology that renderer focus changed, through the
  // platform-specific channels.
  bool ChangeAccessibilityFocus(int acc_obj_id, int process_id, int routing_id);

  // Wrapper function, for cleaner code.
  const WebAccessibility::OutParams& response();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  // This class is a singleton.  Do not instantiate directly.
  BrowserAccessibilityManager();
  friend struct DefaultSingletonTraits<BrowserAccessibilityManager>;

  ~BrowserAccessibilityManager();

 private:
  // Retrieves the BrowserAccessibility instance connected to the
  // RenderProcessHost identified by the process/routing id pair.
  BrowserAccessibility* GetBrowserAccessibility(int process_id, int routing_id);

  // Multi-map from process id (key) to active BrowserAccessibility instances
  // for that RenderProcessHost.
  typedef std::multimap<int, BrowserAccessibility*> RenderProcessHostMap;
  typedef std::pair<int, BrowserAccessibility*> MapEntry;

  NotificationRegistrar registrar_;

  // Mapping to track which RenderProcessHosts ids are active. If a
  // RenderProcessHost is found to be terminated, its id (key) should be removed
  // from this mapping, and the connected BrowserAccessibility ids/instances
  // invalidated.
  RenderProcessHostMap render_process_host_map_;

  // Structure passed by reference to hold response parameters from the
  // renderer.
  WebAccessibility::OutParams out_params_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManager);
};
#endif  // CHROME_BROWSER_BROWSER_ACCESSIBILITY_MANAGER_H_
