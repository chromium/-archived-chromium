// Copyright 2009, Google Inc.  All rights reserved.
// Portions of this file were adapted from the Mozilla project.
// See https://developer.mozilla.org/en/ActiveX_Control_for_Hosting_Netscape_Plug-ins_in_IE
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@eircom.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


// File declaring NPBrowserProxy class providing a subset of the NPAPI browser
// entry points for hosting Mozilla NPAPI plugin objects.

#ifndef O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_NP_BROWSER_PROXY_H_
#define O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_NP_BROWSER_PROXY_H_

#include <dispex.h>
#include <map>
#include "third_party/npapi/files/include/npupp.h"
#include "plugin/npapi_host_control/win/dispatch_proxy.h"
#include "plugin/npapi_host_control/win/np_object_proxy.h"

class CHostControl;
class DispatchProxy;

// Class implementing the NPAPI browser interface for an ActiveX environment.
class NPBrowserProxy {
 public:
  explicit NPBrowserProxy(CHostControl* host, IDispatchEx* window_dispatch);
  ~NPBrowserProxy();

  // Returns the 'v-table' object for interacting with the NPAPI interface
  // of the hosted browser environment.
  static NPNetscapeFuncs* GetBrowserFunctions();

  // Returns the hosting COM control.
  CHostControl* GetHostingControl() {
    return host_control_;
  }

  // Returns a place-holder object for the browser property.  Used in
  // conjunction with NPN_GetValue and NPNVWindowNPObject.
  DispatchProxy* GetVWindowObject() {
    return vwindow_object_;
  }

  // Create or get the existing COM object for the given NPObject. Ensures
  // each NPObject only has a single proxy.
  CComPtr<IDispatchEx> GetDispatchObject(NPObject* np_object);

  // Create or get the existing NPObject for the given COM object. Ensures
  // each COM object only has a single proxy. Caller must release the object.
  NPObject* GetNPObject(IDispatch* dispatch_object);

  // Registers an NPObject with its associated INPObjectProxy.
  void RegisterNPObjectProxy(
    NPObject* np_object,
    const CComPtr<INPObjectProxy>& proxy_wrapper);

  // Called by the NPObjectProxy when it is destroyed.
  void UnregisterNPObjectProxy(NPObject* np_object);

  // Called by the DispatchProxy when it is destroyed.
  void UnregisterDispatchProxy(IDispatchEx* dispatch_object);

  NPIdentifier call_identifier() const { return call_identifier_; }

  // Prepares all allocated resources for the destruction of the NPBrowserProxy
  // instance.  Ensures that all objects returned to the IE scripting
  // environment become unaccessable.
  void TearDown();

 private:
  typedef std::map<NPObject*, CComPtr<INPObjectProxy> > NPObjectProxyMap;

  typedef std::map<IUnknown*, DispatchProxy*> DispatchProxyMap;

  // Back-pointer to the COM control hosting the NPAPI plug-in.
  CHostControl* host_control_;

  // Pointer to place-holder object for the NPNVWindowNPObject value
  // accessible through NPN_GetValue.
  DispatchProxy* vwindow_object_;

  // Map of all NPObjects wrapped with NPObjectProxys.
  NPObjectProxyMap np_object_proxy_map_;

  // Map of all IDispatchEx objects wrapped with DispatchProxies.
  DispatchProxyMap dispatch_proxy_map_;

  NPIdentifier call_identifier_;

  // The following functions implement a sub-set of the NPAPI browser object
  // interface.  The function naming has been preserved from that in the
  // NPAPI interface headers.  For documentation on the expected behaviour
  // of these routines, please refer to the following:
  // http://developer.mozilla.org/en/Gecko_Plugin_API_Reference/Plug-in_Side_Plug-in_API
  static NPError NPN_RequestRead(NPStream *pstream, NPByteRange *rangeList);

  static NPError NPN_GetURLNotify(NPP npp,
                                  const char* relativeURL,
                                  const char* target,
                                  void* notifyData);

  static NPError NPN_GetValue(NPP npp, NPNVariable variable, void *r_value);

  static NPError NPN_SetValue(NPP npp, NPPVariable variable, void *r_value);

  static NPError NPN_GetURL(NPP npp,
                            const char* relativeURL,
                            const char* target);

