// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_RESOURCE_TRACKER_H__
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_RESOURCE_TRACKER_H__

#include <map>

#include "base/basictypes.h"
#include "chrome/common/notification_service.h"
#include "ipc/ipc_message.h"

// Template trick so that AutomationResourceTracker can be used with non-pointer
// types.
template <class T>
struct AutomationResourceTraits {
  typedef T ValueType;
};

template <class T>
struct AutomationResourceTraits<T*> {
  typedef T ValueType;
};

// This class exists for the sole purpose of allowing some of the implementation
// of AutomationResourceTracker to live in a .cc file.
class AutomationResourceTrackerImpl {
public:
  AutomationResourceTrackerImpl(IPC::Message::Sender* sender)
    : cleared_mappings_(false), sender_(sender) {}

  virtual ~AutomationResourceTrackerImpl() {}

  // These need to be implemented in AutomationResourceTracker,
  // since it needs to call the subclass's type-specific notification
  // registration functions.
  virtual void AddObserverTypeProxy(void* resource) = 0;
  virtual void RemoveObserverTypeProxy(void* resource) = 0;

  int AddImpl(void* resource);
  void RemoveImpl(void* resource);
  int GenerateHandle();
  bool ContainsResourceImpl(void* resource);
  bool ContainsHandleImpl(int handle);
  void ClearAllMappingsImpl();
  void* GetResourceImpl(int handle);
  int GetHandleImpl(void* resource);
  void HandleCloseNotification(void* resource);

protected:
  bool cleared_mappings_;
  typedef std::map<void*, int> ResourceToHandleMap;
  typedef std::map<int, void*> HandleToResourceMap;
  ResourceToHandleMap resource_to_handle_;
  HandleToResourceMap handle_to_resource_;

private:
  DISALLOW_EVIL_CONSTRUCTORS(AutomationResourceTrackerImpl);

  IPC::Message::Sender* sender_;
};

// This template defines a superclass for an object that wants to track
// a particular kind of application resource (like windows or tabs) for
// automation purposes.  The only things that a subclass should need to
// define are AddObserver and RemoveObserver for the given resource's
// close notifications, ***and a destructor that calls ClearAllMappings***.
template <class T>
class AutomationResourceTracker : public NotificationObserver,
                                  private AutomationResourceTrackerImpl {
 public:
  AutomationResourceTracker(IPC::Message::Sender* automation)
    : AutomationResourceTrackerImpl(automation) {}

  virtual ~AutomationResourceTracker() {
    // NOTE: Be sure to call ClearAllMappings() from the destructor of your
    //       subclass!  It can't be called here, because it eventually uses
    //       the subclass's RemoveObserver, which no longer exists by the time
    //       this base class destructor is executed.
    DCHECK(cleared_mappings_);
  }

  // Removes all mappings from this tracker, including unregistering from
  // any associated resource notifications (via Remove calling RemoveObserver).
  void ClearAllMappings() {
    ClearAllMappingsImpl();
  }

  // The implementations for these should call the NotificationService
  // to add and remove this object as an observer for the appropriate
  // resource closing notification.
  virtual void AddObserver(T resource) = 0;
  virtual void RemoveObserver(T resource) = 0;

  // Adds the given resource to this tracker, and returns a handle that
  // can be used to refer to that resource.  If the resource is already
  // being tracked, the handle may be the same as one returned previously.
  int Add(T resource) {
    return AddImpl(resource);
  }

  // Removes the given resource from this tracker.  If the resource is not
  // currently present in the tracker, this is a no-op.
  void Remove(T resource) {
    RemoveImpl(resource);
  }

  // Returns true if this tracker currently tracks the resource pointed to
  // by the parameter.
  bool ContainsResource(T resource) {
    return ContainsResourceImpl(resource);
  }

  // Returns true if this tracker currently tracks the given handle.
  bool ContainsHandle(int handle) {
    return ContainsHandleImpl(handle);
  }

  // Returns the resource pointer associated with a given handle, or NULL
  // if that handle is not present in the mapping.
  T GetResource(int handle) {
    return static_cast<T>(GetResourceImpl(handle));
  }

  // Returns the handle associated with a given resource pointer, or 0 if
  // the resource is not currently in the mapping.
  int GetHandle(T resource) {
    return GetHandleImpl(resource);
  }

  // NotificationObserver implementation--the only thing that this tracker
  // does in response to notifications is to tell the AutomationProxy
  // that the associated handle is now invalid.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details){
     T resource =
        Source<typename AutomationResourceTraits<T>::ValueType>(source).ptr();

     HandleCloseNotification(resource);
  }
 private:
   // These proxy calls from the base Impl class to the template's subclss.
   virtual void AddObserverTypeProxy(void* resource) {
     AddObserver(static_cast<T>(resource));
   }
   virtual void RemoveObserverTypeProxy(void* resource) {
     RemoveObserver(static_cast<T>(resource));
   }

  DISALLOW_EVIL_CONSTRUCTORS(AutomationResourceTracker);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_RESOURCE_TRACKER_H__
