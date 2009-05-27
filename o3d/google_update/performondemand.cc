/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// A simple tool for performing and interacting with on demand updates.

#include "google_update/performondemand.h"
#include <windows.h>
#include <sddl.h>
#include <shlobj.h>
#include <atltime.h>
#include <tchar.h>

namespace o3d {

namespace google_update {

// Helper function to convert string to GUID
GUID StringToGuid(const CString& str) {
  GUID guid(GUID_NULL);
  if (!str.IsEmpty()) {
    TCHAR* s = const_cast<TCHAR*>(str.GetString());
    if (NO_ERROR != ::CLSIDFromString(s, &guid)) {
      // Docs don't guarantee that the guid's unmodified on failure.
      guid = GUID_NULL;
    }
  }
  return guid;
}

#define arraysize(x) (sizeof(x) / sizeof(x[0]))

CString GuidToString(const GUID& guid) {
  TCHAR guid_str[40] = {0};
  ::StringFromGUID2(guid, guid_str, arraysize(guid_str));
  ::CharUpperBuff(guid_str, arraysize(guid_str));
  return guid_str;
}

HRESULT GetRegKeyValue(HKEY hkey_parent, const TCHAR *key_name,
    const TCHAR * value_name, TCHAR * * value) {
  HKEY hkey;
  LONG res = ::RegOpenKeyEx(hkey_parent, key_name, 0, KEY_READ, &hkey);
  HRESULT hr = HRESULT_FROM_WIN32(res);

  if (hr != S_OK) {
    return hr;
  }

  DWORD byte_count = 0;
  DWORD type = 0;

  // first get the size of the string buffer
  res = ::SHQueryValueEx(hkey, value_name, NULL, &type, NULL, &byte_count);
  hr = HRESULT_FROM_WIN32(res);

  if (hr == S_OK) {
    // allocate room for the string and a terminating \0
    *value = new TCHAR[(byte_count / sizeof(TCHAR)) + 1];

    if ((*value) != NULL) {
      if (byte_count != 0) {
        // make the call again
        res = ::SHQueryValueEx(hkey, value_name, NULL, &type,
                              reinterpret_cast<byte*>(*value), &byte_count);
        hr = HRESULT_FROM_WIN32(res);
      } else {
        (*value)[0] = _T('\0');
      }

      if (hr == S_OK && (type != REG_SZ) && (type != REG_MULTI_SZ) &&
            (type != REG_EXPAND_SZ)) {
        hr = E_UNEXPECTED;
      }
    } else {
      hr = E_OUTOFMEMORY;
    }
  }

  ::RegCloseKey(hkey);
  return hr;
}

HRESULT SetRegKeyValue(HKEY hkey_parent, const TCHAR *key_name,
    const TCHAR *value_name, const TCHAR *value, DWORD value_size) {
  HKEY hkey;
  LONG res = ::RegCreateKeyEx(hkey_parent, key_name, 0, 0,
      REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hkey, NULL);
  HRESULT hr = HRESULT_FROM_WIN32(res);

  if (hr != S_OK) {
    return hr;
  }

  res = ::RegSetValueEx(hkey, value_name, 0, REG_SZ,
      reinterpret_cast<const BYTE *>(value), value_size);
  hr = HRESULT_FROM_WIN32(res);

  ::RegCloseKey(hkey);
  return hr;
}

// Reads the Proxy information for the given interface from HKCU, and registers
// it with COM.
HRESULT RegisterHKCUPSClsid(IID iid,
                            HMODULE* proxy_module,
                            DWORD* revoke_cookie) {
  *proxy_module = NULL;
  *revoke_cookie = 0;

  const TCHAR* const hkcu_classes_key = _T("Software\\Classes\\");

  // Get the registered proxy for the interface.
  CString interface_proxy_clsid_key;
  interface_proxy_clsid_key.Format(_T("%sInterface\\%s\\ProxyStubClsid32"),
                                   hkcu_classes_key, GuidToString(iid));
  TCHAR *proxy_clsid32_value;
  HRESULT hr = GetRegKeyValue(HKEY_CURRENT_USER,
                              interface_proxy_clsid_key,
                              NULL,
                              &proxy_clsid32_value);
  if (FAILED(hr)) {
    return hr;
  }

  // Get the location of the proxy/stub DLL.
  CString proxy_server32_entry;
  proxy_server32_entry.Format(_T("%sClsid\\%s\\InprocServer32"),
                              hkcu_classes_key, proxy_clsid32_value);
  TCHAR *hkcu_proxy_dll_path;
  hr = GetRegKeyValue(HKEY_CURRENT_USER,
                      proxy_server32_entry,
                      NULL,
                      &hkcu_proxy_dll_path);
  if (FAILED(hr)) {
    return hr;
  }

  // Get the proxy/stub class object.
  typedef HRESULT (STDAPICALLTYPE *DllGetClassObjectTypedef)(REFCLSID clsid,
                                                             REFIID iid,
                                                             void** ptr);
  *proxy_module = ::LoadLibrary(hkcu_proxy_dll_path);
  DllGetClassObjectTypedef fn = NULL;

  fn = reinterpret_cast<DllGetClassObjectTypedef>(
      ::GetProcAddress(*proxy_module, "DllGetClassObject"));
  if (!fn) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    return hr;
  }
  CComPtr<IPSFactoryBuffer> fb;
  CLSID proxy_clsid = StringToGuid(proxy_clsid32_value);
  hr = (*fn)(proxy_clsid, IID_IPSFactoryBuffer, reinterpret_cast<void**>(&fb));
  if (FAILED(hr)) {
    return hr;
  }

