// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper functions that are used by the NPObject proxy and stub.

#ifndef CHROME_PLUGIN_NPOBJECT_UTIL_H__
#define CHROME_PLUGIN_NPOBJECT_UTIL_H__

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "chrome/plugin/npobject_stub.h"

struct _NPVariant;
typedef _NPVariant NPVariant;
class NPObjectProxy;
class PluginChannelBase;
struct NPIdentifier_Param;
struct NPVariant_Param;
typedef void *NPIdentifier;

namespace base {
class WaitableEvent;
}

// Needs to be called early in the plugin process lifetime, before any
// plugin instances are initialized.
void PatchNPNFunctions();

// Returns true if the current process is a plugin process, or false if it's a
// renderer process.
bool IsPluginProcess();

// Creates an object similar to NPIdentifier that can be marshalled.
void CreateNPIdentifierParam(NPIdentifier id, NPIdentifier_Param* param);

// Creates an NPIdentifier from the marshalled object.
NPIdentifier CreateNPIdentifier(const  NPIdentifier_Param& param);

// Creates an object similar to NPVariant that can be marshalled.
// If the containing NPObject happens to be an NPObject, then a stub
// is created around it and param holds the routing id for it.
// If release is true, the NPVariant object is released (except if
// it contains an NPObject, since the stub will manage its lifetime).
void CreateNPVariantParam(const NPVariant& variant,
                          PluginChannelBase* channel,
                          NPVariant_Param* param,
                          bool release,
                          base::WaitableEvent* modal_dialog_event);

// Creates an NPVariant from the marshalled object.
void CreateNPVariant(const NPVariant_Param& param,
                     PluginChannelBase* channel,
                     NPVariant* result,
                     base::WaitableEvent* modal_dialog_event);

#if defined(OS_WIN)
// Given a plugin's HWND, returns an event associated with the TabContents
// that's set when inside a messagebox.  This tells the plugin process that
// the message queue should be pumped (as what would happen if everything was
// in-process).  This avoids deadlocks when a plugin invokes javascript that
// causes a message box to come up.
HANDLE GetMessageBoxEvent(HWND hwnd);
#endif  // defined(OS_WIN)

#endif  // CHROME_PLUGIN_NPOBJECT_UTIL_H__
