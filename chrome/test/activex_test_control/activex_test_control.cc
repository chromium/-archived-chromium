// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>

#include "activex_test_control.h"
#include "activex_test_control_i.c"
#include "chrome/test/activex_test_control/resource.h"

class ActiveXTestControllModule
    : public CAtlDllModuleT<ActiveXTestControllModule> {
 public:
  DECLARE_LIBID(LIBID_activex_test_controlLib)
  DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ACTIVEX_TEST_CONTROL,
                                    "{CDBC0D94-AFF6-4918-90A9-7967179A77D8}")
};

ActiveXTestControllModule g_atlmodule;

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason,
                               LPVOID reserved) {
  return g_atlmodule.DllMain(reason, reserved);
}

// Used to determine whether the DLL can be unloaded by OLE
STDAPI DllCanUnloadNow(void) {
  return g_atlmodule.DllCanUnloadNow();
}

// Returns a class factory to create an object of the requested type
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
  return g_atlmodule.DllGetClassObject(rclsid, riid, ppv);
}

// DllRegisterServer - Adds entries to the system registry
STDAPI DllRegisterServer(void) {
  // registers object, typelib and all interfaces in typelib
  HRESULT hr = g_atlmodule.DllRegisterServer();
  return hr;
}

// DllUnregisterServer - Removes entries from the system registry
STDAPI DllUnregisterServer(void) {
  HRESULT hr = g_atlmodule.DllUnregisterServer();
  return hr;
}
