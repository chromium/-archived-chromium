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
//
// Helper functions that are used by the NPObject proxy and stub.

#ifndef CHROME_PLUGIN_NPOBJECT_UTIL_H__
#define CHROME_PLUGIN_NPOBJECT_UTIL_H__

#include <windows.h>
#include "chrome/plugin/npobject_stub.h"

struct _NPVariant;
typedef _NPVariant NPVariant;
class NPObjectProxy;
class PluginChannelBase;
struct NPIdentifier_Param;
struct NPVariant_Param;
typedef void *NPIdentifier;


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
                          bool release);

// Creates an NPVariant from the marshalled object.
void CreateNPVariant(const NPVariant_Param& param,
                     PluginChannelBase* channel,
                     NPVariant* result,
                     HANDLE modal_dialog_event);

// Given a plugin's HWND, returns an event associated with the WebContents
// that's set when inside a messagebox.  This tells the plugin process that
// the message queue should be pumped (as what would happen if everything was
// in-process).  This avoids deadlocks when a plugin invokes javascript that
// causes a message box to come up.
HANDLE GetMessageBoxEvent(HWND hwnd);

#endif  // CHROME_PLUGIN_NPOBJECT_UTIL_H__
