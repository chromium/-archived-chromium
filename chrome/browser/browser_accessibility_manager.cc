// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_accessibility_manager.h"

#include "chrome/browser/browser_accessibility.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"

using webkit_glue::WebAccessibility;

// The time in ms after which we give up and return an error when processing an
// accessibility message and no response has been received from the renderer.
static const int kAccessibilityMessageTimeOut = 10000;

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::GetInstance() {
  return Singleton<BrowserAccessibilityManager>::get();
}

BrowserAccessibilityManager::BrowserAccessibilityManager()
    : instance_id_(0) {
  NotificationService::current()->AddObserver(this,
      NotificationType::RENDERER_PROCESS_TERMINATED,
      NotificationService::AllSources());
}

BrowserAccessibilityManager::~BrowserAccessibilityManager() {
  // Clear hashmaps.
  instance_map_.clear();
  render_process_host_map_.clear();

  // We don't remove ourselves as an observer because we are a Singleton object,
  // and NotifcationService is likely gone by this point.
}

STDMETHODIMP BrowserAccessibilityManager::CreateAccessibilityInstance(
    REFIID iid, int acc_obj_id, int instance_id, void** interface_ptr) {
  if (IID_IUnknown == iid || IID_IDispatch == iid || IID_IAccessible == iid) {
    CComObject<BrowserAccessibility>* instance = NULL;

    HRESULT hr = CComObject<BrowserAccessibility>::CreateInstance(&instance);
    DCHECK(SUCCEEDED(hr));

    if (!instance)
      return E_FAIL;

    CComPtr<IAccessible> accessibility_instance(instance);

    // Set unique ids.
    instance->set_iaccessible_id(acc_obj_id);
    instance->set_instance_id(instance_id);

    // Retrieve the RenderWidgetHost connected to this request.
    InstanceMap::iterator it = instance_map_.find(instance_id);

    if (it != instance_map_.end()) {
      UniqueMembers* members = it->second;

      if (!members || !members->render_widget_host_)
        return E_FAIL;

      render_process_host_map_[members->render_widget_host_->process()] =
          instance;
    } else {
      // No RenderProcess active for this instance.
      return E_FAIL;
    }

    // All is well, assign the temp instance to the output pointer.
    *interface_ptr = accessibility_instance.Detach();
    return S_OK;
  }
  // No supported interface found, return error.
  *interface_ptr = NULL;
  return E_NOINTERFACE;
}

bool BrowserAccessibilityManager::RequestAccessibilityInfo(
    int acc_obj_id, int instance_id, int acc_func_id, int child_id, long input1,
    long input2) {
  // Create and populate IPC message structure, for retrieval of accessibility
  // information from the renderer.
  WebAccessibility::InParams in_params;
  in_params.object_id = acc_obj_id;
  in_params.function_id = acc_func_id;
  in_params.child_id = child_id;
  in_params.input_long1 = input1;
  in_params.input_long2 = input2;

  // Retrieve the RenderWidgetHost connected to this request.
  InstanceMap::iterator it = instance_map_.find(instance_id);

  if (it == instance_map_.end()) {
    // Id not found.
    return false;
  }

  UniqueMembers* members = it->second;

  if (!members || !members->render_widget_host_)
    return false;

  bool success = false;
  if (members->render_widget_host_->process() &&
      members->render_widget_host_->process()->channel()) {
    IPC::SyncMessage* msg =
        new ViewMsg_GetAccessibilityInfo(members->render_widget_host_->
            routing_id(), in_params, &out_params_);
    // Necessary for the send to keep the UI responsive.
    msg->EnableMessagePumping();
    success = members->render_widget_host_->process()->channel()->
        SendWithTimeout(msg, kAccessibilityMessageTimeOut);
  }
  return success;
}

const WebAccessibility::OutParams& BrowserAccessibilityManager::response() {
  return out_params_;
}

HWND BrowserAccessibilityManager::parent_hwnd(int id) {
  // Retrieve the parent HWND connected to the requester's id.
  InstanceMap::iterator it = instance_map_.find(id);

  if (it == instance_map_.end()) {
    // Id not found.
    return NULL;
  }

  UniqueMembers* members = it->second;

  if (!members || !members->parent_hwnd_)
    return NULL;

  return members->parent_hwnd_;
}

int BrowserAccessibilityManager::SetMembers(BrowserAccessibility* browser_acc,
    HWND parent_hwnd, RenderWidgetHost* render_widget_host) {
  // Set HWND and RenderWidgetHost connected to |browser_acc|.
  instance_map_[instance_id_] =
      new UniqueMembers(parent_hwnd, render_widget_host);

  render_process_host_map_[render_widget_host->process()] = browser_acc;
  return instance_id_++;
}

void BrowserAccessibilityManager::Observe(NotificationType type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  DCHECK(type == NotificationType::RENDERER_PROCESS_TERMINATED);
  RenderProcessHost* rph = Source<RenderProcessHost>(source).ptr();
  DCHECK(rph);
  RenderProcessHostMap::iterator it = render_process_host_map_.find(rph);

  if (it == render_process_host_map_.end() || !it->second) {
    // RenderProcessHost not associated with any BrowserAccessibility instance.
    return;
  }

  // Set BrowserAccessibility instance to inactive state.
  it->second->set_instance_active(false);

  // Delete entry also from InstanceMap.
  InstanceMap::iterator it2 = instance_map_.find(it->second->instance_id());

  if (it2 != instance_map_.end())
    instance_map_.erase(it2);

  // Only delete the first entry once it is no longer in use.
  render_process_host_map_.erase(it);
}
