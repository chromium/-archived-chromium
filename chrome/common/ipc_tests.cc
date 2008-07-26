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

#include <windows.h>
#include <stdio.h>
#include <iostream>
#include <string>

#include "chrome/common/ipc_tests.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug_on_start.h"
#include "base/perftimer.h"
#include "base/process_util.h"
#include "base/thread.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/common/ipc_message_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Define to enable IPC performance testing instead of the regular unit tests
// #define PERFORMANCE_TEST

const wchar_t kTestClientChannel[] = L"T1";
const wchar_t kReflectorChannel[] = L"T2";
const wchar_t kFuzzerChannel[] = L"F3";

const wchar_t kChild[] = L"child";
const wchar_t kReflector[] = L"reflector";
const wchar_t kFuzzer[] = L"fuzzer";

#ifndef PERFORMANCE_TEST

TEST(IPCChannelTest, BasicMessageTest) {
  int v1 = 10;
  std::string v2("foobar");
  std::wstring v3(L"hello world");

  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  EXPECT_TRUE(m.WriteInt(v1));
  EXPECT_TRUE(m.WriteString(v2));
  EXPECT_TRUE(m.WriteWString(v3));

  void* iter = NULL;

  int vi;
  std::string vs;
  std::wstring vw;

  EXPECT_TRUE(m.ReadInt(&iter, &vi));
  EXPECT_EQ(v1, vi);

  EXPECT_TRUE(m.ReadString(&iter, &vs));
  EXPECT_EQ(v2, vs);

  EXPECT_TRUE(m.ReadWString(&iter, &vw));
  EXPECT_EQ(v3, vw);

  // should fail
  EXPECT_FALSE(m.ReadInt(&iter, &vi));
  EXPECT_FALSE(m.ReadString(&iter, &vs));
  EXPECT_FALSE(m.ReadWString(&iter, &vw));
}

static void Send(IPC::Message::Sender* sender, const char* text) {
  static int message_index = 0;

  IPC::Message* message = new IPC::Message(0,
                                           2,
                                           IPC::Message::PRIORITY_NORMAL);
  message->WriteInt(message_index++);
  message->WriteString(std::string(text));

  // make sure we can handle large messages
  char junk[50000];
  junk[sizeof(junk)-1] = 0;
  message->WriteString(std::string(junk));

  // DEBUG: printf("[%u] sending message [%s]\n", GetCurrentProcessId(), text);
  sender->Send(message);
}

class MyChannelListener : public IPC::Channel::Listener {
 public:
  virtual void OnMessageReceived(const IPC::Message& message) {
    IPC::MessageIterator iter(message);

    int index = iter.NextInt();
    const std::string data = iter.NextString();

    if (--messages_left_ == 0) {
      MessageLoop::current()->Quit();
    } else {
      Send(sender_, "Foo");
    }
  }

  void Init(IPC::Message::Sender* s) {
    sender_ = s;
    messages_left_ = 50;
  }

 private:
  IPC::Message::Sender* sender_;
  int messages_left_;
};
static MyChannelListener channel_listener;

TEST(IPCChannelTest, ChannelTest) {
  // setup IPC channel
  IPC::Channel chan(kTestClientChannel, IPC::Channel::MODE_SERVER,
                    &channel_listener);
  chan.Connect();

  channel_listener.Init(&chan);

  HANDLE process_handle = SpawnChild(TEST_CLIENT);
  ASSERT_TRUE(process_handle);

  Send(&chan, "hello from parent");

  // run message loop
  MessageLoop::current()->Run();

  // cleanup child process
  WaitForSingleObject(process_handle, 5000);
  CloseHandle(process_handle);
}

TEST(IPCChannelTest, ChannelProxyTest) {
  // The thread needs to out-live the ChannelProxy.
  Thread thread("ChannelProxyTestServer");
  thread.Start();
  {
    // setup IPC channel proxy
    IPC::ChannelProxy chan(kTestClientChannel, IPC::Channel::MODE_SERVER,
                           &channel_listener, NULL, thread.message_loop());

    channel_listener.Init(&chan);

    HANDLE process_handle = SpawnChild(TEST_CLIENT);
    ASSERT_TRUE(process_handle);

    Send(&chan, "hello from parent");

    // run message loop
    MessageLoop::current()->Run();

    // cleanup child process
    WaitForSingleObject(process_handle, 5000);
    CloseHandle(process_handle);
  }
  thread.Stop();
}