  // Register the proxy/stub class object.
  hr = ::CoRegisterClassObject(proxy_clsid, fb, CLSCTX_INPROC_SERVER,
                               REGCLS_MULTIPLEUSE, revoke_cookie);
  if (FAILED(hr)) {
    return hr;
  }

  // Relate the interface with the proxy/stub, so COM does not do a lookup when
  // unmarshaling the interface.
  hr = ::CoRegisterPSClsid(iid, proxy_clsid);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

// Assumes you're running on Vista; call IsRunningOnVista first to check.
bool IsUserRunningSplitToken() {
  HANDLE process_token;
  if (!::OpenProcessToken(GetCurrentProcess(),
                          TOKEN_QUERY,
                          &process_token)) {
    return false;
  }

  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD size_returned = 0;
  bool ret = false;
  if (::GetTokenInformation(process_token,
                            TokenElevationType,
                            &elevation_type,
                            sizeof(elevation_type),
                            &size_returned)) {
    ret = elevation_type == TokenElevationTypeFull ||
        elevation_type == TokenElevationTypeLimited;
  }
  ::CloseHandle(process_token);

  return ret;
}

// If this function fails to find any of the info it's looking for, it'll
// default to saying "No, this isn't Vista".
bool IsRunningOnVista() {
  // Use GetVersionEx to get OS and Service Pack information.
  OSVERSIONINFOEX osviex;
  ::ZeroMemory(&osviex, sizeof(OSVERSIONINFOEX));
  osviex.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  BOOL success = ::GetVersionEx(reinterpret_cast<OSVERSIONINFO *>(&osviex));

  // If ::GetVersionEx fails when given an OSVERSIONINFOEX then we're running
  // on NT4.0SP5 or earlier.
  if (!success) {
    return false;
  } else if (osviex.dwPlatformId == VER_PLATFORM_WIN32_NT &&
      osviex.dwMajorVersion == 6 && osviex.dwMinorVersion == 0) {
    return true;
  }

  return false;
}

// Sets the thread token to medium integrity, which allows for out-of-proc HKCU
// COM server activation.  For more info on process integrity levels, see
// http://en.wikipedia.org/wiki/Mandatory_Integrity_Control.
DWORD SetTokenIntegrityLevelMedium(HANDLE token) {
  PSID medium_sid = NULL;
  if (!::ConvertStringSidToSid(SDDL_ML_MEDIUM, &medium_sid)) {
    return ::GetLastError();
  }

  TOKEN_MANDATORY_LABEL label = {0};
  label.Label.Attributes = SE_GROUP_INTEGRITY;
  label.Label.Sid = medium_sid;

  size_t size = sizeof(TOKEN_MANDATORY_LABEL) + ::GetLengthSid(medium_sid);
  BOOL success = ::SetTokenInformation(token, TokenIntegrityLevel, &label,
                                      size);
  DWORD result = success ? ERROR_SUCCESS : ::GetLastError();
  ::LocalFree(medium_sid);
  return result;
}

// A helper class for clients of the Google Update on-demand out-of-proc COM
// server.  An instance of this class is typically created on the stack. The
// class does nothing for cases where the OS is not Vista with UAC off.
// This class does the following:
// * Calls CoInitializeSecurity with cloaking set to dynamic. This makes COM
//   use the thread token instead of the process token.
// * Impersonates and sets the thread token to medium integrity. This allows for
//   out-of-proc HKCU COM server activation.
// * Reads and registers per-user proxies for the interfaces that on-demand
//   exposes.
class VistaProxyRegistrar {
 public:
  VistaProxyRegistrar()
      : googleupdate_cookie_(0),
        jobobserver_cookie_(0),
        progresswndevents_cookie_(0),
        is_impersonated(false),
        failed(false),
        googleupdate_library_(NULL),
        jobobserver_library_(NULL),
        progresswndevents_library_(NULL) {
    HRESULT hr = VistaProxyRegistrarImpl();
    if (FAILED(hr)) {
      failed = true;
    }
  }

