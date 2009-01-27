// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DEFAULT_PLUGIN_PLUGIN_INSTALL_JOB_MONITOR_H__
#define WEBKIT_DEFAULT_PLUGIN_PLUGIN_INSTALL_JOB_MONITOR_H__

#include <windows.h>

#include "base/logging.h"
#include "base/thread.h"
#include "base/ref_counted.h"

// Provides the plugin installation job monitoring functionality.
// The PluginInstallationJobMonitorThread class represents a background
// thread which monitors the install job completion port which is associated
// with the job when an instance of this class is initialized.
class PluginInstallationJobMonitorThread :
    public base::Thread,
    public base::RefCountedThreadSafe<PluginInstallationJobMonitorThread> {

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
  // Installs a task on our thread to call WaitForJobThread().  We
  // can't call it directly since we would deadlock (thread which
  // creates me blocks until Start() returns, and Start() doesn't
  // return until Init() does).
  virtual void Init();

  // Blocks on the plugin installation job completion port by invoking the
  // GetQueuedCompletionStatus API.
  // We return from this function when the job monitoring thread is stopped.
  virtual void WaitForJobThread();

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

