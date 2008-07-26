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

#ifndef SANDBOX_SRC_SHAREDMEM_IPC_CLIENT_H__
#define SANDBOX_SRC_SHAREDMEM_IPC_CLIENT_H__

#include "sandbox/src/crosscall_params.h"
#include "sandbox/src/sandbox.h"

// IPC transport implementation that uses shared memory.
// This is the client side
//
// The shared memory is divided on blocks called channels, and potentially
// it can perform as many concurrent IPC calls as channels. The IPC over
// each channel is strictly synchronous for the client.
//
// Each channel as a channel control section associated with. Each control
// section has two kernel events (known as ping and pong) and a integer
// variable that maintains a state
//
// this is the state diagram of a channel:
//
//                   locked                in service
//     kFreeChannel---------->BusyChannel-------------->kAckChannel
//          ^                                                 |
//          |_________________________________________________|
//                             answer ready
//
// The protocol is as follows:
//   1) client finds a free channel: state = kFreeChannel
//   2) does an atomic compare-and-swap, now state = BusyChannel
//   3) client writes the data into the channel buffer
//   4) client signals the ping event and waits (blocks) on the pong event
//   5) eventually the server signals the pong event
//   6) the client awakes and reads the answer from the same channel
//   7) the client updates its InOut parameters with the new data from the
//      shared memory section.
//   8) the client atomically sets the state = kFreeChannel
//
//  In the shared memory the layout is as follows:
//
//    [ channel count    ]
//    [ channel control 0]
//    [ channel control 1]
//    [ channel control N]
//    [ channel buffer 0 ] 1024 bytes
//    [ channel buffer 1 ] 1024 bytes
//    [ channel buffer N ] 1024 bytes
//
// By default each channel buffer is 1024 bytes
namespace sandbox {

// the possible channel states as described above
enum ChannelState {
  // channel is free
  kFreeChannel = 1,
  // IPC in progress client side
  kBusyChannel,
  // IPC in progress server side
  kAckChannel,
  // not used right now
  kReadyChannel,
  // IPC abandoned by client side
  kAbandonnedChannel
};

// The next two constants control the time outs for the IPC.
const DWORD kIPCWaitTimeOut1 = 1000;   // Milliseconds.
const DWORD kIPCWaitTimeOut2 =   50;   // Milliseconds.

// the channel control structure
struct ChannelControl {
  // points to be beginning of the channel buffer, where data goes
  size_t channel_base;
  // maintains the state from the ChannelState enumeration
  volatile LONG state;
  // the ping event is signaled by the client when the IPC data is ready on
  // the buffer
  HANDLE ping_event;
  // the client waits on the pong event for the IPC answer back
  HANDLE pong_event;
  // the IPC unique identifier
  uint32 ipc_tag;
};

struct IPCControl {
  // total number of channels available, some might be busy at a given time
  size_t channels_count;
  // handle to a shared mutex to detect when the server is dead
  HANDLE server_alive;
  // array of channel control structures
  ChannelControl channels[1];
};

// the actual shared memory IPC implementation class. This object is designed
// to be lightweight so it can be constructed on-site (at the calling place)
// wherever an IPC call is needed.
class SharedMemIPCClient {
 public:
  // Creates the IPC client.
  // as parameter it takes the base address of the shared memory
  explicit SharedMemIPCClient(void* shared_mem);

  // locks a free channel and returns the channel buffer memory base. This call
  // blocks until there is a free channel
  void* GetBuffer();

  // releases the lock on the channel, for other to use. call this if you have
  // called GetBuffer and you want to abort but have not called yet DoCall()
  void FreeBuffer(void* buffer);

  // Performs the actual IPC call.
  // params: The blob of packed input parameters.
  // answer: upon IPC completion, it contains the server answer to the IPC.
  // If the return value is not SBOX_ERROR_CHANNEL_ERROR, the caller has to free
  // the channel.
  // returns ALL_OK if the IPC mechanism successfully delivered. You still need
  // to check on the answer structure to see the actual IPC result.
  ResultCode DoCall(CrossCallParams* params, CrossCallReturn* answer);

 private:
  // Returns the index of the first free channel. It sets 'severe_failure'
  // to true if there is an unrecoverable error that does not allow to
  // find a channel.
  size_t LockFreeChannel(bool* severe_failure);
  // Return the channel index given the address of the buffer.
  size_t ChannelIndexFromBuffer(const void* buffer);
  IPCControl* control_;
  // point to the first channel base
  char* first_base_;
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SHAREDMEM_IPC_CLIENT_H__
