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
// - OS version
// - GPU vendor

#include <IOKit/IOKitLib.h>

#include <iostream>
#include <fstream>

#include "plugin/cross/config.h"
#include "plugin/cross/plugin_metrics.h"
#include "plugin_mac.h"

namespace o3d {

// Trivial little functions to check for the OS version boundaries we care about
// and keep the result cached so they are cheap to call repeatedly.


// Returns whether OS is 10.4 (Tiger) or higher.
static bool IsMacOSTenFourOrHigher() {
  static bool isCached = false, result = false;
  if (!isCached) {
    SInt32 major = 0, minor = 0;
    // These selectors don't exist pre 10.4 but as we check the error
    // the function will correctly return NO which is the right answer.
    result = ((Gestalt(gestaltSystemVersionMajor,  &major) == noErr) &&
              (Gestalt(gestaltSystemVersionMinor,  &minor) == noErr) &&
              ((major > 10) || (major == 10 && minor >= 4)));
    isCached = true;
  }
  return result;
}


// Checks OS version
bool CheckOSVersion(NPP npp) {
  // TODO: turn this back on when ready.
  if (!IsMacOSTenFourOrHigher()) {
    std::string error =
        std::string("Unsupported Mac OS X version. 10.4 is required.");
    return AskUser(npp, error);
  }
  return true;
}

bool CheckUserAgent(NPP npp, const std::string &user_agent) {
  return true;
}

bool OpenDriverBlacklistFile(std::ifstream *input_file) {
  // TODO:
  return false;
}


static CFTypeRef SearchPortForProperty(io_registry_entry_t dspPort,
                                       CFStringRef propertyName) {
  return IORegistryEntrySearchCFProperty(dspPort,
                                         kIOServicePlane,
                                         propertyName,
                                         kCFAllocatorDefault,
                                         kIORegistryIterateRecursively |
                                         kIORegistryIterateParents);
}


static void CFReleaseIf(CFTypeRef d) {
  if (d)
    CFRelease(d);
}


static UInt32 IntValueOfCFData(CFDataRef d) {
  UInt32 value = 0;

  if (d) {
    const UInt32 *vp = reinterpret_cast<const UInt32*>(CFDataGetBytePtr(d));
    if (vp != NULL)
      value = *vp;
  }

  return value;
}


static int IntValueOfCFNumber(CFNumberRef n) {
  int value = 0;

  if ((n != NULL) && (CFGetTypeID(n) == CFNumberGetTypeID()))
    CFNumberGetValue(n, kCFNumberSInt32Type, &value);

  return value;
}


static void GetVideoCardInfo(CGDirectDisplayID displayID,
                             int *vendorID,
                             int *deviceID,
                             int *vramSize) {
  io_registry_entry_t dspPort = CGDisplayIOServicePort(displayID);

  CFTypeRef vendorIDRef = SearchPortForProperty(dspPort, CFSTR("vendor-id"));
  if (vendorID) *vendorID = IntValueOfCFData((CFDataRef)vendorIDRef);

  CFTypeRef deviceIDRef = SearchPortForProperty(dspPort, CFSTR("device-id"));
  if (deviceID) *deviceID = IntValueOfCFData((CFDataRef)deviceIDRef);

  CFTypeRef typeCodeRef = SearchPortForProperty(dspPort,
                                                CFSTR(kIOFBMemorySizeKey));
  if (vramSize) *vramSize = IntValueOfCFNumber((CFNumberRef)typeCodeRef);

  CFReleaseIf(vendorIDRef);
  CFReleaseIf(deviceIDRef);
  CFReleaseIf(typeCodeRef);
}

struct GPUInfo {
  int vendorID;
  int deviceID;
};

// A list of GPUs which we know will not work well in o3d
// We want to fallback to using the software render for these
GPUInfo softwareRenderList[] = {
  {0x8086, 0x2a02},  // Intel GMA X3100  Macbook
  {0x8086, 0x27a2}   // Intel GMA 950    Mac Mini
};

bool UseSoftwareRenderer() {
  static bool use_software_renderer = false;
  static bool is_initialized = false;

  if (!is_initialized) {
    int vendorID;
    int deviceID;
    GetVideoCardInfo(kCGDirectMainDisplay,
                     &vendorID,
                     &deviceID,
                     NULL);

    use_software_renderer = false;
    for (int i = 0; i < arraysize(softwareRenderList); ++i) {
      GPUInfo &softwareRenderInfo = softwareRenderList[i];
      if (vendorID == softwareRenderInfo.vendorID
        && deviceID == softwareRenderInfo.deviceID) {
        use_software_renderer = true;
        break;
      }
    }
    is_initialized = true;
  }

  return use_software_renderer;
}

static bool GetVideoCardMetrics(CGDirectDisplayID displayID) {
  int vendorID;
  int deviceID;
  int vramSize;
  GetVideoCardInfo(displayID,
                   &vendorID,
                   &deviceID,
                   &vramSize);

  o3d::metric_gpu_vendor_id = vendorID;
  o3d::metric_gpu_device_id = deviceID;
  o3d::metric_gpu_vram_size = vramSize;

  return true;
}


// Return a pointer to the last character with value c in string s.
// Returns NULL if c is not found.
static char* FindLastChar(char *s, char c) {
  char *s_found = NULL;

  while (*s != '\0') {
    if (*s == c)
      s_found = s;
    s++;
  }

  return s_found;
}


bool GetOpenGLMetrics() {
  char *gl_version_string = (char*)glGetString(GL_VERSION);
  char *gl_extensions_string = (char*)glGetString(GL_EXTENSIONS);

  if ((gl_version_string == NULL) || (gl_extensions_string == NULL))
    return false;

  // Get the OpenGL version from the start of the string.
  int gl_major = 0, gl_minor = 0;
  sscanf(gl_version_string, "%u.%u",  &gl_major, &gl_minor);
  o3d::metric_gl_major_version = gl_major;
  o3d::metric_gl_minor_version = gl_minor;

  // Get the OpenGL driver version.
  // This bit is Mac specific - Mac OpenGL drivers have the driver version
  // at the end of the gl version string preceded by a dash.
  char *s = FindLastChar(gl_version_string, '-');
  if (s) {
    char *driver_string = s + 1;  // skip '-'
    int driver_major = 0, driver_minor = 0, driver_bugfix = 0;
    sscanf(driver_string, "%u.%u.%u",
           &driver_major, &driver_minor, &driver_bugfix);
    o3d::metric_gpu_driver_major_version = driver_major;
    o3d::metric_gpu_driver_minor_version = driver_minor;
    o3d::metric_gpu_driver_bugfix_version = driver_bugfix;
  }

  // Get the HLSL version.
  // On OpenGL 1.x it's 1.0 if the GL_ARB_shading_language_100 extension is
  // present.
  // On OpenGL 2.x  it's a matter of getting the GL_SHADING_LANGUAGE_VERSION
  // string.
  int gl_hlsl_major = 0, gl_hlsl_minor = 0;
  if ((gl_major == 1) &&
      strstr(gl_extensions_string, "GL_ARB_shading_language_100")) {
    gl_hlsl_major = 1;
    gl_hlsl_minor = 0;
  } else if (gl_major >= 2) {
    char* glsl_version_string = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    if (glsl_version_string) {
      sscanf(glsl_version_string, "%u.%u", &gl_hlsl_major, &gl_hlsl_minor);
    }
  }
  o3d::metric_gl_hlsl_major_version = gl_hlsl_major;
  o3d::metric_gl_hlsl_minor_version = gl_hlsl_minor;

  return true;
}


bool GetUserConfigMetrics() {
  // Check Mac version.
  o3d::metric_system_type = o3d::SYSTEM_NAME_MAC;

  SInt32 major = 0, minor = 0, bugfix = 0;

  Gestalt(gestaltSystemVersionMajor, &major);
  Gestalt(gestaltSystemVersionMinor, &minor);
  Gestalt(gestaltSystemVersionBugFix, &bugfix);

  o3d::metric_mac_major_version = major;    // eg 10
  o3d::metric_mac_minor_version = minor;    // eg 5
  o3d::metric_mac_bugfix_version = bugfix;  // eg 6

  o3d::metric_direct3d_available.Set(false);

  GetVideoCardMetrics(kCGDirectMainDisplay);

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
  // The OmniWeb user_agent also contains Safari. Search for OminWeb first.
  } else if (std::string::npos != user_agent.find("OmniWeb")) {
    o3d::metric_browser_type = o3d::BROWSER_NAME_OMNIWEB;
  // now we can safely look for Safari
  } else if (std::string::npos != user_agent.find("Safari")) {
    o3d::metric_browser_type = o3d::BROWSER_NAME_SAFARI;
  } else if (std::string::npos != user_agent.find("Opera")) {
    o3d::metric_browser_type = o3d::BROWSER_NAME_OPERA;
  } else if (std::string::npos != user_agent.find("Firefox")) {
    o3d::metric_browser_type = o3d::BROWSER_NAME_FIREFOX;
  } else if (std::string::npos != user_agent.find("MSIE")) {
    o3d::metric_browser_type = o3d::BROWSER_NAME_MSIE;
  } else if (std::string::npos != user_agent.find("Camino")) {
    o3d::metric_browser_type = o3d::BROWSER_NAME_CAMINO;
  } else {
    o3d::metric_browser_type = o3d::BROWSER_NAME_UNKNOWN;
  }

  int browser_major = 0, browser_minor = 0, browser_bugfix = 0;
  if (GetBrowserVersionInfo(&browser_major, &browser_minor, &browser_bugfix)) {
    o3d::metric_browser_major_version = browser_major;
    o3d::metric_browser_minor_version = browser_minor;
    o3d::metric_browser_bugfix_version = browser_bugfix;
  }

  return true;
}

}  // namespace o3d
