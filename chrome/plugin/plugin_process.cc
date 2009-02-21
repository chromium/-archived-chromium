// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include "chrome/plugin/plugin_process.h"

#include "base/basictypes.h"
#include "base/scoped_handle.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/ipc_message_utils.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "webkit/glue/webkit_glue.h"

// How long to wait after there are no more plugin instances before killing the
// process.
static const int kProcessShutdownDelayMs = 10 * 1000;

// No AddRef required when using PluginProcess with RunnableMethod.  This is
// okay since the lifetime of the PluginProcess is always greater than the
// lifetime of PluginThread because it's a member variable.
template <> struct RunnableMethodTraits<PluginProcess> {
  static void RetainCallee(PluginProcess*) {}
  static void ReleaseCallee(PluginProcess*) {}
};

PluginProcess::PluginProcess()
    : ChildProcess(new PluginThread()) {
}

PluginProcess::~PluginProcess() {
}

// Note: may be called on any thread
void PluginProcess::OnFinalRelease() {
  // We override this to have the process linger for a few seconds to
  // better accomdate back/forth navigation. This avoids shutting down and
  // immediately starting a new plugin process. If a new channel is
  // opened in the interim, the current process will not be shutdown.
  child_thread()->owner_loop()->PostDelayedTask(FROM_HERE, NewRunnableMethod(
      this, &PluginProcess::OnProcessShutdownTimeout),
      kProcessShutdownDelayMs);
}

void PluginProcess::OnProcessShutdownTimeout() {
  if (ProcessRefCountIsZero()) {
    // The plugin process shutdown sequence is a request response based
    // mechanism, where we send out an initial feeler request to the plugin
    // process host instance in the browser to verify if it is ok to shutdown
    // the plugin process. The browser then sends back a response indicating
    // whether it is ok to shutdown.
    child_thread()->Send(new PluginProcessHostMsg_ShutdownRequest);
  }
}

void PluginProcess::Shutdown() {
  ChildProcess::OnFinalRelease();
}
