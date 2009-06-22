// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_CHANNEL_HANDLE_H_
#define CHROME_COMMON_IPC_CHANNEL_HANDLE_H_

#include "build/build_config.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

// On Windows, any process can create an IPC channel and others can fetch
// it by name.  We pass around the channel names over IPC.
// On POSIX, we instead pass around handles to channel endpoints via IPC.
// When it's time to IPC a new channel endpoint around, we send both the
// channel name as well as a base::FileDescriptor, which is itself a special
// type that knows how to copy a socket endpoint over IPC.
//
// In sum, when passing a handle to a channel over IPC, use this data structure
// to work on both Windows and POSIX.

namespace IPC {

struct ChannelHandle {
  // Note that serialization for this object is defined in the ParamTraits
  // template specialization in ipc_message_utils.h.
  std::string name;
#if defined(OS_POSIX)
  base::FileDescriptor socket;
#endif

  ChannelHandle() {}
#if defined(OS_POSIX)
  ChannelHandle(const std::string& n, const base::FileDescriptor& s)
      : name(n), socket(s) {}
#else
  ChannelHandle(const std::string& n) : name(n) {}
#endif
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_HANDLE_H_
