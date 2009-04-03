// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_switches.h"

namespace switches {

// On POSIX only: use FIFO for IPC channels so that "unrelated" process
// can connect to a channel, provided it knows its name. For debugging purposes.
const wchar_t kIPCUseFIFO[]                    = L"ipc-use-fifo";

// The value of this switch tells the child process which
// IPC channel the browser expects to use to communicate with it.
const wchar_t kProcessChannelID[]              = L"channel";

}  // namespace switches
