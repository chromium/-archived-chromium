// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

#include "config.h"

#include "bindings/npruntime.h"
#include "npruntime_impl.h"
#include "npruntime_priv.h"

// This file is a dumping ground for KJS-related fixups that we need.  It
// should ideally be eliminated in favor of less lameness.

// KJS should only expose functions declared in npruntime.h (NPN_*)
// and npruntime_priv.h (which is an extension of npruntime.h), and
// not exposing _NPN_* functions declared in npruntime_impl.h.
// KJSBridge implements NPN_* functions by wrapping around _NPN_* functions.
// 
// Following styles in JavaScriptCore/bindings/npruntime.cpp
void NPN_ReleaseVariantValue(NPVariant *variant) {
  _NPN_ReleaseVariantValue(variant);
}

NPIdentifier NPN_GetStringIdentifier(const NPUTF8 *name) {
  return _NPN_GetStringIdentifier(name);
}

void NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount,
                              NPIdentifier *identifiers) {
  _NPN_GetStringIdentifiers(names, nameCount, identifiers);
}

NPIdentifier NPN_GetIntIdentifier(int32_t intid) {
  return _NPN_GetIntIdentifier(intid);
}

bool NPN_IdentifierIsString(NPIdentifier identifier) {
  return _NPN_IdentifierIsString(identifier);
}

NPUTF8 *NPN_UTF8FromIdentifier(NPIdentifier identifier) {
  return _NPN_UTF8FromIdentifier(identifier);
}

int32_t NPN_IntFromIdentifier(NPIdentifier identifier) {
  return _NPN_IntFromIdentifier(identifier);
}

NPObject *NPN_CreateObject(NPP npp, NPClass *aClass) {
  return _NPN_CreateObject(npp, aClass);
}

NPObject *NPN_RetainObject(NPObject *obj) {
  return _NPN_RetainObject(obj);
}

void NPN_ReleaseObject(NPObject *obj) {
  _NPN_ReleaseObject(obj);
}

void NPN_DeallocateObject(NPObject *obj) {
  _NPN_DeallocateObject(obj);
}

bool NPN_Invoke(NPP npp, NPObject *npobj, NPIdentifier methodName,
                const NPVariant *args, uint32_t argCount, NPVariant *result) {
  return _NPN_Invoke(npp, npobj, methodName, args, argCount, result);
}

bool NPN_InvokeDefault(NPP npp, NPObject *npobj, const NPVariant *args,
                       uint32_t argCount, NPVariant *result) {
  return _NPN_InvokeDefault(npp, npobj, args, argCount, result);
}

bool NPN_Evaluate(NPP npp, NPObject *npobj, NPString *script, NPVariant *result) {
  return _NPN_Evaluate(npp, npobj, script, result);
}

bool NPN_EvaluateHelper(NPP npp, bool popups_allowed, NPObject* npobj,
                        NPString* npscript, NPVariant *result) {
  return _NPN_Evaluate(npp, npobj, script, result);
}

bool NPN_GetProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName,
                     NPVariant *result) {
  return _NPN_GetProperty(npp, npobj, propertyName, result);
}

bool NPN_SetProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName,
                     const NPVariant *value) {
  return _NPN_SetProperty(npp, npobj, propertyName, value);
}

bool NPN_RemoveProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName) {
  return _NPN_RemoveProperty(npp, npobj, propertyName);
}

bool NPN_HasProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName) {
  return _NPN_HasProperty(npp, npobj, propertyName);
}

bool NPN_HasMethod(NPP npp, NPObject *npobj, NPIdentifier methodName) {
  return _NPN_HasMethod(npp, npobj, methodName);
}

void NPN_SetException(NPObject *obj, const NPUTF8 *message) {
  _NPN_SetException(obj, message);
}

bool NPN_Enumerate(NPP npp, NPObject *npobj, NPIdentifier **identifier,
                   uint32_t *count) {
  return _NPN_Enumerate(npp, npobj, identifier, count);
}
