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

#include "sandbox/src/crosscall_client.h"
#include "sandbox/src/crosscall_server.h"
#include "sandbox/src/sharedmem_ipc_client.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace sandbox {

// Helper function to make the fake shared memory with some
// basic elements initialized.
IPCControl* MakeChannels(size_t channel_size, size_t total_shared_size,
                         size_t* base_start) {
  // Allocate memory
  char* mem = new char[total_shared_size];
  memset(mem, 0, total_shared_size);

  // Calculate how many channels we can fit in the shared memory.
  total_shared_size -= offsetof(IPCControl, channels);
  size_t channel_count =
    total_shared_size / (sizeof(ChannelControl) + channel_size);

  // Calculate the start of the first channel.
  *base_start = (sizeof(ChannelControl)* channel_count) +
    offsetof(IPCControl, channels);

  // Setup client structure.
  IPCControl* client_control = reinterpret_cast<IPCControl*>(mem);
  client_control->channels_count = channel_count;

  return client_control;
}

TEST(IPCTest, ChannelMaker) {
  size_t channel_start = 0;
  IPCControl* client_control = MakeChannels(12*64, 4096, &channel_start);

  // Test that our testing rig is computing offsets properly. We should have
  // 5 channnels and the offset to the first channel is 108 bytes.
  ASSERT_TRUE(NULL != client_control);
  EXPECT_EQ(5, client_control->channels_count);
  EXPECT_EQ(108, channel_start);

  delete [] reinterpret_cast<char*>(client_control);
}

TEST(IPCTest, ClientLockUnlock) {
  // Make 7 channels of kIPCChannelSize (1kb) each. Test that we lock and
  // unlock channels properly.
  const size_t channel_size = kIPCChannelSize;
  size_t base_start = 0;
  IPCControl* client_control = MakeChannels(channel_size, 4096*2, &base_start);

  for (size_t ix = 0; ix != client_control->channels_count; ++ix) {
    ChannelControl& channel = client_control->channels[ix];
    channel.channel_base = base_start;
    channel.state = kFreeChannel;
    base_start += channel_size;
  }

  char* mem = reinterpret_cast<char*>(client_control);
  SharedMemIPCClient client(mem);

  // Test that we lock the first 3 channels in sequence.
  void* buff0 = client.GetBuffer();
  EXPECT_TRUE(mem + client_control->channels[0].channel_base == buff0);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[1].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[2].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[3].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[4].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[5].state);

  void* buff1 = client.GetBuffer();
  EXPECT_TRUE(mem + client_control->channels[1].channel_base == buff1);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[1].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[2].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[3].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[4].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[5].state);

  void* buff2 = client.GetBuffer();
  EXPECT_TRUE(mem + client_control->channels[2].channel_base == buff2);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[1].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[2].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[3].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[4].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[5].state);

  // Test that we unlock and re-lock the right channel.
  client.FreeBuffer(buff1);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[1].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[2].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[3].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[4].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[5].state);

  void* buff2b = client.GetBuffer();
  EXPECT_TRUE(mem + client_control->channels[1].channel_base == buff2b);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[1].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[2].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[3].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[4].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[5].state);

  client.FreeBuffer(buff0);
  EXPECT_EQ(kFreeChannel, client_control->channels[0].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[1].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[2].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[3].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[4].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[5].state);

  delete [] reinterpret_cast<char*>(client_control);
}

