// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_DESCRIPTORS_H_
#define CHROME_COMMON_CHROME_DESCRIPTORS_H_

// This is a list of global descriptor keys to be used with the
// base::GlobalDescriptors object (see base/global_descriptors_posix.h)
enum {
  kPrimaryIPCChannel = 0,
  kCrashDumpSignal = 1,
  kSandboxIPCChannel = 2,  // http://code.google.com/p/chromium/LinuxSandboxIPC
};

#endif  // CHROME_COMMON_CHROME_DESCRIPTORS_H_
