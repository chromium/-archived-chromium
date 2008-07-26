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

#include <windows.h>
#include <atlbase.h>

#pragma comment(lib, "wbemuuid.lib")

#include "base/wmi_util.h"

bool WMIUtil::CreateLocalConnection(bool set_blanket,
                                    IWbemServices** wmi_services) {
  CComPtr<IWbemLocator> wmi_locator;
  HRESULT hr = wmi_locator.CoCreateInstance(CLSID_WbemLocator, NULL,
                                            CLSCTX_INPROC_SERVER);
  if (FAILED(hr))
    return false;

  CComPtr<IWbemServices> wmi_services_r;
  hr = wmi_locator->ConnectServer(CComBSTR(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL,
                                  0, 0, &wmi_services_r);
  if (FAILED(hr))
    return false;

  if (set_blanket) {
    hr = ::CoSetProxyBlanket(wmi_services_r,
                             RPC_C_AUTHN_WINNT,
                             RPC_C_AUTHZ_NONE,
                             NULL,
                             RPC_C_AUTHN_LEVEL_CALL,
                             RPC_C_IMP_LEVEL_IMPERSONATE,
                             NULL,
                             EOAC_NONE);
    if (FAILED(hr))
      return false;
  }

  *wmi_services = wmi_services_r.Detach();
  return true;
}

bool WMIUtil::CreateClassMethodObject(IWbemServices* wmi_services,
                                      const std::wstring& class_name,
                                      const std::wstring& method_name,
                                      IWbemClassObject** class_instance) {
  // We attempt to instantiate a COM object that represents a WMI object plus
  // a method rolled into one entity.
  CComBSTR b_class_name(class_name.c_str());
  CComBSTR b_method_name(method_name.c_str());
  CComPtr<IWbemClassObject> class_object = NULL;
  HRESULT hr;
  hr = wmi_services->GetObject(b_class_name, 0, NULL, &class_object, NULL);
  if (FAILED(hr))
    return false;

  CComPtr<IWbemClassObject> params_def = NULL;
  hr = class_object->GetMethod(b_method_name, 0, &params_def, NULL);
  if (FAILED(hr))
    return false;

  if (NULL == params_def) {
    // You hit this special case if the WMI class is not a CIM class. MSDN
    // sometimes tells you this. Welcome to WMI hell.
    return false;
  }

  hr = params_def->SpawnInstance(0, class_instance);
  return(SUCCEEDED(hr));
}

bool SetParameter(IWbemClassObject* class_method,
                  const std::wstring& parameter_name, VARIANT* parameter) {
  HRESULT hr = class_method->Put(parameter_name.c_str(), 0, parameter, 0);
  return SUCCEEDED(hr);
}


// The code in Launch() basically calls the Create Method of the Win32_Process
// CIM class is documented here:
// http://msdn2.microsoft.com/en-us/library/aa389388(VS.85).aspx

bool WMIProcessUtil::Launch(const std::wstring& command_line, int* process_id) {
  CComPtr<IWbemServices> wmi_local;
  if (!WMIUtil::CreateLocalConnection(true, &wmi_local))
    return false;

  const wchar_t class_name[] = L"Win32_Process";
  const wchar_t method_name[] = L"Create";
  CComPtr<IWbemClassObject> process_create;
  if (!WMIUtil::CreateClassMethodObject(wmi_local, class_name, method_name,
                                        &process_create))
    return false;

  CComVariant b_command_line(command_line.c_str());
  if (!SetParameter(process_create, L"CommandLine", &b_command_line))
    return false;

  CComPtr<IWbemClassObject> out_params;
  HRESULT hr = wmi_local->ExecMethod(CComBSTR(class_name),
                                     CComBSTR(method_name), 0, NULL,
                                     process_create, &out_params, NULL);
  if (FAILED(hr))
    return false;

  CComVariant ret_value;
  hr = out_params->Get(L"ReturnValue", 0, &ret_value, NULL, 0);
  if (FAILED(hr) || (0 != ret_value.uintVal))
    return false;

  CComVariant pid;
  hr = out_params->Get(L"ProcessId", 0, &pid, NULL, 0);
  if (FAILED(hr) || (0 == pid.intVal))
    return false;

  if (process_id)
    *process_id = pid.intVal;

  return true;
}
