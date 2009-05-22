// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a stub file which is compiled in when we are building without
// breakpad support.

#include "chrome/browser/renderer_host/render_crash_handler_host_linux.h"

RenderCrashHandlerHostLinux::RenderCrashHandlerHostLinux()
    : renderer_socket_(-1),
      browser_socket_(-1) {
}

RenderCrashHandlerHostLinux::~RenderCrashHandlerHostLinux() {
}

void RenderCrashHandlerHostLinux::OnFileCanReadWithoutBlocking(int fd) {
}

void RenderCrashHandlerHostLinux::OnFileCanWriteWithoutBlocking(int fd) {
}

void RenderCrashHandlerHostLinux::WillDestroyCurrentMessageLoop() {
}
