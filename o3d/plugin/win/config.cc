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


// This file contains code to check the hardware and software configuration of
// the client machine:
// - User agent (browser)
// - Windows version
// - GPU vendor

// TODO: Waiting on posix updates to be able to include this in order
//            to parse the useragent string for browser version.
// #include <regex.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <tchar.h>
#include <windows.h>
#ifdef RENDERER_D3D9
#include <d3d9.h>
#endif

#include <string>
#include <iostream>
#include <fstream>

#include "base/logging.h"
#include "plugin/cross/config.h"
#include "plugin/cross/plugin_metrics.h"
#include "core/cross/install_check.h"
#include "third_party/nixysa/files/static_glue/npapi/common.h"

namespace o3d {

// Check Windows version.
bool CheckOSVersion(NPP npp) {
  OSVERSIONINFOEX version = {sizeof(OSVERSIONINFOEX)};  // NOLINT
  GetVersionEx(reinterpret_cast<OSVERSIONINFO *>(&version));
  if (version.dwMajorVersion == 5 && version.dwMinorVersion == 1) {
    // NT 5.1 = Windows XP
    if (version.wServicePackMajor < 2) {
      // TODO: internationalize messages.
      std::string error = std::string("Windows XP Service Pack 2 is required.");
      if (!AskUser(npp, error)) return false;
    }
  } else if (version.dwMajorVersion > 5 ||
	         (version.dwMajorVersion == 5 && version.dwMinorVersion >= 2)) {
    // 6.0 is Vista or Server 2008; it's now worth a try.
  } else {
    std::string error = std::string("Unsupported Windows version.");
    if (!AskUser(npp, error)) return false;
  }
  return true;
}

// Checks user agent string. We only allow Firefox, Chrome, and IE.
bool CheckUserAgent(NPP npp, const std::string &user_agent) {
  if (user_agent.find("Firefox") == user_agent.npos &&
      user_agent.find("Chrome") == user_agent.npos &&
      user_agent.find("MSIE") == user_agent.npos) {
    std::string error = std::string("Unsupported user agent: ") + user_agent;
    return AskUser(npp, error);
  }
  return true;
}

bool OpenDriverBlacklistFile(std::ifstream *input_file) {
  CHECK(input_file);
  CHECK(!input_file->is_open());

  // Determine the full path.
  // It will look something like:
  // "c:\Documents and Settings\username\Application Data\Google\O3D\
  //    driver_blacklist.txt"

  TCHAR app_data_path[MAX_PATH];
  HRESULT result = SHGetFolderPath(
      NULL,
      CSIDL_APPDATA,
      NULL,
      0,
      app_data_path);

  if (result != 0) {
    return false;
  }

  PathAppend(app_data_path, _T("Google\\O3D\\driver_blacklist.txt"));
  if (!PathFileExists(app_data_path)) {
    return false;
  }
  input_file->open(app_data_path, std::ifstream::in);
  return input_file->good();
}

bool GetUserConfigMetrics() {
  // Check Windows version.
  o3d::metric_system_type = o3d::SYSTEM_NAME_WIN;

  OSVERSIONINFOEX version = {sizeof(OSVERSIONINFOEX)};  // NOLINT
  GetVersionEx(reinterpret_cast<OSVERSIONINFO *>(&version));
  o3d::metric_windows_major_version = version.dwMajorVersion;
  o3d::metric_windows_minor_version = version.dwMinorVersion;
  o3d::metric_windows_sp_major_version = version.wServicePackMajor;
  o3d::metric_windows_sp_minor_version = version.wServicePackMinor;

  // Check the device capabilities.
#ifdef RENDERER_D3D9
  // Check GPU vendor using D3D.
  IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
  if (!d3d) {
    o3d::metric_direct3d_available.Set(false);
    DLOG(ERROR) << "Direct3D9 is unavailable";
    return false;
  }
  o3d::metric_direct3d_available.Set(true);
  D3DADAPTER_IDENTIFIER9 identifier;
  HRESULT hr = d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &identifier);
  D3DCAPS9 d3d_caps;
  HRESULT caps_result = d3d->GetDeviceCaps(D3DADAPTER_DEFAULT,
                                           D3DDEVTYPE_HAL,
                                           &d3d_caps);
  // Get GPU device information
  if (hr != D3D_OK) {
    DLOG(ERROR) << "Unable to get device ID";
    return false;
  }
  o3d::metric_gpu_vendor_id = identifier.VendorId;
  o3d::metric_gpu_device_id = identifier.DeviceId;
  o3d::metric_gpu_driver_major_version = identifier.DriverVersion.LowPart;
  o3d::metric_gpu_driver_minor_version = identifier.DriverVersion.HighPart;

  // Need to release after we get the vram size
  d3d->Release();

