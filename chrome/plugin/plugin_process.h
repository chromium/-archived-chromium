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

#ifndef CHROME_PLUGIN_PLUGIN_PROCESS_H__
#define CHROME_PLUGIN_PLUGIN_PROCESS_H__

#include "chrome/common/child_process.h"
#include "chrome/plugin/plugin_thread.h"

// Represents the plugin end of the renderer<->plugin connection. The
// opposite end is the PluginProcessHost. This is a singleton object for
// each plugin.
class PluginProcess : public ChildProcess {
 public:
  static bool GlobalInit(const std::wstring& channel_name,
                         const std::wstring& plugin_path);

  // Invoked with the response from the browser indicating whether it is
  // ok to shutdown the plugin process.
  static void ShutdownProcessResponse(bool ok_to_shutdown);

  // Invoked when the browser is shutdown. This ensures that the plugin
  // process does not hang around waiting for future invocations
  // from the browser.
  static void BrowserShutdown();

  // File path of the plugin dll this process hosts.
  const std::wstring& plugin_path() { return plugin_path_; }

 private:
  friend class PluginProcessFactory;
  PluginProcess(const std::wstring& channel_name,
                const std::wstring& plugin_path);
  virtual ~PluginProcess();

  virtual void OnFinalRelease();
  void Shutdown();
  void OnProcessShutdownTimeout();

  const std::wstring plugin_path_;

  // The thread where plugin instances live.  Since NPAPI plugins weren't
  // created with multi-threading in mind, running multiple instances on
  // different threads would be asking for trouble.
  PluginThread plugin_thread_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginProcess);
};

#endif  // CHROME_PLUGIN_PLUGIN_PROCESS_H__
