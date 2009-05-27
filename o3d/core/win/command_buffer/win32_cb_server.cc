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


// This file contains the implementation of the Win32CBServer class.

#include "core/cross/precompile.h"
#include "core/win/command_buffer/win32_cb_server.h"
#include "command_buffer/service/cross/buffer_rpc.h"
#include "command_buffer/service/cross/cmd_buffer_engine.h"
#include "command_buffer/service/cross/gapi_decoder.h"

namespace o3d {

using command_buffer::GAPIDecoder;
using command_buffer::IMCSender;
using command_buffer::IMCMessageProcessor;
using command_buffer::BufferSyncProxy;
using command_buffer::CommandBufferEngine;

Win32CBServer::Win32CBServer(HWND window)
    : gapi_(),
      proxy_(NULL),
      imc_sender_(NULL),
      thread_(NULL) {
  gapi_.set_hwnd(window);

  nacl::Handle handles[2];
  nacl::SocketPair(handles);

  socket_pair_[0] = nacl::CreateImcDesc(handles[0]);
  socket_pair_[1] = nacl::CreateImcDesc(handles[1]);
  imc_sender_.reset(new IMCSender(socket_pair_[0]));
  proxy_.reset(new BufferSyncProxy(imc_sender_.get()));

  thread_ = ::CreateThread(NULL, 0, ThreadMain, this, 0, NULL);
}

Win32CBServer::~Win32CBServer() {
  imc_sender_->SendCall(command_buffer::POISONED_MESSAGE_ID, NULL, 0, NULL,
                        0);
  ::WaitForSingleObject(thread_, INFINITE);
  ::CloseHandle(thread_);

  nacl::Close(socket_pair_[0]);
  nacl::Close(socket_pair_[1]);
}

DWORD WINAPI Win32CBServer::ThreadMain(LPVOID param) {
  Win32CBServer *server = static_cast<Win32CBServer *>(param);

  scoped_ptr<GAPIDecoder> decoder(new GAPIDecoder(&server->gapi_));
  scoped_ptr<CommandBufferEngine> engine(
      new CommandBufferEngine(decoder.get()));
  decoder->set_engine(engine.get());

  IMCMessageProcessor processor(server->socket_pair_[1], engine->rpc_impl());
  engine->set_process_interface(&processor);
  IMCSender sender(server->socket_pair_[1]);
  engine->set_client_rpc(&sender);

  server->gapi_.Initialize();
  bool running = true;
  while (running) {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (msg.message == WM_QUIT) {
        running = false;
        break;
      }
    }
    if (!running) break;
    // DoWork() will block if there is nothing to be done, meaning we are only
    // going to handle message after commands are sent. It should happen at
    // least once a frame, so it's OK.
    // TODO: figure out a way to wait on the socket OR messages with
    // MsgWaitForMultipleObjects. Asynchronous ("overlapped") read on the
    // socket may let us do that.
    running = engine->DoWork();
  }
  server->gapi_.Destroy();

  return 0;
}

}  // namespace o3d
