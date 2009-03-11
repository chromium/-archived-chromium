// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_ACTIVEX_SHIM_DISPATCH_OBJECT_H__
#define WEBKIT_ACTIVEX_SHIM_DISPATCH_OBJECT_H__

#include <oaidl.h>

#include <list>

#include "base/scoped_ptr.h"
#include "webkit/glue/plugins/nphostapi.h"

namespace activex_shim {

struct DispatchNPObject;

// DispatchObject provides service to translate calls on NPObject to the
// underlying IDispatch interface. It is isolated from the ActiveXPlugin, so
// that when we have scripts like:
//   wmp.controls.stop();
// We can create a spawned dispatch object for "controls" -- an IDispatch
// interface returned from property "controls" of the wmp activex control.
class DispatchObject {
 public:
  DispatchObject(DispatchObject* root);
  virtual ~DispatchObject();

  // This is used when NPP_GetValue is called by browser and asked for
  // NPPVpluginScriptableNPObject.
  NPObject* GetScriptableNPObject();

  // Object scripting
  NPObject* NPAllocate(NPClass* theClass);
  void NPInvalidate();
  bool NPHasMethod(NPIdentifier name);
  bool NPHasProperty(NPIdentifier name);
  bool NPInvoke(NPIdentifier name, const NPVariant* args,
                uint32_t argCount, NPVariant* result);
  bool NPInvokeDefault(const NPVariant* args, uint32_t argCount,
                       NPVariant* result);
  bool NPGetProperty(NPIdentifier name, NPVariant* variant);
  bool NPSetProperty(NPIdentifier name, const NPVariant* variant);
  bool NPRemoveProperty(NPIdentifier propertyName);

  // Called by NPDeallocate so that we can remove our reference.
  void OnDeallocateObject(DispatchNPObject* obj);
  DispatchObject* root() { return root_== NULL ? this : root_; }
  // Only the root object needs to take care of this.
  void AddSpawned(DispatchObject* obj);
  // If a spawned child is released earlier than the root object, it needs to
  // call this function to remove itself from the children list, to avoid
  // another destruction when the root object is being destructed.
  void RemoveSpawned(DispatchObject* obj);

 protected:
  // Only the root object can release spawned dispatch object. The root object
  // is coupled with the actual ActiveX control. Thus if the control is dead
  // we must also release all Dispatch interfaces from that control.
  void ReleaseSpawned();
  // root_ is the owner of this object. If root_ == NULL, then this object
  // itself is the root.
  DispatchObject* const root_;

 private:
  typedef std::list<DispatchObject*> SpawnedChildrenList;

  // Must be overrided by subclass to be functional.
  virtual IDispatch* GetDispatch() = 0;
  // If this is true, when the related npobject is released, it should delete
  // this object as well.
  virtual bool NPObjectOwnsMe() = 0;

  // We create only one NPObject per ActiveXPlugin. It may have different life
  // span than ActiveXPlugin thus we need a separate object created
  // specifically for this purpose.
  DispatchNPObject* npobject_;
  // A list of spawned children from this root object (if it is).
  SpawnedChildrenList spawned_children_;
  bool deleting_spawned_children_;
};

// The spawned dipatch object contains a reference to an IDispatch interface
// that it owns. Its lifetime is controlled by the lifetime of related
// NPObject, and the root DispatchObject it is spawned from.
class SpawnedDispatchObject : public DispatchObject {
 public:
  // Constructor will addref to the dispatch interface, and add itself to
  // spawned children of root.
  SpawnedDispatchObject(IDispatch* dispatch, DispatchObject* root);
  ~SpawnedDispatchObject();

 private:
  virtual IDispatch* GetDispatch() { return dispatch_; }
  virtual bool NPObjectOwnsMe() { return true; }

  IDispatch* dispatch_;
};

// A simple extension of the NPObject. So that we can put additional information
// like who is the underlying DispatchObject with the NPObject. When methods of
// NPObject are requested we can resort to the DispatchObject to handle them.
struct DispatchNPObject : public NPObject {
  DispatchObject* dispatch_object;
};

// These functions are to support scriptable plugin object.
NPObject* NPAllocate(NPP npp, NPClass* theClass);
void NPDeallocate(NPObject* obj);
void NPInvalidate(NPObject* obj);
bool NPHasMethod(NPObject* obj, NPIdentifier name);
bool NPInvoke(NPObject* obj, NPIdentifier name, const NPVariant* args,
              uint32_t argCount, NPVariant* result);
bool NPInvokeDefault(NPObject* obj, const NPVariant* args, uint32_t argCount,
                     NPVariant* result);
bool NPHasProperty(NPObject* obj, NPIdentifier name);
bool NPGetProperty(NPObject* obj, NPIdentifier name, NPVariant* variant);
bool NPSetProperty(NPObject* obj, NPIdentifier name, const NPVariant* variant);
bool NPRemoveProperty(NPObject* npobj, NPIdentifier propertyName);

}  // namespace activex_shim

#endif // #ifndef WEBKIT_ACTIVEX_SHIM_DISPATCH_OBJECT_H__
