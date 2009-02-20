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
  static bool GlobalInit(const std::wstring& channel_name,
                         const FilePath& plugin_path);

  // Invoked with the response from the browser indicating whether it is
  // ok to shutdown the plugin process.
  static void ShutdownProcessResponse(bool ok_to_shutdown);

  // Invoked when the browser is shutdown. This ensures that the plugin
  // process does not hang around waiting for future invocations
  // from the browser.
  static void BrowserShutdown();

  // File path of the plugin this process hosts.
  const FilePath& plugin_path() { return plugin_path_; }

 private:
  friend class PluginProcessFactory;
  PluginProcess(const std::wstring& channel_name,
                const FilePath& plugin_path);
  virtual ~PluginProcess();

  virtual void OnFinalRelease();
  void Shutdown();
  void OnProcessShutdownTimeout();

  const FilePath plugin_path_;

  // The thread where plugin instances live.  Since NPAPI plugins weren't
  // created with multi-threading in mind, running multiple instances on
  // different threads would be asking for trouble.
  PluginThread plugin_thread_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginProcess);
};

#endif  // CHROME_PLUGIN_PLUGIN_PROCESS_H_
