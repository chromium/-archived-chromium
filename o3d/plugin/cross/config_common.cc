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


// This file contains common code to check the hardware and software
// configuration of the client machine:
// - User agent (browser)
// - OS version
// - GPU vendor

#ifdef RENDERER_D3D9
#include <d3d9.h>
#endif

#include <iostream>
#include <fstream>

#include "base/logging.h"
#include "core/cross/install_check.h"
#include "plugin/cross/config.h"
#include "plugin/cross/o3d_glue.h"
#include "core/cross/error.h"
#include "third_party/nixysa/files/static_glue/npapi/common.h"

using glue::_o3d::GetServiceLocator;

// Gets the value of "navigator.userAgent" in the JavaScript context, which
// contains the user agent string.
std::string GetUserAgent(NPP npp) {
  GLUE_PROFILE_START(npp, "NPN_UserAgent");
  const char* user_agent = NPN_UserAgent(npp);
  GLUE_PROFILE_STOP(npp, "NPN_UserAgent");
  std::string uagent_string;
  if (user_agent) {
    uagent_string = std::string(user_agent);
  }
  return uagent_string;
}

// Pops up a dialog box using JavaScript showing the error and gives the user a
// chance to continue anyway.
bool AskUser(NPP npp, const std::string &error) {
  NPObject *global_object;
  GLUE_PROFILE_START(npp, "NPN_GetValue");
  NPN_GetValue(npp, NPNVWindowNPObject, &global_object);
  GLUE_PROFILE_STOP(npp, "NPN_GetValue");
  GLUE_PROFILE_START(npp, "NPN_GetStringIdentifier");
  NPIdentifier alert_id = NPN_GetStringIdentifier("confirm");
  GLUE_PROFILE_STOP(npp, "NPN_GetStringIdentifier");
  std::string message = std::string("O3D: ") + error;
  // TODO: internationalize message.
  // TODO: Should this change to call some hardcoded javascript function
  //    like "o3djs.util.confirmContinuation" or even a global name like
  //    o3djs_confirmContinuation. This would move localization outside
  //    C++ and give the developer a chance to handle it his own way.
  message += "\nPress OK to continue anyway.";

  NPVariant args[1];
  NPVariant result;
  STRINGN_TO_NPVARIANT(message.c_str(), message.length(), args[0]);
  GLUE_PROFILE_START(npp, "NPN_Invoke");
  bool temp = NPN_Invoke(npp, global_object, alert_id, args, 1, &result);
  GLUE_PROFILE_STOP(npp, "NPN_Invoke");
  if (temp) {
    bool retval = NPVARIANT_IS_BOOLEAN(result) && NPVARIANT_TO_BOOLEAN(result);
    GLUE_PROFILE_START(npp, "NPN_ReleaseVariantValue");
    NPN_ReleaseVariantValue(&result);
    GLUE_PROFILE_STOP(npp, "NPN_ReleaseVariantValue");
    return retval;
  } else {
    return false;
  }
}

// Gets the GPU device IDs and name, pops up a dialog box to confirm with the
// user in case we can't get the information.
bool GetGPUDevice(NPP npp, GPUDevice *device) {
#if defined(RENDERER_D3D9)
  // Check GPU vendor using D3D.
  IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
  if (!d3d) {
    O3D_ERROR(GetServiceLocator(npp)) << "Direct3D9 is unavailable";
    return false;
  }
  D3DADAPTER_IDENTIFIER9 identifier;
  HRESULT hr = d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &identifier);
  d3d->Release();
  if (hr != D3D_OK) {
    O3D_ERROR(GetServiceLocator(npp)) << "Unable to get device ID";
    device->vendor_id = 0;
    device->device_id = 0;
    device->name = "Unknown";
    device->driver = "Unknown";
    device->description = "Unknown";
    device->guid = 0;
    return false;
  }
  device->vendor_id = identifier.VendorId;
  device->device_id = identifier.DeviceId;
  device->name = identifier.DeviceName;
  device->driver = identifier.Driver;
  device->description = identifier.Description;
  device->guid = identifier.DeviceIdentifier.Data1;
  return true;
#else
  // TODO: check GL version, blacklisted vendors ?
  device->vendor_id = 0;
  device->device_id = 0;
  device->name = "Unknown";
  device->driver = "Unknown";
  device->description = "Unknown";
  device->guid = 0;
  return true;
#endif
}

// List of "black-listed" GPUs.
//
// A Vendor ID of 0 means end of the list. A device ID of 0 means the entire
// line of devices from this vendor is black-listed.
//
// NOTE: Black-listed GPUs are only for GPUs that have security or stability
// issues. GPUs that are missing required features are handled by the renderer.
static const GPUDevice g_blacklisted_gpus[] = {
  {0, 0, },  // End Marker
};

// Checks various configuration elements:
// - Windows version
// - GPU vendor
// - User agent (browser)
bool CheckConfig(NPP npp) {
  if (!CheckOSVersion(npp)) return false;

  GPUDevice device;
  if (!GetGPUDevice(npp, &device)) return false;
  for (unsigned int i = 0;
        g_blacklisted_gpus[i].vendor_id != 0; ++i) {
    if (device.vendor_id == g_blacklisted_gpus[i].vendor_id &&
        (device.device_id == g_blacklisted_gpus[i].device_id ||
         g_blacklisted_gpus[i].device_id == 0)) {
      O3D_ERROR(GetServiceLocator(npp))
          << "Unsupported GPU device: " + device.name;
      return false;
    }
  }

  {
    std::ifstream blacklist_file;
    std::string error;
    if (!OpenDriverBlacklistFile(&blacklist_file)) {
      // Allow missing blacklist file for now, or else pulse and developer
      // builds [which don't install the file] will fail.
      // TODO: Look into this again for the public release.
      // error = "Failed to open driver blacklist file.\n"
      //    "Can't verify that it's safe to run O3D.";
    } else if (IsDriverBlacklisted(&blacklist_file, device.guid)) {
      if (blacklist_file.fail()) {
        error = "Error reading driver blacklist file.\n"
                "Can't verify that it's safe to run O3D.";
      } else {
        error = "Your driver cannot run O3D safely.";
      }
    }
    if (error.length() && !AskUser(npp, error)) {
      return false;
    }
  }

  {
    std::string error;
    if (!o3d::RendererInstallCheck(&error)) {
      if (error.length()) {
        O3D_ERROR(GetServiceLocator(npp)) << error;
      } else {
        O3D_ERROR(GetServiceLocator(npp))
            << "Could not initialize the graphics driver.";
      }
      return false;
    }
  }

  // Check User agent. Only Firefox, Chrome and IE are supported.
  std::string user_agent = GetUserAgent(npp);
  if (!CheckUserAgent(npp, user_agent)) return false;
  return true;
}