static bool RunTestClient() {
  // setup IPC channel
  IPC::Channel chan(kTestClientChannel, IPC::Channel::MODE_CLIENT,
                    &channel_listener);
  chan.Connect();
  channel_listener.Init(&chan);
  Send(&chan, "hello from child");
  // run message loop
  MessageLoop::current()->Run();
  return true;
}

#endif  // !PERFORMANCE_TEST

#ifdef PERFORMANCE_TEST

//-----------------------------------------------------------------------------
// Manually performance test
//
//    This test times the roundtrip IPC message cycle. It is enabled with a
//    special preprocessor define to enable it instead of the standard IPC
//    unit tests. This works around some funny termination conditions in the
//    regular unit tests.
//
//    This test is not automated. To test, you will want to vary the message
//    count and message size in TEST to get the numbers you want.
//
//    FIXME(brettw): Automate this test and have it run by default.

// This channel listener just replies to all messages with the exact same
// message. It assumes each message has one string parameter. When the string
// "quit" is sent, it will exit.
class ChannelReflectorListener : public IPC::Channel::Listener {
 public:
  ChannelReflectorListener(IPC::Channel *channel) :
    channel_(channel),
    count_messages_(0),
    latency_messages_(0) {
    std::cout << "Reflector up" << std::endl;
  }

  ~ChannelReflectorListener() {
    std::cout << "Client Messages: " << count_messages_ << std::endl;
    std::cout << "Client Latency: " << latency_messages_ << std::endl;
  }

  virtual void OnMessageReceived(const IPC::Message& message) {
    count_messages_++;
    IPC::MessageIterator iter(message);
    int time = iter.NextInt();
    int msgid = iter.NextInt();
    std::string payload = iter.NextString();
    latency_messages_ += GetTickCount() - time;

    // cout << "reflector msg received: " << msgid << endl;
    if (payload == "quit")
      MessageLoop::current()->Quit();

    IPC::Message* msg = new IPC::Message(0,
                                         2,
                                         IPC::Message::PRIORITY_NORMAL);
    msg->WriteInt(GetTickCount());
    msg->WriteInt(msgid);
    msg->WriteString(payload);
    channel_->Send(msg);
  }
 private:
  IPC::Channel *channel_;
  int count_messages_;
  int latency_messages_;
};

class ChannelPerfListener : public IPC::Channel::Listener {
 public:
  ChannelPerfListener(IPC::Channel* channel, int msg_count, int msg_size) :
       count_down_(msg_count),
       channel_(channel),
       count_messages_(0),
       latency_messages_(0) {
    payload_.resize(msg_size);
    for (int i = 0; i < static_cast<int>(payload_.size()); i++)
      payload_[i] = 'a';
    std::cout << "perflistener up" << std::endl;
  }

  ~ChannelPerfListener() {
    std::cout << "Server Messages: " << count_messages_ << std::endl;
    std::cout << "Server Latency: " << latency_messages_ << std::endl;
  }

  virtual void OnMessageReceived(const IPC::Message& message) {
    count_messages_++;
    // decode the string so this gets counted in the total time
    IPC::MessageIterator iter(message);
    int time = iter.NextInt();
    int msgid = iter.NextInt();
    std::string cur = iter.NextString();
    latency_messages_ += GetTickCount() - time;

    // cout << "perflistener got message" << endl;

    count_down_--;
    if (count_down_ == 0) {
      IPC::Message* msg = new IPC::Message(0,
                                           2,
                                           IPC::Message::PRIORITY_NORMAL);
      msg->WriteInt(GetTickCount());
      msg->WriteInt(count_down_);
      msg->WriteString("quit");
      channel_->Send(msg);
      SetTimer(NULL, 1, 250, (TIMERPROC) PostQuitMessage);
      return;
    }

    IPC::Message* msg = new IPC::Message(0,
                                         2,
                                         IPC::Message::PRIORITY_NORMAL);
    msg->WriteInt(GetTickCount());
    msg->WriteInt(count_down_);
    msg->WriteString(payload_);
    channel_->Send(msg);
  }