  // Get shader versions
  DWORD pixel_shader = d3d_caps.PixelShaderVersion;
  o3d::metric_pixel_shader_main_version =
      D3DSHADER_VERSION_MAJOR(pixel_shader);
  o3d::metric_pixel_shader_sub_version =
      D3DSHADER_VERSION_MINOR(pixel_shader);
  DWORD vertex_shader = d3d_caps.VertexShaderVersion;
  o3d::metric_vertex_shader_main_version =
      D3DSHADER_VERSION_MAJOR(vertex_shader);
  o3d::metric_vertex_shader_sub_version =
      D3DSHADER_VERSION_MINOR(vertex_shader);

  // Detemine if device can handle NPoT textures
  o3d::metric_POW2_texture_caps.Set(
      (d3d_caps.TextureCaps & D3DPTEXTURECAPS_POW2) != 0);
  o3d::metric_NONPOW2CONDITIONAL_texture_caps.Set(
      (d3d_caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) != 0);

  o3d::metric_d3d_devcaps = d3d_caps.DevCaps;
  o3d::metric_d3d_misccaps = d3d_caps.PrimitiveMiscCaps;
  o3d::metric_d3d_rastercaps = d3d_caps.RasterCaps;
  o3d::metric_d3d_zcmpcaps = d3d_caps.ZCmpCaps;
  o3d::metric_d3d_srcblendcaps = d3d_caps.SrcBlendCaps;
  o3d::metric_d3d_dstblendcaps = d3d_caps.DestBlendCaps;
  o3d::metric_d3d_alphacaps = d3d_caps.AlphaCmpCaps;
  o3d::metric_d3d_texcaps = d3d_caps.TextureCaps;
  o3d::metric_d3d_texfiltercaps = d3d_caps.TextureFilterCaps;
  o3d::metric_d3d_cubetexfiltercaps = d3d_caps.CubeTextureFilterCaps;
  o3d::metric_d3d_texaddrcaps = d3d_caps.TextureAddressCaps;
  o3d::metric_d3d_linecaps = d3d_caps.LineCaps;
  o3d::metric_d3d_stencilcaps = d3d_caps.StencilCaps;
  o3d::metric_d3d_texopcaps = d3d_caps.TextureOpCaps;
  o3d::metric_d3d_vs20caps = d3d_caps.VS20Caps.Caps;
  o3d::metric_d3d_vs20_dynflowctrldepth =
      d3d_caps.VS20Caps.DynamicFlowControlDepth;
  o3d::metric_d3d_vs20_numtemps =  d3d_caps.VS20Caps.NumTemps;
  o3d::metric_d3d_vs20_staticflowctrldepth =
      d3d_caps.VS20Caps.StaticFlowControlDepth;
  o3d::metric_d3d_ps20caps = d3d_caps.PS20Caps.Caps;
  o3d::metric_d3d_ps20_dynflowctrldepth =
      d3d_caps.PS20Caps.DynamicFlowControlDepth;
  o3d::metric_d3d_ps20_numtemps = d3d_caps.PS20Caps.NumTemps;
  o3d::metric_d3d_ps20_staticflowctrldepth =
      d3d_caps.PS20Caps.StaticFlowControlDepth;
  o3d::metric_d3d_ps20_numinstrslots = d3d_caps.PS20Caps.NumInstructionSlots;
#else
  o3d::metric_direct3d_available.Set(false);
#endif
  return true;
}

bool GetUserAgentMetrics(NPP npp) {
  // Check User agent so we can get the browser
  // TODO: This is the best we could come up with for this in order to
  //            go from browser to string.
  GLUE_PROFILE_START(npp, "uagent");
  std::string user_agent = NPN_UserAgent(npp);
  GLUE_PROFILE_STOP(npp, "uagent");
  // The Chrome user_agent string also contains Safari. Search for Chrome first.
  if (std::string::npos != user_agent.find("Chrome")) {
    o3d::metric_browser_type = o3d::BROWSER_NAME_CHROME;
  } else if (std::string::npos != user_agent.find("Safari")) {
    o3d::metric_browser_type = o3d::BROWSER_NAME_SAFARI;
  } else if (std::string::npos != user_agent.find("Opera")) {
    o3d::metric_browser_type = o3d::BROWSER_NAME_OPERA;
  } else if (std::string::npos != user_agent.find("Firefox")) {
    o3d::metric_browser_type = o3d::BROWSER_NAME_FIREFOX;
  } else if (std::string::npos != user_agent.find("MSIE")) {
    o3d::metric_browser_type = o3d::BROWSER_NAME_MSIE;
  } else {
    o3d::metric_browser_type = o3d::BROWSER_NAME_UNKNOWN;
  }
  return true;
}
}  // namespace o3d
