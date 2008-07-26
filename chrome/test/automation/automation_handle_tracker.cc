// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/test/automation/automation_handle_tracker.h"

#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"

AutomationResourceProxy::AutomationResourceProxy(
    AutomationHandleTracker* tracker, AutomationMessageSender* sender,
    AutomationHandle handle)
    : tracker_(tracker),
      sender_(sender),
      handle_(handle),
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
