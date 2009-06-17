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


// This file contains a "big" test of the whole command buffer service, making
// sure all the pieces fit together.
//
// Currently this checks that the RPC mechanism properly forwards call to the
// service thread.

#include <build/build_config.h>
#include "command_buffer/common/cross/rpc_imc.h"
#include "command_buffer/service/cross/big_test_helpers.h"
#include "command_buffer/service/cross/buffer_rpc.h"
#include "command_buffer/service/cross/cmd_buffer_engine.h"
#include "command_buffer/service/cross/gapi_decoder.h"
#include "third_party/native_client/googleclient/native_client/src/trusted/desc/nrd_all_modules.h"

namespace o3d {
namespace command_buffer {

nacl::SocketAddress g_address = { "command-buffer" };

// Main function. Creates a socket, and waits for an incoming connection on it.
// Then run the engine main loop.
void BigTest() {
  NaClNrdAllModulesInit();
  GAPIDecoder decoder(g_gapi);
  CommandBufferEngine engine(&decoder);
  decoder.set_engine(&engine);
  nacl::Handle server_socket = nacl::BoundSocket(&g_address);
  nacl::Handle handles[1];
  nacl::MessageHeader msg;
  msg.iov = NULL;
  msg.iov_length = 0;
  msg.handles = handles;
  msg.handle_count = 1;
  int r = nacl::ReceiveDatagram(server_socket, &msg, 0);
  DCHECK_NE(r, -1);
  nacl::Close(server_socket);

  nacl::HtpHandle htp_handle = nacl::CreateImcDesc(handles[0]);
  IMCMessageProcessor processor(htp_handle, engine.rpc_impl());
  engine.set_process_interface(&processor);
  IMCSender sender(htp_handle);
  engine.set_client_rpc(&sender);

  bool result = g_gapi->Initialize();
  DCHECK(result);

  bool running = true;
  while (running) {
    running = ProcessSystemMessages();
    if (!running) break;
    // DoWork() will block if there is nothing to be done, meaning we are only
    // going to handle message after commands are sent. It should happen at
    // least once a frame, so it's "good enough".
    // TODO: figure out a way to wait on the socket OR messages with
    // MsgWaitForMultipleObjects. Asynchronous ("overlapped") read on the
    // socket may let us do that on windows.
    running = engine.DoWork();
  }
  g_gapi->Destroy();
  nacl::Close(htp_handle);
  NaClNrdAllModulesFini();
}

}  // namespace command_buffer
}  // namespace o3d

#ifdef OS_WIN
int big_test_main(int argc, wchar_t **argv) {
#else
int big_test_main(int argc, char **argv) {
#endif
  o3d::command_buffer::BigTest();
  return 0;
}
