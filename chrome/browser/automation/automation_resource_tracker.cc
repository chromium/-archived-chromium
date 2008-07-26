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

#include "chrome/browser/automation/automation_resource_tracker.h"

#include "chrome/test/automation/automation_messages.h"

int AutomationResourceTrackerImpl::AddImpl(void* resource) {
  if (ContainsResourceImpl(resource))
    return resource_to_handle_[resource];

  int handle = GenerateHandle();
  DCHECK(!ContainsHandleImpl(handle));

  resource_to_handle_[resource] = handle;
  handle_to_resource_[handle] = resource;

  AddObserverTypeProxy(resource);

  return handle;
}

void AutomationResourceTrackerImpl::RemoveImpl(void* resource) {
  if (!ContainsResourceImpl(resource))
    return;

  int handle = resource_to_handle_[resource];
  DCHECK(handle_to_resource_[handle] == resource);

  RemoveObserverTypeProxy(resource);

  resource_to_handle_.erase(resource);
  handle_to_resource_.erase(handle);
}

int AutomationResourceTrackerImpl::GenerateHandle() {
  static int handle = 0;
  return ++handle;
}

bool AutomationResourceTrackerImpl::ContainsResourceImpl(void* resource) {
  return resource_to_handle_.find(resource) != resource_to_handle_.end();
}

bool AutomationResourceTrackerImpl::ContainsHandleImpl(int handle) {
  return handle_to_resource_.find(handle) != handle_to_resource_.end();
}

void AutomationResourceTrackerImpl::ClearAllMappingsImpl() {
  while (!resource_to_handle_.empty()) {
    RemoveImpl(resource_to_handle_.begin()->first);
  }
  cleared_mappings_ = true;
}

void* AutomationResourceTrackerImpl::GetResourceImpl(int handle) {
  HandleToResourceMap::const_iterator iter = handle_to_resource_.find(handle);
  if (iter == handle_to_resource_.end())
    return NULL;

  return iter->second;
}

int AutomationResourceTrackerImpl::GetHandleImpl(void* resource) {
  ResourceToHandleMap::const_iterator iter =
    resource_to_handle_.find(resource);
  if (iter == resource_to_handle_.end())
    return 0;

  return iter->second;
}

void AutomationResourceTrackerImpl::HandleCloseNotification(void* resource) {
  if (!ContainsResourceImpl(resource))
    return;

  sender_->Send(
    new AutomationMsg_InvalidateHandle(0, resource_to_handle_[resource]));

  RemoveImpl(resource);
}
