// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RESERVED_FILE_DESCRIPTORS_H_
#define BASE_RESERVED_FILE_DESCRIPTORS_H_

#if defined(OS_POSIX)

// Chrome uses predefined file descriptors to communicate with child processes.
// Normally this is a private contract between code that does fork/exec and the
// code it invokes, but in zygote mode, things get a little more interesting.
// It's a huge layering violation for this to be in base, but
// logging and ZygoteManager need kReservedFileDescriptors, so there.

enum GlobalReservedFds {
  // Classic unix file descriptors.
  // Let's leave them alone even if we don't use them.
  kStdinFd = 0,
  kStdoutFd = 1,
  kStderrFd = 2,

  // See chrome/common/ipc_channel_posix.cc
  kClientChannelFd = 3,

  // See chrome/app/breakpad_linux.cc and
  // chrome/browser/renderer_host/browser_render_process_host.cc
  kMagicCrashSignalFd = 4,

  // One plus highest fd mentioned in this enum.
  kReservedFds = 5
};

#endif

#endif
