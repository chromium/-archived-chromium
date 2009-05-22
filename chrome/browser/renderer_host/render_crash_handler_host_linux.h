// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_CRASH_HANDLER_HOST_LINUX_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_CRASH_HANDLER_HOST_LINUX_H_

#include "base/singleton.h"
#include "base/message_loop.h"

// This is a singleton object which crash dumps renderers on Linux. We perform
// the crash dump from the browser because it allows us to be outside the
// sandbox.
//
// Renderers signal that they need to be dumped by sending a datagram over a
// UNIX domain socket. All renderers share the client end of this socket which
// is installed in their descriptor table before exec.
class RenderCrashHandlerHostLinux : public MessageLoopForIO::Watcher,
                                    public MessageLoop::DestructionObserver {
 public:
  // Get the file descriptor which renderers should be given in order to signal
  // crashes to the browser.
  int GetDeathSignalSocket() const {
    return renderer_socket_;
  }

  // MessagePumbLibevent::Watcher impl:
  virtual void OnFileCanWriteWithoutBlocking(int fd);
  virtual void OnFileCanReadWithoutBlocking(int fd);

  // MessageLoop::DestructionObserver impl:
  virtual void WillDestroyCurrentMessageLoop();

 private:
  friend struct DefaultSingletonTraits<RenderCrashHandlerHostLinux>;
  RenderCrashHandlerHostLinux();
  ~RenderCrashHandlerHostLinux();
  void Init();

  int renderer_socket_;
  int browser_socket_;
  MessageLoopForIO::FileDescriptorWatcher file_descriptor_watcher_;

  DISALLOW_EVIL_CONSTRUCTORS(RenderCrashHandlerHostLinux);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_CRASH_HANDLER_HOST_LINUX_H_
