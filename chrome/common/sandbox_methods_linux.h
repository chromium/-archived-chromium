// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SANDBOX_METHODS_LINUX_H_
#define CHROME_COMMON_SANDBOX_METHODS_LINUX_H_

// This is a list of sandbox IPC methods which the renderer may send to the
// sandbox host. See http://code.google.com/p/chromium/LinuxSandboxIPC
// This isn't the full list, values < 32 are reserved for methods called from
// Skia.
class LinuxSandbox {
 public:
  enum Methods {
    METHOD_GET_FONT_FAMILY_FOR_CHARS = 32,
  };
};

#endif  // CHROME_COMMON_SANDBOX_METHODS_LINUX_H_
