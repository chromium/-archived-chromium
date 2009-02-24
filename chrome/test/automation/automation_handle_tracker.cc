// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/automation_handle_tracker.h"

#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"

AutomationResourceProxy::AutomationResourceProxy(
    AutomationHandleTracker* tracker, AutomationMessageSender* sender,
    AutomationHandle handle)
    : handle_(handle),
      tracker_(tracker),
      sender_(sender),
      is_valid_(true) {
      tracker_->Add(this);
}

AutomationResourceProxy::~AutomationResourceProxy() {
  if (tracker_)
    tracker_->Remove(this);
}

AutomationHandleTracker::~AutomationHandleTracker() {
  // Tell any live objects that the tracker is going away so they don't try to
  // call us when they are being destroyed.
  for (HandleToObjectMap::iterator iter = handle_to_object_.begin();
       iter != handle_to_object_.end(); ++iter) {
    iter->second->Invalidate();
    iter->second->TrackerGone();
  }
}

void AutomationHandleTracker::Add(AutomationResourceProxy* proxy) {
  handle_to_object_.insert(MapEntry(proxy->handle(), proxy));
}

void AutomationHandleTracker::Remove(AutomationResourceProxy* proxy) {
  HandleToObjectMap::iterator iter = handle_to_object_.find(proxy->handle());
  if (iter == handle_to_object_.end())
    return;

  HandleToObjectMap::iterator end_of_matching_objects =
    handle_to_object_.upper_bound(proxy->handle());

  while(iter != end_of_matching_objects) {
    if (iter->second == proxy) {
      handle_to_object_.erase(iter);

      // If we have no more proxy objects using this handle, tell the
      // app that it can clean up that handle.  If the proxy isn't valid,
      // that means that the app has already discarded this handle, and
      // thus doesn't need to be notified that the handle is unused.
      if (proxy->is_valid() && handle_to_object_.count(proxy->handle()) == 0) {
        sender_->Send(new AutomationMsg_HandleUnused(0, proxy->handle()));
      }
      return;
    }
    ++iter;
  }
}

void AutomationHandleTracker::InvalidateHandle(AutomationHandle handle) {
  HandleToObjectMap::iterator iter = handle_to_object_.lower_bound(handle);
  HandleToObjectMap::const_iterator end_of_matching_objects =
    handle_to_object_.upper_bound(handle);

  for (; iter != end_of_matching_objects; ++iter) {
    iter->second->Invalidate();
  }
}

