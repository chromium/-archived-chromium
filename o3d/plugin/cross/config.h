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


// This file defines a few functions to check the user's hardware and software
// configuration.

#ifndef O3D_PLUGIN_CROSS_CONFIG_H_
#define O3D_PLUGIN_CROSS_CONFIG_H_

#include <fstream>
#include <string>
#include "plugin/cross/o3d_glue.h"

namespace o3d {

// Returns the user agent string.
// Arguments:
//   npp: plugin instance.
// Returns:
//   The user agent string.
std::string GetUserAgent(NPP npp);

struct GPUDevice {
  unsigned int vendor_id;
  unsigned int device_id;
  std::string driver;
  std::string description;
  std::string name;
  unsigned int guid;
};

// Asks the user to ok a continuation
// Arguments:
//   npp: plugin instance.
//   error: the error to signal.
// Returns:
//   true if the error was overridden by the user, false otherwise.
bool AskUser(NPP npp, const std::string &error);

// Gets the device information.
// Arguments:
//   device: a GPUDevice structure that will contain the vendor and device IDs.
//   name: a pointer to a string that will contain the device string.
// Returns:
//   true if successful. If not, device and name are not modified.
bool GetGPUDevice(NPP npp, GPUDevice *device);

// Checks that the current hardware and software configuration is supported,
// prompting the user to give him and option to run anyway.
// Arguments:
//   npp: plugin instance.
// Returns:
//   Whether to allow the plug-in to run.
bool CheckConfig(NPP npp);

// The following functions are platform-dependent, and are implemented in the
// respective {platform}/config.cc files

// Checks the operating system version.
bool CheckOSVersion(NPP npp);

// Checks the user agent string.
bool CheckUserAgent(NPP npp, const std::string &user_agent);

// Used to get the text file that lists blacklisted driver GUIDs.
// Returns true on success.
bool OpenDriverBlacklistFile(std::ifstream *input_file);

// Checks the driver GUID against the blacklist file.  Returns true if there's a
// match [this driver is blacklisted] or if an IO error occurred.  Check the
// state of input_file to determine which it was.
// Note that this function always returns false if the guid is 0, since it will
// be 0 if we had a failure in reading it, and the user will already have been
// warned.
bool IsDriverBlacklisted(std::ifstream *input_file, unsigned int guid);

// Checks the current hardware, software configurations and puts the values
// into metrics.
bool GetUserConfigMetrics();

// Checks the browser version.
// Arguments:
//   npp: plugin instance.
bool GetUserAgentMetrics(NPP npp);

bool GetOpenGLMetrics();

}  // namespace o3d

#endif  // O3D_PLUGIN_CROSS_CONFIG_H_