  static NPError NPN_PostURLNotify(NPP npp,
                                   const char* relativeURL,
                                   const char *target,
                                   uint32 len,
                                   const char *buf,
                                   NPBool file,
                                   void* notifyData);

  static NPError NPN_PostURL(NPP npp,
                             const char* relativeURL,
                             const char *target,
                             uint32 len,
                             const char *buf,
                             NPBool file);

  static NPError NPN_NewStream(NPP npp,
                               NPMIMEType type,
                               const char* window,
                               NPStream **pstream);

  static int32 NPN_Write(NPP npp, NPStream *pstream, int32 len, void *buffer);

  static NPError NPN_DestroyStream(NPP npp, NPStream *pstream, NPError reason);

  static void NPN_Status(NPP npp, const char *message);

  static void* NPN_MemAlloc(uint32 size);

  static void NPN_MemFree(void *ptr);

  static uint32 NPN_MemFlush(uint32 size);

  static void NPN_ReloadPlugins(NPBool reloadPages);

  static void NPN_InvalidateRect(NPP npp, NPRect *invalidRect);

  static void NPN_InvalidateRegion(NPP npp, NPRegion invalidRegion);

  static const char* NPN_UserAgent(NPP npp);

  static void* NPN_GetJavaEnv(void);

  static void* NPN_GetJavaPeer(NPP npp);

  static void* NPN_GetJavaClass(void* handle);

  static void NPN_ForceRedraw(NPP npp);

  static NPObject* NPN_CreateObject(NPP npp, NPClass *aClass);

  static NPObject* NPN_RetainObject(NPObject *object);

  static void NPN_ReleaseObject(NPObject *object);

  static NPIdentifier NPN_GetStringIdentifier(const NPUTF8 *name);

  static void NPN_GetStringIdentifiers(const NPUTF8 **names,
                                       int32_t nameCount,
                                       NPIdentifier *identifiers);

  static NPUTF8* NPN_UTF8FromIdentifier(NPIdentifier identifier);

  static NPIdentifier NPN_GetIntIdentifier(int32_t intid);
  static int32_t NPN_IntFromIdentifier(NPIdentifier identifier);
  static bool NPN_IdentifierIsString(NPIdentifier identifier);

  static void NPN_ReleaseVariantValue(NPVariant *variant);

  static bool NPN_GetProperty(NPP npp, NPObject *obj,
                              NPIdentifier propertyName,
                              NPVariant *result);

  static bool NPN_SetProperty(NPP npp,
                              NPObject *obj,
                              NPIdentifier propertyName,
                              const NPVariant *value);

  static bool NPN_HasProperty(NPP npp,
                              NPObject *npobj,
                              NPIdentifier propertyName);

  static bool NPN_RemoveProperty(NPP npp,
                                 NPObject *npobj,
                                 NPIdentifier propertyName);

  static bool NPN_HasMethod(NPP npp,
                            NPObject *npobj,
                            NPIdentifier methodName);

  static bool NPN_Invoke(NPP npp,
                         NPObject *obj,
                         NPIdentifier methodName,
                         const NPVariant *args,
                         unsigned argCount,
                         NPVariant *result);

  static bool NPN_InvokeDefault(NPP npp,
                                NPObject *obj,
                                const NPVariant *args,
                                unsigned argCount,
                                NPVariant *result);

  static bool NPN_Construct(NPP npp,
                            NPObject *obj,
                            const NPVariant *args,
                            unsigned argCount,
                            NPVariant *result);

  static bool NPN_Enumerate(NPP npp,
                            NPObject* obj,
                            NPIdentifier** ids,
                            uint32_t* idCOunt);

  static bool ConstructObject(NPP npp,
                              NPObject* window_object,
                              NPUTF8* constructor_name,
                              NPVariant* args,
                              uint32_t numArgs,
                              NPObject** result);

  static bool NPN_Evaluate(NPP npp,
                           NPObject *obj,
                           NPString *script,
                           NPVariant *result);

  static void NPN_SetException(NPObject *obj, const NPUTF8 *message);

  // Static table of function pointers to the member function entry points
  // for the NPAPI browser environment interface.
  static NPNetscapeFuncs kNetscapeFunctions;

  DISALLOW_COPY_AND_ASSIGN(NPBrowserProxy);
};

#endif  // O3D_PLUGIN_NPAPI_HOST_CONTROL_WIN_NP_BROWSER_PROXY_H_
