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

#ifndef WEBKIT_DEFAULT_PLUGIN_PLUGIN_INSTALL_JOB_MONITOR_H__
#define WEBKIT_DEFAULT_PLUGIN_PLUGIN_INSTALL_JOB_MONITOR_H__

#include "base/logging.h"
#include "base/thread.h"

// Provides the plugin installation job monitoring functionality.
// The PluginInstallationJobMonitorThread class represents a background
// thread which monitors the install job completion port which is associated
// with the job when an instance of this class is initialized.
class PluginInstallationJobMonitorThread : public Thread {
 public:
  PluginInstallationJobMonitorThread();
  virtual ~PluginInstallationJobMonitorThread();

  // Initializes the plugin install job. This involves creating the job object
  // and starting the thread which monitors the job completion port.
  // Returns true on success.
  bool Initialize();

  // Stops job monitoring and invokes the base Stop function.
  void Stop();

  // Simple setter/getters for the plugin window handle.
  void set_plugin_window(HWND plugin_window) {
    DCHECK(::IsWindow(plugin_window));
    plugin_window_ = plugin_window;
  }

  HWND plugin_window() const {
    return plugin_window_;
  }

  // Adds a process to the job object.
  //
  // Parameters:
  // process_handle
  //   Handle to the process which is to be added to the plugin install job.
  // Returns true on success.
  bool AssignProcessToJob(HANDLE process_handle);

 protected:
  // Blocks on the plugin installation job completion port by invoking the
  // GetQueuedCompletionStatus API.
  // We return from this function when the job monitoring thread is stopped.
  virtual void Init();

 private:
  // The install job completion port. Created in Init.
  HANDLE install_job_completion_port_;
  // Indicates that job monitoring is to be stopped
  bool stop_job_monitoring_;
  // The install job. Should be created before the job monitor thread
  // is started.
  HANDLE install_job_;
  // The plugin window handle.
  HWND plugin_window_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginInstallationJobMonitorThread);
};

#endif  // WEBKIT_DEFAULT_PLUGIN_PLUGIN_INSTALL_JOB_MONITOR_H__
