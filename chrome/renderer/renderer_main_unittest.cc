// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/multiprocess_test.h"
#include "base/process_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/main_function_params.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(port): Bring up this test this on other platforms.
#if defined(OS_MACOSX)

using base::ProcessHandle;

const std::wstring kRendererTestChannelName(L"test");

extern int RendererMain(const MainFunctionParams& parameters);

// TODO(port): The IPC Channel tests have a class with extremely similar
// functionality, we should combine them.
class RendererMainTest : public MultiProcessTest {
 protected:

  // Create a new MessageLoopForIO For each test.
  virtual void SetUp();
  virtual void TearDown();

  // Spawns a child process of the specified type
  base::ProcessHandle SpawnChild(const std::wstring &procname,
                                 IPC::Channel *channel);

  // Created around each test instantiation.
  MessageLoopForIO *message_loop_;
};

void RendererMainTest::SetUp() {
  MultiProcessTest::SetUp();

  // Construct a fresh IO Message loop for the duration of each test.
  message_loop_ = new MessageLoopForIO();
}

void RendererMainTest::TearDown() {
  delete message_loop_;
  message_loop_ = NULL;

  MultiProcessTest::TearDown();
}

ProcessHandle RendererMainTest::SpawnChild(const std::wstring &procname,
                                           IPC::Channel *channel) {
  base::file_handle_mapping_vector fds_to_map;
  int src_fd;
  int dest_fd;
  channel->GetClientFileDescriptorMapping(&src_fd, &dest_fd);
  if (src_fd > -1) {
    fds_to_map.push_back(std::pair<int,int>(src_fd, dest_fd));
  }

   return MultiProcessTest::SpawnChild(procname, fds_to_map, false);
}

// Listener class that kills the message loop when it connects.
class SuicidalListener : public IPC::Channel::Listener {
 public:
  void OnChannelConnected(int32 peer_pid) {
    MessageLoop::current()->Quit();
  }

  void OnMessageReceived(const IPC::Message& message) {
    // We shouldn't receive any messages
    ASSERT_TRUE(false);
  }
};

MULTIPROCESS_TEST_MAIN(SimpleRenderer) {
  SandboxInitWrapper dummy_sandbox_init;
  CommandLine cl(*CommandLine::ForCurrentProcess());
  cl.AppendSwitchWithValue(switches::kProcessChannelID,
                                     kRendererTestChannelName);

  MainFunctionParams dummy_params(cl,
                                  dummy_sandbox_init);
  return RendererMain(dummy_params);
}

TEST_F(RendererMainTest, CreateDestroy) {
  SuicidalListener listener;
  IPC::Channel control_channel(kRendererTestChannelName,
                               IPC::Channel::MODE_SERVER,
                               &listener);
  base::ProcessHandle renderer_pid = SpawnChild(L"SimpleRenderer",
                                                &control_channel);

  control_channel.Connect();
  MessageLoop::current()->Run();

  // The renderer should exit when we close the channel.
  control_channel.Close();

  // TODO(playmobil): Remove this.
  // The renderer should shut down automatically when the Channel is closed
  // but there are still issues with this, leave in the kill for bringup.
  base::KillProcess(renderer_pid, 9, true);

  // TODO(playmobil): Enable this.
  // see comment above KillProcess()
  // EXPECT_TRUE(base::WaitForSingleProcess(renderer_pid, 5000));
}

#endif  // defined(OS_MACOSX)
