// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_accessibility_manager.h"

#include "chrome/browser/browser_accessibility.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
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

BrowserAccessibilityManager::BrowserAccessibilityManager() {
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllSources());
}

BrowserAccessibilityManager::~BrowserAccessibilityManager() {
  // Clear hashmap.
  render_process_host_map_.clear();
}

STDMETHODIMP BrowserAccessibilityManager::CreateAccessibilityInstance(
    REFIID iid, int acc_obj_id, int routing_id, int process_id,
    HWND parent_hwnd, void** interface_ptr) {
  if (IID_IUnknown == iid || IID_IDispatch == iid || IID_IAccessible == iid) {
    CComObject<BrowserAccessibility>* instance = NULL;

    HRESULT hr = CComObject<BrowserAccessibility>::CreateInstance(&instance);
    DCHECK(SUCCEEDED(hr));

    if (!instance)
      return E_FAIL;

    CComPtr<IAccessible> accessibility_instance(instance);

    // Set class member variables.
    instance->Initialize(acc_obj_id, routing_id, process_id, parent_hwnd);

    // Retrieve the RenderViewHost connected to this request.
    RenderViewHost* rvh = RenderViewHost::FromID(process_id, routing_id);

    // Update cache with RenderProcessHost/BrowserAccessibility pair.
    if (rvh && rvh->process()) {
      render_process_host_map_.insert(
          MapEntry(rvh->process()->pid(), instance));
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
    WebAccessibility::InParams* in, int routing_id, int process_id) {
  // Create and populate IPC message structure, for retrieval of accessibility
  // information from the renderer.
  WebAccessibility::InParams in_params;
  in_params.object_id = in->object_id;
  in_params.function_id = in->function_id;
  in_params.child_id = in->child_id;
  in_params.input_long1 = in->input_long1;
  in_params.input_long2 = in->input_long2;

  // Retrieve the RenderViewHost connected to this request.
  RenderViewHost* rvh = RenderViewHost::FromID(process_id, routing_id);

  // Send accessibility information retrieval message to the renderer.
  bool success = false;
  if (rvh && rvh->process() && rvh->process()->HasConnection()) {
    IPC::SyncMessage* msg =
        new ViewMsg_GetAccessibilityInfo(routing_id, in_params, &out_params_);
    // Necessary for the send to keep the UI responsive.
    msg->EnableMessagePumping();
    success = rvh->process()->SendWithTimeout(msg,
        kAccessibilityMessageTimeOut);
  }
  return success;
}

bool BrowserAccessibilityManager::ChangeAccessibilityFocus(int acc_obj_id,
                                                           int process_id,
                                                           int routing_id) {
  BrowserAccessibility* browser_acc =
      GetBrowserAccessibility(process_id, routing_id);
  if (browser_acc) {
    // Notify Access Technology that there was a change in keyboard focus.
    ::NotifyWinEvent(EVENT_OBJECT_FOCUS, browser_acc->parent_hwnd(),
                     OBJID_CLIENT, static_cast<LONG>(acc_obj_id));
    return true;
  }
  return false;
}

const WebAccessibility::OutParams& BrowserAccessibilityManager::response() {
  return out_params_;
}

BrowserAccessibility* BrowserAccessibilityManager::GetBrowserAccessibility(
    int process_id, int routing_id) {
  // Retrieve the BrowserAccessibility connected to the requester's id. There
  // could be multiple BrowserAccessibility connected to the given |process_id|,
  // but they all have the same parent HWND, so using the first hit is fine.
  RenderProcessHostMap::iterator it =
      render_process_host_map_.lower_bound(process_id);

  RenderProcessHostMap::iterator end_of_matching_objects =
    render_process_host_map_.upper_bound(process_id);

  for (; it != end_of_matching_objects; ++it) {
    if (it->second && it->second->routing_id() == routing_id)
      return it->second;
  }
  return NULL;
}

void BrowserAccessibilityManager::Observe(NotificationType type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  DCHECK(type == NotificationType::RENDERER_PROCESS_TERMINATED);
  RenderProcessHost* rph = Source<RenderProcessHost>(source).ptr();
  DCHECK(rph);

  RenderProcessHostMap::iterator it =
      render_process_host_map_.lower_bound(rph->pid());

  RenderProcessHostMap::iterator end_of_matching_objects =
    render_process_host_map_.upper_bound(rph->pid());

  for (; it != end_of_matching_objects; ++it) {
    if (it->second) {
      // Set all matching BrowserAccessibility instances to inactive state.
      // TODO(klink): Do more active memory cleanup as well.
      it->second->set_instance_active(false);
    }
  }
}