TEST(IPCTest, CrossCallStrPacking) {
  // This test tries the CrossCall object with null and non-null string
  // combination of parameters and verifies that the unpacker can read them
  // properly.
  const size_t channel_size = kIPCChannelSize;
  size_t base_start = 0;
  IPCControl* client_control = MakeChannels(channel_size, 4096*2, &base_start);
  client_control->server_alive = HANDLE(1);

  for (size_t ix = 0; ix != client_control->channels_count; ++ix) {
    ChannelControl& channel = client_control->channels[ix];
    channel.channel_base = base_start;
    channel.state = kFreeChannel;
    channel.ping_event = ::CreateEventW(NULL, FALSE, FALSE, NULL);
    channel.pong_event = ::CreateEventW(NULL, FALSE, TRUE, NULL);
    base_start += channel_size;
  }

  char* mem = reinterpret_cast<char*>(client_control);
  SharedMemIPCClient client(mem);

  CrossCallReturn answer;
  uint32 tag1 = 666;
  const wchar_t text[] = L"98765 - 43210";
  std::wstring copied_text;
  CrossCallParamsEx* actual_params;

  CrossCall(client, tag1, text, &answer);
  actual_params = reinterpret_cast<CrossCallParamsEx*>(client.GetBuffer());
  EXPECT_EQ(1, actual_params->GetParamsCount());
  EXPECT_EQ(tag1, actual_params->GetTag());
  EXPECT_TRUE(actual_params->GetParameterStr(0, &copied_text));
  EXPECT_STREQ(text, copied_text.c_str());

  // Check with an empty string.
  uint32 tag2 = 777;
  const wchar_t* null_text = NULL;
  CrossCall(client, tag2, null_text, &answer);
  actual_params = reinterpret_cast<CrossCallParamsEx*>(client.GetBuffer());
  EXPECT_EQ(1, actual_params->GetParamsCount());
  EXPECT_EQ(tag2, actual_params->GetTag());
  size_t param_size = 1;
  ArgType type = INVALID_TYPE;
  void* param_addr = actual_params->GetRawParameter(0, &param_size, &type);
  EXPECT_TRUE(NULL != param_addr);
  EXPECT_EQ(0, param_size);
  EXPECT_EQ(WCHAR_TYPE, type);
  EXPECT_TRUE(actual_params->GetParameterStr(0, &copied_text));

  uint32 tag3 = 888;
  param_size = 1;
  copied_text.clear();

  // Check with an empty string and a non-empty string.
  CrossCall(client, tag3, null_text, text, &answer);
  actual_params = reinterpret_cast<CrossCallParamsEx*>(client.GetBuffer());
  EXPECT_EQ(2, actual_params->GetParamsCount());
  EXPECT_EQ(tag3, actual_params->GetTag());
  type = INVALID_TYPE;
  param_addr = actual_params->GetRawParameter(0, &param_size, &type);
  EXPECT_TRUE(NULL != param_addr);
  EXPECT_EQ(0, param_size);
  EXPECT_EQ(WCHAR_TYPE, type);
  EXPECT_TRUE(actual_params->GetParameterStr(0, &copied_text));
  EXPECT_TRUE(actual_params->GetParameterStr(1, &copied_text));
  EXPECT_STREQ(text, copied_text.c_str());

  param_size = 1;
  std::wstring copied_text_p0, copied_text_p2;

  const wchar_t text2[] = L"AeFG";
  CrossCall(client, tag1, text2, null_text, text, &answer);
  actual_params = reinterpret_cast<CrossCallParamsEx*>(client.GetBuffer());
  EXPECT_EQ(3, actual_params->GetParamsCount());
  EXPECT_EQ(tag1, actual_params->GetTag());
  EXPECT_TRUE(actual_params->GetParameterStr(0, &copied_text_p0));
  EXPECT_STREQ(text2, copied_text_p0.c_str());
  EXPECT_TRUE(actual_params->GetParameterStr(2, &copied_text_p2));
  EXPECT_STREQ(text, copied_text_p2.c_str());
  type = INVALID_TYPE;
  param_addr = actual_params->GetRawParameter(1, &param_size, &type);
  EXPECT_TRUE(NULL != param_addr);
  EXPECT_EQ(0, param_size);
  EXPECT_EQ(WCHAR_TYPE, type);

  for (size_t ix = 0; ix != client_control->channels_count; ++ix) {
    ChannelControl& channel = client_control->channels[ix];
    ::CloseHandle(channel.ping_event);
    ::CloseHandle(channel.pong_event);
  }
  delete [] reinterpret_cast<char*>(client_control);
}

// This structure is passed to the mock server threads to simulate
// the server side IPC so it has the required kernel objects.
struct ServerEvents {
  HANDLE ping;
  HANDLE pong;
  volatile LONG* state;
  HANDLE mutex;
};

