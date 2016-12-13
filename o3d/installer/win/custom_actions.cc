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

// Custom installer action which checks if DirectX 9.0c or later is present.

#include <dxdiag.h>
#include <msi.h>
#include <msiquery.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <tchar.h>
#include <strsafe.h>  // Must be after tchar.h.
#include <windows.h>
#include <atlstr.h>

#include "plugin/win/update_lock.h"

#pragma comment(linker, "/EXPORT:CheckDirectX=_CheckDirectX@4")
#pragma comment(linker, "/EXPORT:IsSoftwareRunning=_IsSoftwareRunning@4")
#pragma comment(linker, "/EXPORT:InstallD3DXIfNeeded=_InstallD3DXIfNeeded@4")

#if 0
// NOTE: Useful for debugging, but not currently in use.  Left here so
// that I don't have to figure out how to write it again.
void PopupMsiError(MSIHANDLE installer_handle, int id) {
  PMSIHANDLE record_handle = MsiCreateRecord(1);
  MsiRecordSetInteger(record_handle, 1, id);
  MsiProcessMessage(installer_handle, INSTALLMESSAGE(INSTALLMESSAGE_USER|MB_OK),
                    record_handle);
  MsiCloseHandle(record_handle);
}
#endif

void WriteToMsiLog(MSIHANDLE installer_handle, TCHAR *message) {
  PMSIHANDLE record_handle = MsiCreateRecord(1);
  MsiRecordSetString(record_handle, 1, message);
  MsiProcessMessage(installer_handle, INSTALLMESSAGE_INFO, record_handle);
  MsiCloseHandle(record_handle);
}

