// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_PLUGIN_PROCESS_H_
#define CHROME_PLUGIN_PLUGIN_PROCESS_H_

#include "base/file_path.h"
#include "chrome/common/child_process.h"
#include "chrome/plugin/plugin_thread.h"

// Represents the plugin end of the renderer<->plugin connection. The
// opposite end is the PluginProcessHost. This is a singleton object for
// each plugin.
class PluginProcess : public ChildProcess {
 public:
  PluginProcess();
  virtual ~PluginProcess();

  // Invoked when the browser is shutdown. This ensures that the plugin
  // process does not hang around waiting for future invocations
  // from the browser.
  void Shutdown();

  // Returns a pointer to the PluginProcess singleton instance.
  static PluginProcess* current() {
    return static_cast<PluginProcess*>(ChildProcess::current());
  }

 private:

  virtual void OnFinalRelease();
  void OnProcessShutdownTimeout();

  DISALLOW_EVIL_CONSTRUCTORS(PluginProcess);
};

#endif  // CHROME_PLUGIN_PLUGIN_PROCESS_H_
