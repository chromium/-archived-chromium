// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/common/ipc_tests.h"

#include <sys/stat.h>

#include "base/message_loop.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/ipc_message_utils.h"

#if defined(OS_POSIX)

class MyChannelDescriptorListener : public IPC::Channel::Listener {
 public:
  virtual void OnMessageReceived(const IPC::Message& message) {
    void* iter = NULL;

    FileDescriptor descriptor;

    ASSERT_TRUE(
        IPC::ParamTraits<FileDescriptor>::Read(&message, &iter, &descriptor));
    VerifyDescriptor(&descriptor);
    MessageLoop::current()->Quit();
  }

  virtual void OnChannelError() {
    MessageLoop::current()->Quit();
  }

private:
  static void VerifyDescriptor(FileDescriptor* descriptor) {
    const int fd = open("/dev/null", O_RDONLY);
    struct stat st1, st2;
    fstat(fd, &st1);
    close(fd);
    fstat(descriptor->fd, &st2);
    close(descriptor->fd);
    ASSERT_EQ(st1.st_ino, st2.st_ino);
  }
};

TEST_F(IPCChannelTest, DescriptorTest) {
  // Setup IPC channel.
  MyChannelDescriptorListener listener;

  IPC::Channel chan(kTestClientChannel, IPC::Channel::MODE_SERVER,
                    &listener);
  chan.Connect();

  base::ProcessHandle process_handle = SpawnChild(TEST_DESCRIPTOR_CLIENT,
                                                  &chan);
  ASSERT_TRUE(process_handle);

  FileDescriptor descriptor;
  const int fd = open("/dev/null", O_RDONLY);
  ASSERT_GE(fd, 0);
  descriptor.auto_close = true;
  descriptor.fd = fd;

  IPC::Message* message = new IPC::Message(0, // routing_id
                                           3, // message type
                                           IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<FileDescriptor>::Write(message, descriptor);
  chan.Send(message);

  // Run message loop.
  MessageLoop::current()->Run();

  // Close Channel so client gets its OnChannelError() callback fired.
  chan.Close();

  // Cleanup child process.
  EXPECT_TRUE(base::WaitForSingleProcess(process_handle, 5000));
}

MULTIPROCESS_TEST_MAIN(RunTestDescriptorClient) {
  MessageLoopForIO main_message_loop;
  MyChannelDescriptorListener listener;

  // setup IPC channel
  IPC::Channel chan(kTestClientChannel, IPC::Channel::MODE_CLIENT,
                    &listener);
  chan.Connect();

  // run message loop
  MessageLoop::current()->Run();
  // return true;
  return NULL;
}

#endif  // defined(OS_POSIX)
