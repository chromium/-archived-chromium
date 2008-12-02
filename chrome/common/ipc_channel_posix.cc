// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_channel.h"


namespace IPC {

// TODO(playmobil): implement.

//------------------------------------------------------------------------------

Channel::Channel(const std::wstring& channel_id, Mode mode, Listener* listener) 
    : factory_(this) {
  NOTREACHED();
}

void Channel::Close() {
  NOTREACHED();
}

bool Channel::Send(Message* message) {
  NOTREACHED();
  return false;
}

bool Channel::Connect() {
  NOTREACHED();
  return false;
}
}  // namespace IPC