  ~VistaProxyRegistrar() {
    if (googleupdate_cookie_) {
      if (FAILED((::CoRevokeClassObject(googleupdate_cookie_)))) {
        // TODO: How can we get errors out of the installer?
        // LOG(ERROR("CoRevokeClassObject failed."));
      }
    }

    if (jobobserver_cookie_) {
      if (FAILED((::CoRevokeClassObject(jobobserver_cookie_)))) {
        // LOG(ERROR("CoRevokeClassObject failed."));
      }
    }

    if (progresswndevents_cookie_) {
      if (FAILED((::CoRevokeClassObject(progresswndevents_cookie_)))) {
        // LOG(ERROR("CoRevokeClassObject failed."));
      }
    }

    if (is_impersonated) {
      if (FAILED(::RevertToSelf())) {
        // LOG(ERROR("RevertToSelf failed."));
      }
    }
    ::FreeLibrary(googleupdate_library_);
    ::FreeLibrary(jobobserver_library_);
    ::FreeLibrary(progresswndevents_library_);
  }

 private:
  HRESULT VistaProxyRegistrarImpl() {
    if (!IsRunningOnVista() ||
        IsUserRunningSplitToken() ||
        !::IsUserAnAdmin()) {
      return S_OK;
    }

    // Needs to be called very early on in a process.
    // Turn on dynamic cloaking so COM picks up the impersonated thread token.
    HRESULT hr = ::CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IDENTIFY,
        NULL,
        EOAC_DYNAMIC_CLOAKING,
        NULL);
    if (FAILED(hr)) {
      return hr;
    }

    is_impersonated = !!::ImpersonateSelf(SecurityImpersonation);
    if (!is_impersonated) {
      hr = HRESULT_FROM_WIN32(::GetLastError());
      return hr;
    }

    HANDLE thread_token;
    if (!::OpenThreadToken(::GetCurrentThread(),
                           TOKEN_ALL_ACCESS,
                           false,
                           &thread_token)) {
      hr = HRESULT_FROM_WIN32(::GetLastError());
      return hr;
    }

    DWORD result = SetTokenIntegrityLevelMedium(thread_token);
    ::CloseHandle(thread_token);
    if (result != ERROR_SUCCESS) {
      return HRESULT_FROM_WIN32(result);
    }

    hr = RegisterHKCUPSClsid(__uuidof(IGoogleUpdate),
                             &googleupdate_library_,
                             &googleupdate_cookie_);
    if (FAILED(hr)) {
      return hr;
    }

    hr = RegisterHKCUPSClsid(__uuidof(IJobObserver),
                             &jobobserver_library_,
                             &jobobserver_cookie_);
    if (FAILED(hr)) {
      return hr;
    }

    hr = RegisterHKCUPSClsid(__uuidof(IProgressWndEvents),
                             &progresswndevents_library_,
                             &progresswndevents_cookie_);
    if (FAILED(hr)) {
      return hr;
    }

    return S_OK;
  }

 private:
  HMODULE googleupdate_library_;
  HMODULE jobobserver_library_;
  HMODULE progresswndevents_library_;

