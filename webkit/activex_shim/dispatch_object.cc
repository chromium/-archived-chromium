// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/activex_shim/dispatch_object.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "webkit/activex_shim/activex_util.h"
#include "webkit/activex_shim/npp_impl.h"

using std::string;
using std::wstring;

namespace activex_shim {

// Used when the browser asks for scriptable object.
static NPClass npclass = {
  1, // NP_CLASS_STRUCT_VERSION,
  NPAllocate,
  NPDeallocate,
  NPInvalidate,
  NPHasMethod,
  NPInvoke,
  NPInvokeDefault,
  NPHasProperty,
  NPGetProperty,
  NPSetProperty,
  NPRemoveProperty
};

DispatchObject::DispatchObject(DispatchObject* root)
    : npobject_(NULL),
      root_(root),
      deleting_spawned_children_(false) {
}

DispatchObject::~DispatchObject() {
  if (npobject_) {
    // We are gone, but the NPObject may still be there. So remove
    // the reference to myself to avoid future trouble.
    npobject_->dispatch_object = NULL;
  }
}

NPObject* DispatchObject::GetScriptableNPObject() {
  if (npobject_ == NULL) {
    npobject_ = static_cast<DispatchNPObject*>(NPAllocate(&npclass));
  } else {
    // If it is requesting the object again, we should just return the
    // object with increased reference count.
    g_browser->retainobject(npobject_);
  }
  return npobject_;
}

NPObject* DispatchObject::NPAllocate(NPClass* cls) {
  DispatchNPObject* obj = new DispatchNPObject();
  obj->_class = cls;
  obj->referenceCount = 1;
  obj->dispatch_object = this;
  return obj;
}

void DispatchObject::NPInvalidate() {
}

void DispatchObject::OnDeallocateObject(DispatchNPObject* obj) {
  DCHECK_EQ(obj, npobject_);
  if (obj == npobject_) {
    // Just null our reference so that we won't accidentally access it
    // during destruction.
    npobject_ = NULL;
    if (NPObjectOwnsMe()) {
      delete this;
    }
  }
}

bool DispatchObject::NPHasMethod(NPIdentifier name) {
  wstring wname;
  if (!NPIdentifierToWString(name, &wname))
    return false;
  return DispIsMethodOrProperty(GetDispatch(), wname.c_str(), true);
}

bool DispatchObject::NPHasProperty(NPIdentifier name) {
  wstring wname;
  if (!NPIdentifierToWString(name, &wname))
    return false;
  return DispIsMethodOrProperty(GetDispatch(), wname.c_str(), false);
  // Here is another way. But the problem is it can not distiguish between
  // method and property.
  // DISPID dispid;
  // if (GetDispID(npobj->dispatch_object->GetDispatch(), wname.c_str(), &dispid))
  //   return true;
}

bool DispatchObject::NPInvoke(NPIdentifier name, const NPVariant* args,
                             uint32_t argCount, NPVariant* result) {
  wstring wname;
  if (!NPIdentifierToWString(name, &wname))
    return false;
  std::vector<ScopedVariant> vars;
  ScopedVariant vtres;
  bool res = false;
  do {
    if (argCount > 0) {
      vars.resize(argCount);
      unsigned int i;
      for (i = 0; i < argCount; i++) {
        // Note that we need to reverse the order of arguments for
        // IDispatch::Invoke.
        if (!NPVariantToVariant(&args[argCount - i - 1], &vars[i]))
          break;
      }
      if (i < argCount)
        break;
    }
    if (!DispInvoke(GetDispatch(), wname.c_str(),
                    argCount > 0 ? &vars[0] : NULL,
                    argCount, &vtres))
      break;
    if (!VariantToNPVariant(this, &vtres, result))
      break;
    res = true;
  } while (false);
  return res;
}

bool DispatchObject::NPInvokeDefault(const NPVariant* args, uint32_t argCount,
                                    NPVariant* result) {
  return false;
}

bool DispatchObject::NPGetProperty(NPIdentifier name, NPVariant* variant) {
  wstring wname;
  if (!NPIdentifierToWString(name, &wname))
    return false;
  ScopedVariant result;
  if (!DispInvoke(GetDispatch(), wname.c_str(), NULL, 0, &result))
    return false;
  if (!VariantToNPVariant(this, &result, variant))
    return false;
  return true;
}

bool DispatchObject::NPSetProperty(NPIdentifier name, const NPVariant* variant) {
  wstring wname;
  if (!NPIdentifierToWString(name, &wname))
    return false;
  ScopedVariant rvalue;
  if (!NPVariantToVariant(variant, &rvalue))
    return false;
  if (!DispSetProperty(GetDispatch(), wname.c_str(), rvalue))
    return false;
  return true;
}

bool DispatchObject::NPRemoveProperty(NPIdentifier propertyName) {
  return false;
}

void DispatchObject::AddSpawned(DispatchObject* obj) {
  // I myself must be the root.
  DCHECK(root_ == NULL);
  spawned_children_.push_back(obj);
}

void DispatchObject::RemoveSpawned(DispatchObject* obj) {
  // This is to avoid problem when the root object is calling ReleaseSpawned to
  // delete all spawned children.
  if (deleting_spawned_children_)
    return;
  DCHECK(root_ == NULL);
  SpawnedChildrenList::iterator it = std::find(spawned_children_.begin(),
                                               spawned_children_.end(), obj);
  if (it == spawned_children_.end()) {
    DCHECK(false);
    return;
  }
  spawned_children_.erase(it);
}

void DispatchObject::ReleaseSpawned() {
  DCHECK(root_ == NULL);
  deleting_spawned_children_ = true;
  for (SpawnedChildrenList::iterator it = spawned_children_.begin();
       it != spawned_children_.end(); ++it)
    delete *it;
  deleting_spawned_children_ = false;
  spawned_children_.clear();
}

SpawnedDispatchObject::SpawnedDispatchObject(IDispatch* dispatch,
                                             DispatchObject* root)
    : DispatchObject(root),
      dispatch_(dispatch) {
  if (dispatch)
    dispatch->AddRef();
  DCHECK(root != NULL);
  root->AddSpawned(this);
}

SpawnedDispatchObject::~SpawnedDispatchObject() {
  if (dispatch_)
    dispatch_->Release();
  DCHECK(root_ != NULL);
  root_->RemoveSpawned(this);
}

///////////////////////////////////////////////////////////////////////////////
// Scripting object functions implementation.

NPObject* NPAllocate(NPP npp, NPClass* theClass) {
  DispatchObject* dispatch_object = static_cast<DispatchObject*>(npp->pdata);
  return dispatch_object->NPAllocate(theClass);
}

void NPDeallocate(NPObject* obj) {
  DispatchNPObject* npobj = static_cast<DispatchNPObject*>(obj);
  // The dispatch_object could be well gone before the NPObject is released.
  if (npobj->dispatch_object != NULL) {
    npobj->dispatch_object->OnDeallocateObject(npobj);
  }
  delete npobj;
}

void NPInvalidate(NPObject* obj) {
  DispatchNPObject* npobj = static_cast<DispatchNPObject*>(obj);
  if (npobj->dispatch_object == NULL)
    return;
  npobj->dispatch_object->NPInvalidate();
}

bool NPHasMethod(NPObject* obj, NPIdentifier name) {
  DispatchNPObject* npobj = static_cast<DispatchNPObject*>(obj);
  if (npobj->dispatch_object == NULL)
    return false;
  return npobj->dispatch_object->NPHasMethod(name);
}

bool NPInvoke(NPObject* obj, NPIdentifier name, const NPVariant* args,
              uint32_t argCount, NPVariant* result) {
  DispatchNPObject* npobj = static_cast<DispatchNPObject*>(obj);
  if (npobj->dispatch_object == NULL)
    return false;
  return npobj->dispatch_object->NPInvoke(name, args, argCount, result);
}

bool NPInvokeDefault(NPObject* obj, const NPVariant* args, uint32_t argCount,
                     NPVariant* result) {
  DispatchNPObject* npobj = static_cast<DispatchNPObject*>(obj);
  if (npobj->dispatch_object == NULL)
    return false;
  return npobj->dispatch_object->NPInvokeDefault(args, argCount, result);
}

bool NPHasProperty(NPObject* obj, NPIdentifier name) {
  DispatchNPObject* npobj = static_cast<DispatchNPObject*>(obj);
  if (npobj->dispatch_object == NULL)
    return false;
  return npobj->dispatch_object->NPHasProperty(name);
}

bool NPGetProperty(NPObject* obj, NPIdentifier name, NPVariant* variant) {
  DispatchNPObject* npobj = static_cast<DispatchNPObject*>(obj);
  if (npobj->dispatch_object == NULL)
    return false;
  return npobj->dispatch_object->NPGetProperty(name, variant);
}

bool NPSetProperty(NPObject* obj, NPIdentifier name, const NPVariant* variant) {
  DispatchNPObject* npobj = static_cast<DispatchNPObject*>(obj);
  if (npobj->dispatch_object == NULL)
    return false;
  return npobj->dispatch_object->NPSetProperty(name, variant);
}

bool NPRemoveProperty(NPObject* obj, NPIdentifier name) {
  DispatchNPObject* npobj = static_cast<DispatchNPObject*>(obj);
  if (npobj->dispatch_object == NULL)
    return false;
  return npobj->dispatch_object->NPRemoveProperty(name);
}

}  // namespace activex_shim
