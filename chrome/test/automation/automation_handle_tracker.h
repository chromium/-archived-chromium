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

// This file defines a mapping between Automation Proxy objects and
// their associated app-side handles.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_HANDLE_TRACKER_H__
#define CHROME_TEST_AUTOMATION_AUTOMATION_HANDLE_TRACKER_H__

#include <map>

#include "base/basictypes.h"

// This represents a value that the app's AutomationProvider returns
// when asked for a resource (like a window or tab).
typedef int AutomationHandle;

class AutomationHandleTracker;
class AutomationMessageSender;

class AutomationResourceProxy {
 public:
  AutomationResourceProxy(AutomationHandleTracker* tracker,
                          AutomationMessageSender* sender,
                          AutomationHandle handle);
  virtual ~AutomationResourceProxy();

  // Marks this proxy object as no longer valid; this generally means
  // that the corresponding resource on the app side is gone.
  void Invalidate() { is_valid_ = false; }
  bool is_valid() const { return is_valid_; }

  // Returns the handle that the app has generated to refer to this resource.
  AutomationHandle handle() { return handle_; }

 protected:
  friend class AutomationHandleTracker;

  AutomationHandle handle_;

  // Called by the tracker when it is being destroyed so we know not to call
  // it back.
  void TrackerGone() {
    tracker_ = NULL;
  }

  // Not owned by us, owned by the AutomationProxy object. May be NULL if the
  // tracker has been destroyed (and hence the object is invalid).
  AutomationHandleTracker* tracker_;

  // Not owned by us.
  AutomationMessageSender* sender_;

 private:
  // True if the resource that this object is a proxy for on the app side
  // still exists.
  bool is_valid_;

  DISALLOW_EVIL_CONSTRUCTORS(AutomationResourceProxy);
};

// This class keeps track of the mapping between AutomationHandles and
// AutomationResourceProxy objects.  This is important because (1) multiple
// AutomationResourceProxy objects can be generated for the same handle
// (2) handles can be invalidated by the app, and all the associated
// proxy objects then need to be invalidated, and (3) when a handle is no
// longer being used on this end, we need to tell the app that it can
// discard the handle.
class AutomationHandleTracker {
 public:
  AutomationHandleTracker(AutomationMessageSender* sender)
      : sender_(sender) {}
  ~AutomationHandleTracker();

  // Adds the specified proxy object to the tracker.
  void Add(AutomationResourceProxy* proxy);

  // Removes a given proxy object from the mapping, and unregisters the
  // handle on the app side if this was the last proxy object that was using
  // that handle.  This is a no-op if the proxy object is not currently
  // in the tracker.
  void Remove(AutomationResourceProxy* proxy);

  // Marks all proxy objects related to a given handle invalid.  This is
  // used when a resource (like a window) on the app side is closed, meaning
  // that no further operations can be completed using the handle that
  // identified that resource.
  void InvalidateHandle(AutomationHandle handle);

 private:
  typedef
    std::multimap<AutomationHandle, AutomationResourceProxy*> HandleToObjectMap;
  typedef std::pair<AutomationHandle, AutomationResourceProxy*> MapEntry;

  HandleToObjectMap handle_to_object_;

  AutomationMessageSender* sender_;

  DISALLOW_EVIL_CONSTRUCTORS(AutomationHandleTracker);
};

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_HANDLE_TRACKER_H__