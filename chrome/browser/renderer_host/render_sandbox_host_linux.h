// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_SANDBOX_HOST_LINUX_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_SANDBOX_HOST_LINUX_H_

#include "base/singleton.h"
#include "base/thread.h"
#include "base/message_loop.h"

// This is a singleton object which handles sandbox requests from the
// renderers.
class RenderSandboxHostLinux {
 public:
  // Get the file descriptor which renderers should be given in order to signal
  // crashes to the browser.
  int GetRendererSocket() const { return renderer_socket_; }

 private:
  friend struct DefaultSingletonTraits<RenderSandboxHostLinux>;
  // This object must be constructed on the main thread.
  RenderSandboxHostLinux();
  ~RenderSandboxHostLinux();

  int renderer_socket_;
  int childs_lifeline_fd_;

  DISALLOW_EVIL_CONSTRUCTORS(RenderSandboxHostLinux);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_SANDBOX_HOST_LINUX_H_
