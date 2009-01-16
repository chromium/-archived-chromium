// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACCESSIBILITY_MANAGER_H_
#define CHROME_BROWSER_BROWSER_ACCESSIBILITY_MANAGER_H_

#include <oaidl.h>  // Needed for VARIANT structure.
#include <hash_map>

#include "base/singleton.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"

class BrowserAccessibility;
class RenderProcessHost;
class RenderWidgetHost;

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
  // iaccessible_id and parent_id.
  STDMETHODIMP CreateAccessibilityInstance(REFIID iid,
                                           int iaccessible_id,
                                           int instance_id,
                                           void** interface_ptr);

  // Composes and sends a message for requesting needed accessibility
  // information. Unused LONG input parameters should be NULL, and the VARIANT
  // an empty, valid instance.
  bool RequestAccessibilityInfo(int iaccessible_id,
                                int instance_id,
                                int iaccessible_func_id,
                                VARIANT var_id,
                                LONG input1,
                                LONG input2);

  // Wrapper function, for cleaner code.
  ViewHostMsg_Accessibility_Out_Params response();

  // Retrieves the parent HWND connected to the provided id.
  HWND parent_hwnd(int id);

  // Mutator, needed since constructor does not take any arguments, and to keep
  // instance accessor clean.
  int SetMembers(BrowserAccessibility* browser_acc,
                  HWND parent_hwnd,
                  RenderWidgetHost* render_widget_host);

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
  // Member variable structure, used in instance hashmap.
  struct UniqueMembers {
    RenderWidgetHost* render_widget_host_;
    HWND parent_hwnd_;

    UniqueMembers(HWND parent_hwnd, RenderWidgetHost* render_widget_host)
      : parent_hwnd_(parent_hwnd),
        render_widget_host_(render_widget_host) {
    }
  };

  typedef stdext::hash_map<int, UniqueMembers*> InstanceMap;
  typedef stdext::hash_map<RenderProcessHost*, BrowserAccessibility*>
      RenderProcessHostMap;

  // Caching of the unique member variables used to handle browser accessibility
  // requests from multiple processes.
  InstanceMap instance_map_;
  int instance_id_;

  // Reverse mapping to track which RenderProcessHosts are active. If a
  // RenderProcessHost is found to be terminated, it should be removed from this
  // mapping, and the connected BrowserAccessibility ids/instances invalidated.
  RenderProcessHostMap render_process_host_map_;

  ViewHostMsg_Accessibility_Out_Params out_params_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManager);
};
#endif  // CHROME_BROWSER_BROWSER_ACCESSIBILITY_MANAGER_H_