 private:
  int count_down_;
  std::string payload_;
  IPC::Channel *channel_;
  int count_messages_;
  int latency_messages_;
};

TEST(IPCChannelTest, Performance) {
  // setup IPC channel
  IPC::Channel chan(kReflectorChannel, IPC::Channel::MODE_SERVER, NULL);
  ChannelPerfListener perf_listener(&chan, 10000, 100000);
  chan.set_listener(&perf_listener);
  chan.Connect();

  HANDLE process = SpawnChild(TEST_REFLECTOR);
  ASSERT_TRUE(process);

  Sleep(1000);

  PerfTimeLogger logger("IPC_Perf");

  // this initial message will kick-start the ping-pong of messages
  IPC::Message* message = new IPC::Message(0,
                                           2,
                                           IPC::Message::PRIORITY_NORMAL);
  message->WriteInt(GetTickCount());
  message->WriteInt(-1);
  message->WriteString("Hello");
  chan.Send(message);

  // run message loop
  MessageLoop::current()->Run();

  // cleanup child process
  WaitForSingleObject(process, 5000);
  CloseHandle(process);
}

// This message loop bounces all messages back to the sender
static bool RunReflector() {
  IPC::Channel chan(kReflectorChannel, IPC::Channel::MODE_CLIENT, NULL);
  ChannelReflectorListener channel_reflector_listener(&chan);
  chan.set_listener(&channel_reflector_listener);
  chan.Connect();

  MessageLoop::current()->Run();
  return true;
}

#endif  // PERFORMANCE_TEST

// All fatal log messages (e.g. DCHECK failures) imply unit test failures
static void IPCTestAssertHandler(const std::string& str) {
  FAIL() << str;
}

// Disable crash dialogs so that it doesn't gum up the buildbot
static void SuppressErrorDialogs() {
  UINT new_flags = SEM_FAILCRITICALERRORS |
                   SEM_NOGPFAULTERRORBOX |
                   SEM_NOOPENFILEERRORBOX;

  // Preserve existing error mode, as discussed at http://t/dmea
  UINT existing_flags = SetErrorMode(new_flags);
  SetErrorMode(existing_flags | new_flags);
}

HANDLE SpawnChild(ChildType child_type) {
  // spawn child process
  std::wstring cl(GetCommandLineW());
  switch(child_type) {
  case TEST_CLIENT:
    CommandLine::AppendSwitch(&cl, kChild);
  	break;
  case TEST_REFLECTOR:
    CommandLine::AppendSwitch(&cl, kReflector);
  	break;
  case FUZZER_SERVER:
    CommandLine::AppendSwitch(&cl, kFuzzer);
    break;
  default:
      return NULL;
  }
  // kDebugChildren support.
  if (CommandLine().HasSwitch(switches::kDebugChildren)) {
    CommandLine::AppendSwitch(&cl, switches::kDebugOnStart);
  }
  HANDLE process = NULL;
  if (!process_util::LaunchApp(cl, false, true, &process))
    return NULL;

  return process;
}

int main(int argc, char** argv) {
  MessageLoop main_message_loop;

  // suppress standard crash dialogs and such unless a debugger is present.
  if (!IsDebuggerPresent()) {
    SuppressErrorDialogs();
    logging::SetLogAssertHandler(IPCTestAssertHandler);
  }

#ifndef PERFORMANCE_TEST
  if (CommandLine().HasSwitch(kChild))
    return RunTestClient();
  if (CommandLine().HasSwitch(kFuzzer))
    return RunFuzzServer();
#else
  if (CommandLine().HasSwitch(kReflector))
    return RunReflector();

  if (!InitPerfLog("ipc_perf_child.log"))
    return 1;
#endif

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