// This is the server thread that quicky answers an IPC and exits.
DWORD WINAPI QuickResponseServer(PVOID param) {
  ServerEvents* events = reinterpret_cast<ServerEvents*>(param);
  DWORD wait_result = 0;
  wait_result = ::WaitForSingleObject(events->ping, INFINITE);
  ::InterlockedExchange(events->state, kAckChannel);
  ::SetEvent(events->pong);
  return wait_result;
}

class CrossCallParamsMock : public CrossCallParams {
 public:
  CrossCallParamsMock(uint32 tag, size_t params_count)
      :  CrossCallParams(tag, params_count) {
  }
 private:
  void* params[4];
};

void FakeOkAnswerInChannel(void* channel) {
  CrossCallReturn* answer = reinterpret_cast<CrossCallReturn*>(channel);
  answer->call_outcome = SBOX_ALL_OK;
}

// Create two threads that will quickly answer IPCs; the first one
// using channel 1 (channel 0 is busy) and one using channel 0. No time-out
// should occur.
TEST(IPCTest, ClientFastServer) {
  const size_t channel_size = kIPCChannelSize;
  size_t base_start = 0;
  IPCControl* client_control = MakeChannels(channel_size, 4096*2, &base_start);

  for (size_t ix = 0; ix != client_control->channels_count; ++ix) {
    ChannelControl& channel = client_control->channels[ix];
    channel.channel_base = base_start;
    channel.state = kFreeChannel;
    channel.ping_event = ::CreateEventW(NULL, FALSE, FALSE, NULL);
    channel.pong_event = ::CreateEventW(NULL, FALSE, FALSE, NULL);
    base_start += channel_size;
  }

  client_control->server_alive = ::CreateMutex(NULL, FALSE, NULL);

  char* mem = reinterpret_cast<char*>(client_control);
  SharedMemIPCClient client(mem);

  ServerEvents events = {0};
  events.ping = client_control->channels[1].ping_event;
  events.pong = client_control->channels[1].pong_event;
  events.state = &client_control->channels[1].state;

  HANDLE t1 = ::CreateThread(NULL, 0, QuickResponseServer, &events, 0, NULL);
  ASSERT_TRUE(NULL != t1);
  ::CloseHandle(t1);

  void* buff0 = client.GetBuffer();
  EXPECT_TRUE(mem + client_control->channels[0].channel_base == buff0);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[1].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[2].state);

  void* buff1 = client.GetBuffer();
  EXPECT_TRUE(mem + client_control->channels[1].channel_base == buff1);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[1].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[2].state);

  EXPECT_EQ(0, client_control->channels[1].ipc_tag);

  uint32 tag = 7654;
  CrossCallReturn answer;
  CrossCallParamsMock* params1 = new(buff1) CrossCallParamsMock(tag, 1);
  FakeOkAnswerInChannel(buff1);

  ResultCode result = client.DoCall(params1, &answer);
  if (SBOX_ERROR_CHANNEL_ERROR != result)
    client.FreeBuffer(buff1);

  EXPECT_TRUE(SBOX_ALL_OK == result);
  EXPECT_EQ(tag, client_control->channels[1].ipc_tag);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[1].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[2].state);

  HANDLE t2 = ::CreateThread(NULL, 0, QuickResponseServer, &events, 0, NULL);
  ASSERT_TRUE(NULL != t2);
  ::CloseHandle(t2);

  client.FreeBuffer(buff0);
  events.ping = client_control->channels[0].ping_event;
  events.pong = client_control->channels[0].pong_event;
  events.state = &client_control->channels[0].state;

  tag = 4567;
  CrossCallParamsMock* params2 = new(buff0) CrossCallParamsMock(tag, 1);
  FakeOkAnswerInChannel(buff0);

  result = client.DoCall(params2, &answer);
  if (SBOX_ERROR_CHANNEL_ERROR != result)
    client.FreeBuffer(buff0);

  EXPECT_TRUE(SBOX_ALL_OK == result);
  EXPECT_EQ(tag, client_control->channels[0].ipc_tag);
  EXPECT_EQ(kFreeChannel, client_control->channels[0].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[1].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[2].state);

  for (size_t ix = 0; ix != client_control->channels_count; ++ix) {
    ChannelControl& channel = client_control->channels[ix];
    ::CloseHandle(channel.ping_event);
    ::CloseHandle(channel.pong_event);
  }

  ::CloseHandle(client_control->server_alive);
  delete [] reinterpret_cast<char*>(client_control);
}