HRESULT SetRegKeyValueString(HKEY hkey_parent, const TCHAR *key_name,
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

HRESULT SetRegKeyValueDWord(HKEY hkey_parent, const TCHAR *key_name,
    const TCHAR *value_name, DWORD value) {
  HKEY hkey;
  LONG res = ::RegCreateKeyEx(hkey_parent, key_name, 0, 0,
      REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hkey, NULL);
  HRESULT hr = HRESULT_FROM_WIN32(res);

  if (hr != S_OK) {
    return hr;
  }

  res = ::RegSetValueEx(hkey, value_name, 0, REG_DWORD,
      reinterpret_cast<const BYTE *>(&value), sizeof(value));
  hr = HRESULT_FROM_WIN32(res);

  ::RegCloseKey(hkey);
  return hr;
}

// Retrieve the currently installed version of DirectX using a COM
// DxDiagProvider.  Returns 0 on error.
DWORD GetDirectXVersion() {
  HRESULT hr;
  DWORD directx_version = 0;
  DWORD directx_version_major = 0;
  DWORD directx_version_minor = 0;
  TCHAR directx_version_letter = ' ';

  bool cleanup_com = false;

  bool success_getting_major = false;
  bool success_getting_minor = false;
  bool success_getting_letter = false;

  // Init COM.  COM may fail if its already been inited with a different
  // concurrency model.  And if it fails you shouldn't release it.
  hr = CoInitialize(NULL);
  cleanup_com = SUCCEEDED(hr);

  // Get an IDxDiagProvider
  IDxDiagProvider* dx_diag_provider = NULL;
  hr = CoCreateInstance(CLSID_DxDiagProvider,
                        NULL,
                        CLSCTX_INPROC_SERVER,
                        IID_IDxDiagProvider,
                        reinterpret_cast<LPVOID*>(&dx_diag_provider));
  if (SUCCEEDED(hr)) {
    // Fill out a DXDIAG_INIT_PARAMS struct
    DXDIAG_INIT_PARAMS dx_diag_init_param;
    ZeroMemory(&dx_diag_init_param, sizeof(DXDIAG_INIT_PARAMS));
    dx_diag_init_param.dwSize = sizeof(DXDIAG_INIT_PARAMS);
    dx_diag_init_param.dwDxDiagHeaderVersion = DXDIAG_DX9_SDK_VERSION;
    dx_diag_init_param.bAllowWHQLChecks = false;
    dx_diag_init_param.pReserved = NULL;

    // Init the DxDiagProvider
    hr = dx_diag_provider->Initialize(&dx_diag_init_param);
    if (SUCCEEDED(hr)) {
      IDxDiagContainer* dx_diag_root = NULL;
      IDxDiagContainer* dx_diag_system_info = NULL;

      // Get the DxDiag root container
      hr = dx_diag_provider->GetRootContainer(&dx_diag_root);
      if (SUCCEEDED(hr)) {
        // Get the object called DxDiag_SystemInfo
        hr = dx_diag_root->GetChildContainer(L"DxDiag_SystemInfo",
                                             &dx_diag_system_info);
        if (SUCCEEDED(hr)) {
          VARIANT var;
          VariantInit(&var);

          // Get the "dwDirectXVersionMajor" property
          hr = dx_diag_system_info->GetProp(L"dwDirectXVersionMajor", &var);
          if (SUCCEEDED(hr) && var.vt == VT_UI4) {
            directx_version_major = var.ulVal;
            success_getting_major = true;
          }
          VariantClear(&var);

          // Get the "dwDirectXVersionMinor" property
          hr = dx_diag_system_info->GetProp(L"dwDirectXVersionMinor", &var);
          if (SUCCEEDED(hr) && var.vt == VT_UI4) {
            directx_version_minor = var.ulVal;
            success_getting_minor = true;
          }
          VariantClear(&var);

          // Get the "szDirectXVersionLetter" property
          hr = dx_diag_system_info->GetProp(L"szDirectXVersionLetter", &var);
          if (SUCCEEDED(hr) && var.vt == VT_BSTR &&
              SysStringLen(var.bstrVal) != 0) {
#ifdef UNICODE
            directx_version_letter = var.bstrVal[0];
#else
            char strDestination[10];
            WideCharToMultiByte(CP_ACP, 0, var.bstrVal, -1, strDestination,
                                10 * sizeof(CHAR), NULL, NULL);
            directx_version_letter =
                static_cast<char>(tolower(strDestination[0]));
#endif
            success_getting_letter = true;
          }
          VariantClear(&var);

          // If it all worked right, then mark it down
          if (success_getting_major && success_getting_minor &&
              success_getting_letter) {
            // Convert to hex representation.
            directx_version = directx_version_major;
            directx_version <<= 8;
            directx_version += directx_version_minor;
            directx_version <<= 8;
            if (directx_version_letter >= 'a' &&
                directx_version_letter <= 'z') {
              directx_version += (directx_version_letter - 'a') + 1;
            }
          }
          dx_diag_system_info->Release();
        }
        dx_diag_root->Release();
      }
    }
    dx_diag_provider->Release();
  }

  if (cleanup_com) {
    CoUninitialize();
  }
  return directx_version;
}

HRESULT SetCustomUpdateError(MSIHANDLE installer_handle,
                             DWORD error_code,
                             CString message) {
    wchar_t key_name[256];
    DWORD key_size = 256;
    UINT ret = MsiGetProperty(installer_handle, L"GoogleUpdateResultKey",
        reinterpret_cast<LPWSTR>(&key_name), &key_size);
    if (ret != ERROR_SUCCESS) {
      WriteToMsiLog(installer_handle, _T("MsiGetProperty failed!"));
      return ERROR_READ_FAULT;
    } else {
      HRESULT hr = SetRegKeyValueDWord(HKEY_CURRENT_USER, key_name,
          _T("InstallerResult"), 1 /* FAILED_CUSTOM_ERROR */);
      if (hr != S_OK) {
        WriteToMsiLog(installer_handle, _T("SetRegKeyValueDWord failed!"));
        return hr;
      }
      hr = SetRegKeyValueDWord(HKEY_CURRENT_USER, key_name,
          _T("InstallerError"), error_code);
      if (hr != S_OK) {
        WriteToMsiLog(installer_handle, _T("SetRegKeyValueDWord failed!"));
        return hr;
      }
      hr = SetRegKeyValueString(HKEY_CURRENT_USER, key_name,
          _T("InstallerResultUIString"),
          message, message.GetLength() * sizeof(TCHAR));
      if (hr != S_OK) {
        WriteToMsiLog(installer_handle, _T("SetRegKeyValueString failed!"));
        return hr;
      }
    }
}

// Check whether DirectX version 9.0c or higher is installed and
// notify the installer about the result.
extern "C" UINT __stdcall CheckDirectX(MSIHANDLE installer_handle) {
  // Get current version.
  DWORD installed_version = GetDirectXVersion();

  // 0x090003 == 9.0c
  if (installed_version >= 0x090003) {
    // Set msi property to let the installer know that the currently
    // installed version of dx is new enough.
    UINT ret = MsiSetProperty(installer_handle, L"DIRECTX_9_0_C_INSTALLED",
                              L"1");
  } else {
    // TODO: This will need i18n when we do that for the rest of o3d.
    CString message =
        _T("O3D needs an installation of DirectX 9.0 revision C or later.\n")
            _T("\nPlease download the latest version of DirectX from ")
            _T("http://www.microsoft.com/download.");
    DWORD error_code = 1603; /* Fatal error during installation */
    if (SetCustomUpdateError(installer_handle, error_code, message) != S_OK) {
      return ERROR_WRITE_FAULT;
    }
  }
  return ERROR_SUCCESS;
}

// Check to see whether the plugin is currently running.  If it is, we can't
// update the plugin.  The installer will check for the SOFTWARE_RUNNING flag
// and exit if it's trying to do a silent update.  Knowing that it's failed this
// time, it'll try again later.
extern "C" UINT __stdcall IsSoftwareRunning(MSIHANDLE installer_handle) {
  if (!update_lock::CanUpdate()) {
    MsiSetProperty(installer_handle, L"SOFTWARE_RUNNING", L"1");
  }

  return ERROR_SUCCESS;
}

// TODO: Get security to review this method in particular, as it runs an
// executable in a predictable location.
extern "C" UINT __stdcall InstallD3DXIfNeeded(MSIHANDLE installer_handle) {
  HANDLE handle = ::LoadLibrary(L"d3dx9_36.dll");
  if (handle) {
    ::CloseHandle(handle);
  } else {
    // 2 output characters per byte in the input due to hex format, then one
    // extra for the NUL.
    TCHAR idString[sizeof(DWORD) * 2 + 1];
    HRESULT hr = ::StringCchPrintf(idString, sizeof(idString) / sizeof(TCHAR),
                                   _T("%x"), ::GetCurrentProcessId());
    if (!SUCCEEDED(hr)) {
      WriteToMsiLog(installer_handle, _T("StringCchPrintf failed!"));
      return ERROR_GEN_FAILURE;
    }
    TCHAR getextras_path[] = _T("%TEMP%\\Extras\\getextras.exe");
    SHELLEXECUTEINFO info = {0};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_DOENVSUBST | SEE_MASK_FLAG_NO_UI | SEE_MASK_UNICODE;
    info.lpVerb = _T("open");
    info.lpFile = getextras_path;
    info.lpParameters = idString;
    // SW_HIDE is a wild guess, but seems as good as any.
    info.nShow = SW_HIDE;
    info.lpDirectory = NULL;
    if (!::ShellExecuteEx(&info)) {
      WriteToMsiLog(installer_handle,
                    _T("ShellExecuteEx of getextras.exe failed."));
      WriteToMsiLog(installer_handle,
                    _T("Path was:"));
      WriteToMsiLog(installer_handle, getextras_path);
      return ERROR_GEN_FAILURE;
    }
  }
  return ERROR_SUCCESS;
}