  DWORD googleupdate_cookie_;
  DWORD jobobserver_cookie_;
  DWORD progresswndevents_cookie_;
  bool is_impersonated;
  bool failed;
};

class scoped_co_init {
 public:
   scoped_co_init() {
     hr_ = ::CoInitializeEx(0, COINIT_APARTMENTTHREADED);
   }
 ~scoped_co_init() {
    if (SUCCEEDED(hr_))
        ::CoUninitialize();
 }
 private:
  HRESULT hr_;
};

int PerformOnDemandInstall(CString guid) {
  // Verify that the guid is valid.
  GUID parsed = StringToGuid(guid);
  if (parsed == GUID_NULL) {
    return -1;
  }

  // Set a fake registry value that tells Google Update that the package is
  // already installed, but with an ancient version that needs updating.  This
  // is because Google Update doesn't really support install-on-demand, only
  // update-on-demand.
  // TODO: Check with the google_update team if this does any damage if the
  // software's already installed.  If so, we should attempt to determine
  // whether it's installed first--although we should be wary of trusting
  // these registry entries, which might be stale.
  CString key_path(_T("Software\\Google\\Update\\Clients\\"));
  key_path += guid;
  CString value = _T("0.0.0.1");
  HRESULT hr = SetRegKeyValue(HKEY_CURRENT_USER, key_path, _T("pv"),
      value, value.GetLength() * sizeof(TCHAR));

  if (FAILED(hr)) {
    return hr;
  }
  int timeout = 60;
  CComModule module;
  scoped_co_init com_apt;
  VistaProxyRegistrar registrar;

  CComObject<JobObserver>* job_observer;
  hr = CComObject<JobObserver>::CreateInstance(&job_observer);
  if (!SUCCEEDED(hr)) {
    return -1;
  }
  CComPtr<IJobObserver> job_holder(job_observer);

  CComPtr<IGoogleUpdate> on_demand;
  hr = on_demand.CoCreateInstance(__uuidof(OnDemandUserAppsClass));

  if (!SUCCEEDED(hr)) {
    return -1;
  }

  hr = on_demand->Update(guid, job_observer);

  if (!SUCCEEDED(hr)) {
    return -1;
  }

  // Main message loop:
  MSG msg;
  SYSTEMTIME start_system_time = {0};
  SYSTEMTIME current_system_time = {0};
  ::GetSystemTime(&start_system_time);
  CTime start_time(start_system_time);
  CTimeSpan timeout_period(0, 0, 0, timeout);

  while (::GetMessage(&msg, NULL, 0, 0))
  {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
    ::GetSystemTime(&current_system_time);
    CTime current_time(current_system_time);
    CTimeSpan elapsed_time = current_time - start_time;
    if (timeout_period < elapsed_time) {
      // TODO: Right now the timeout does correctly break, but then
      // the COM interactions continue on to completion.
      break;
    }
  }
  int ret_val = job_observer->observed;

  if (!ret_val) {
    return -1;  // This really shouldn't happen, but just in case...
  }

  if (ret_val & (JobObserver::ON_COMPLETE_SUCCESS |
      JobObserver::ON_COMPLETE_SUCCESS_CLOSE_UI)) {
    return 0;  // The success case.
  }

  return ret_val;  // Otherwise tell what happened.  Never sets all bits (-1).
}

int GetD3DX9() {
  return PerformOnDemandInstall("{34B2805D-C72C-4f81-AED5-5A22D1E092F1}");
}

}  // namespace google_update

}  // namespace o3d

// Arguments expected:
// [0]: Our binary's name, as usual
// [1]: The process ID of the Google Update process, in hex; we'll wait for it
// to exit before doing our stuff.  If there's no process ID supplied, don't
// wait.
int _tmain(int argc, TCHAR* argv[]) {
  if (argc > 2) {
    return -1;
  }
  if (argc == 2) {
    TCHAR *end = NULL;
    DWORD google_update_id = _tcstol(argv[1], &end, 16);
    if (!google_update_id || end == argv[1]) {
      return -1;
    }

    HANDLE google_update_handle = OpenProcess(SYNCHRONIZE, false,
                                              google_update_id);

    if (google_update_handle) {
      // 120 minutes is safer than INFINITY, but effectively the same thing.
      DWORD timeout = 120 * 60 * 1000;
      DWORD waitResponse = WaitForSingleObject(google_update_handle, timeout);

      if (waitResponse == WAIT_TIMEOUT || waitResponse == WAIT_FAILED) {
        return -1;
      }
      if (waitResponse != WAIT_ABANDONED && waitResponse != WAIT_OBJECT_0) {
        return -1;
      }
    } // Else optimistically assume it already exited.
  }
  return o3d::google_update::GetD3DX9();
}