// This is the server thread that very slowly answers an IPC and exits. Note
// that the pong event needs to be signaled twice.
DWORD WINAPI SlowResponseServer(PVOID param) {
  ServerEvents* events = reinterpret_cast<ServerEvents*>(param);
  DWORD wait_result = 0;
  wait_result = ::WaitForSingleObject(events->ping, INFINITE);
  ::Sleep(kIPCWaitTimeOut1 + kIPCWaitTimeOut2 + 200);
  ::InterlockedExchange(events->state, kAckChannel);
  ::SetEvent(events->pong);
  return wait_result;
}

// This thread's job is to keep the mutex locked.
DWORD WINAPI MainServerThread(PVOID param) {
  ServerEvents* events = reinterpret_cast<ServerEvents*>(param);
  DWORD wait_result = 0;
  wait_result = ::WaitForSingleObject(events->mutex, INFINITE);
  Sleep(kIPCWaitTimeOut1 * 20);
  return wait_result;
}

// Creates a server thread that answers the IPC so slow that is guaranteed to
// trigger the time-out code path in the client. A second thread is created
// to hold locked the server_alive mutex: this signals the client that the
// server is not dead and it retries the wait.
TEST(IPCTest, ClientSlowServer) {
  const size_t channel_size = kIPCChannelSize;
  size_t base_start = 0;
  IPCControl* client_control = MakeChannels(channel_size, 4096*2, &base_start);

  for (size_t ix = 0; ix != client_control->channels_count; ++ix) {
    ChannelControl& channel = client_control->channels[ix];
    channel.channel_base = base_start;
    channel.state = kFreeChannel;
    channel.ping_event = ::CreateEventW(NULL, FALSE, FALSE, NULL);
    channel.pong_event = ::CreateEventW(NULL, FALSE, FALSE, NULL);
    base_start += channel_size;
  }

  client_control->server_alive = ::CreateMutex(NULL, FALSE, NULL);

  char* mem = reinterpret_cast<char*>(client_control);
  SharedMemIPCClient client(mem);

  ServerEvents events = {0};
  events.ping = client_control->channels[0].ping_event;
  events.pong = client_control->channels[0].pong_event;
  events.state = &client_control->channels[0].state;

  HANDLE t1 = ::CreateThread(NULL, 0, SlowResponseServer, &events, 0, NULL);
  ASSERT_TRUE(NULL != t1);
  ::CloseHandle(t1);

  ServerEvents events2 = {0};
  events2.pong = events.pong;
  events2.mutex = client_control->server_alive;

  HANDLE t2 = ::CreateThread(NULL, 0, MainServerThread, &events2, 0, NULL);
  ASSERT_TRUE(NULL != t2);
  ::CloseHandle(t2);

  ::Sleep(1);

  void* buff0 = client.GetBuffer();
  uint32 tag = 4321;
  CrossCallReturn answer;
  CrossCallParamsMock* params1 = new(buff0) CrossCallParamsMock(tag, 1);
  FakeOkAnswerInChannel(buff0);

  ResultCode result = client.DoCall(params1, &answer);
  if (SBOX_ERROR_CHANNEL_ERROR != result)
    client.FreeBuffer(buff0);

  EXPECT_TRUE(SBOX_ALL_OK == result);
  EXPECT_EQ(tag, client_control->channels[0].ipc_tag);
  EXPECT_EQ(kFreeChannel, client_control->channels[0].state);

  for (size_t ix = 0; ix != client_control->channels_count; ++ix) {
    ChannelControl& channel = client_control->channels[ix];
    ::CloseHandle(channel.ping_event);
    ::CloseHandle(channel.pong_event);
  }
  ::CloseHandle(client_control->server_alive);
  delete [] reinterpret_cast<char*>(client_control);
}

}  // namespace sandbox
