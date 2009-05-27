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


// This file declares the Win32CBServer class, helper class that runs a command
// buffer server in a separate win32 thread.

#ifndef O3D_CORE_WIN_COMMAND_BUFFER_WIN32_CB_SERVER_H_
#define O3D_CORE_WIN_COMMAND_BUFFER_WIN32_CB_SERVER_H_

#include "core/cross/precompile.h"
#include "command_buffer/common/cross/rpc_imc.h"
#include "command_buffer/client/cross/buffer_sync_proxy.h"
#include "command_buffer/service/win/d3d9/gapi_d3d9.h"

namespace o3d {

// The current Renderer API assumes we connect directly to the window. This
// class creates a command buffer server in a separate thread, and sets up the
// communication socket.
// This code will go away once we fix the API, and provide a separate mechanism
// to connect to the service.
class Win32CBServer {
 public:
  explicit Win32CBServer(HWND window);
  ~Win32CBServer();

  // Gets the (client-side) command buffer interface.
  command_buffer::BufferSyncInterface *GetInterface() { return proxy_.get(); }

 private:
  static DWORD WINAPI ThreadMain(LPVOID param);

  command_buffer::GAPID3D9 gapi_;
  nacl::HtpHandle socket_pair_[2];
  scoped_ptr<command_buffer::IMCSender> imc_sender_;
  scoped_ptr<command_buffer::BufferSyncProxy> proxy_;
  HANDLE thread_;
};

}  // namespace o3d

#endif  // O3D_CORE_WIN_COMMAND_BUFFER_WIN32_CB_SERVER_H_
