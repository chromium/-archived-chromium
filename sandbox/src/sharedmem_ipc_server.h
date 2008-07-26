// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SANDBOX_SRC_SHAREDMEM_IPC_SERVER_H__
#define SANDBOX_SRC_SHAREDMEM_IPC_SERVER_H__

#include <list>

#include "base/basictypes.h"
#include "sandbox/src/crosscall_params.h"
#include "sandbox/src/crosscall_server.h"
#include "sandbox/src/sharedmem_ipc_client.h"

// IPC transport implementation that uses shared memory.
// This is the server side
//
// The server side has knowledge about the layout of the shared memory
// and the state transitions. Both are explained in sharedmem_ipc_client.h
//
// As opposed to SharedMemIPClient, the Server object should be one for the
// entire lifetime of the target process. The server is in charge of creating
// the events (ping, pong) both for the client and for the target that are used
// to signal the IPC and also in charge of setting the initial state of the
// channels.
//
// When an IPC is ready, the server relies on being called by on the
// ThreadPingEventReady callback. The IPC server then retrieves the buffer,
// marshals it into a CrossCallParam object and calls the Dispatcher, who is in
// charge of fulfilling the IPC request.
namespace sandbox {

// the shared memory implementation of the IPC server. There should be one
// of these objects per target (IPC client) process
class SharedMemIPCServer {
 public:
  // Creates the IPC server.
  // target_process: handle to the target process. It must be suspended.
  // target_process_id: process id of the target process.
  // target_job: the job object handle associated with the target process.
  // thread_provider: a thread provider object.
  // dispatcher: an object that can service IPC calls.
  SharedMemIPCServer(HANDLE target_process, DWORD target_process_id,
                     HANDLE target_job, ThreadProvider* thread_provider,
                     Dispatcher* dispatcher);

  ~SharedMemIPCServer();

  // Initializes the server structures, shared memory structures and
  // creates the kernels events used to signal the IPC.
  bool Init(void* shared_mem, size_t shared_size, size_t channel_size);

 private:
  // When an event fires (IPC request). A thread from the ThreadProvider
  // will call this function. The context parameter should be the same as
  // provided when ThreadProvider::RegisterWait was called.
  static void __stdcall ThreadPingEventReady(void* context,
                                             unsigned char);

  // Makes the client and server events. This function is called once
  // per channel.
  bool MakeEvents(HANDLE* server_ping, HANDLE* server_pong,
                  HANDLE* client_ping, HANDLE* client_pong);

  // A copy this structure is maintained per channel.
  // Note that a lot of the fields are just the same of what we have in the IPC
  // object itself. It is better to have the copies since we can dispatch in the
  // static method without worrying about converting back to a member function
  // call or about threading issues.
  struct ServerControl {
    // This channel server ping event.
    HANDLE ping_event;
    // This channel server pong event.
    HANDLE pong_event;
    // The size of this channel.
    size_t channel_size;
    // The pointer to the actual channel data.
    char* channel_buffer;
    // The pointer to the base of the shared memory.
    char* shared_base;
    // A pointer to this channel's client-side control structure this structure
    // lives in the shared memory.
    ChannelControl* channel;
    // the IPC dispatcher associated with this channel.
    Dispatcher* dispatcher;
    // The target process information associated with this channel.
    ClientInfo target_info;
  };

  // Looks for the appropriate handler for this IPC and invokes it.
  static bool InvokeCallback(const ServerControl* service_context,
                             void* ipc_buffer, CrossCallReturn* call_result);

  // Points to the shared memory channel control which lives at
  // the start of the shared section.
  IPCControl* client_control_;

  // Keeps track of the server side objects that are used to answer an IPC.
  typedef std::list<ServerControl*> ServerContexts;
  ServerContexts server_contexts_;

  // The thread provider provides the threads that call back into this object
  // when the IPC events fire.
  ThreadProvider* thread_provider_;

  // The IPC object is associated with a target process.
  HANDLE target_process_;

  // The target process id associated with the IPC object.
  DWORD target_process_id_;

  // The target object is inside a job too.
  HANDLE target_job_object_;

  // The dispatcher handles 'ready' IPC calls.
  Dispatcher* call_dispatcher_;

  DISALLOW_EVIL_CONSTRUCTORS(SharedMemIPCServer);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SHAREDMEM_IPC_SERVER_H__
