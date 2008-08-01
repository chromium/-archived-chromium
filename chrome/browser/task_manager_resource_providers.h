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

#ifndef CHROME_BROWSER_TASK_MANAGER_RESOURCE_PROVIDERS_H__
#define CHROME_BROWSER_TASK_MANAGER_RESOURCE_PROVIDERS_H__

#include "base/basictypes.h"
#include "chrome/browser/plugin_process_info.h"
#include "chrome/browser/task_manager.h"
#include "chrome/common/notification_service.h"

class PluginProcessHost;
class WebContents;

// These file contains the resource providers used in the task manager.

class TaskManagerWebContentsResource : public TaskManager::Resource {
 public:
  explicit TaskManagerWebContentsResource(WebContents* web_contents);
  ~TaskManagerWebContentsResource();

  // TaskManagerResource methods:
  std::wstring GetTitle() const;
  SkBitmap GetIcon() const;
  HANDLE GetProcess() const;
  // WebContents always provide the network usage.
  bool SupportNetworkUsage() const { return true; }
  void SetSupportNetworkUsage() { };

 private:
  WebContents* web_contents_;
  HANDLE process_;
  int pid_;

  DISALLOW_EVIL_CONSTRUCTORS(TaskManagerWebContentsResource);
};

class TaskManagerWebContentsResourceProvider
    : public TaskManager::ResourceProvider,
      public NotificationObserver {
 public:
  explicit TaskManagerWebContentsResourceProvider(TaskManager* task_manager);
  virtual ~TaskManagerWebContentsResourceProvider();

  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id);
  virtual void StartUpdating();
  virtual void StopUpdating();

  // NotificationObserver method:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  void Add(WebContents* web_contents);
  void Remove(WebContents* web_contents);

  void AddToTaskManager(WebContents* web_contents);

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  TaskManager* task_manager_;

  // Maps the actual resources (the WebContents) to the Task Manager
  // resources.
  std::map<WebContents*, TaskManagerWebContentsResource*> resources_;

  DISALLOW_EVIL_CONSTRUCTORS(TaskManagerWebContentsResourceProvider);
};

class TaskManagerPluginProcessResource : public TaskManager::Resource {
 public:
  explicit TaskManagerPluginProcessResource(PluginProcessInfo plugin_proc);
  ~TaskManagerPluginProcessResource();

  // TaskManagerResource methods:
  std::wstring GetTitle() const;
  SkBitmap GetIcon() const;
  HANDLE GetProcess() const;

  bool SupportNetworkUsage() const {
    return network_usage_support_;
  }

  void SetSupportNetworkUsage() {
    network_usage_support_ = true;
  }

  // Returns the pid of the plugin process.
  int process_id() const { return pid_; }

 private:
  PluginProcessInfo plugin_process_;
  int pid_;
  mutable std::wstring title_;
  bool network_usage_support_;

  // The icon painted for plugins.
  // TODO (jcampan): we should have plugin specific icons for well-known
  // plugins.
  static SkBitmap* default_icon_;

  DISALLOW_EVIL_CONSTRUCTORS(TaskManagerPluginProcessResource);
};

class TaskManagerPluginProcessResourceProvider
    : public TaskManager::ResourceProvider,
      public NotificationObserver {
 public:
  explicit TaskManagerPluginProcessResourceProvider(TaskManager* task_manager);
  virtual ~TaskManagerPluginProcessResourceProvider();

  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id);
  virtual void StartUpdating();
  virtual void StopUpdating();

  // NotificationObserver method:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Retrieves the current PluginProcessInfo (performed in the IO thread).
  virtual void RetrievePluginProcessInfo();

  // Notifies the UI thread that the PluginProcessInfo have been retrieved.
  virtual void PluginProcessInfoRetreived();

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  // The list of PluginProcessInfo retrieved when starting the update.
  std::vector<PluginProcessInfo> existing_plugin_process_info;

 private:
  void Add(PluginProcessInfo plugin_process_info);
  void Remove(PluginProcessInfo plugin_process_info);

  void AddToTaskManager(PluginProcessInfo plugin_process_info);

  TaskManager* task_manager_;

  MessageLoop* ui_loop_;

  // Maps the actual resources (the PluginProcessInfo) to the Task Manager
  // resources.
  std::map<PluginProcessInfo, TaskManagerPluginProcessResource*> resources_;

  // Maps the pids to the resources (used for quick access to the resource on
  // byte read notifications).
  std::map<int, TaskManagerPluginProcessResource*> pid_to_resources_;

  DISALLOW_EVIL_CONSTRUCTORS(TaskManagerPluginProcessResourceProvider);
};

class TaskManagerBrowserProcessResource : public TaskManager::Resource {
 public:
  TaskManagerBrowserProcessResource();
  ~TaskManagerBrowserProcessResource();

  // TaskManagerResource methods:
  std::wstring GetTitle() const;
  SkBitmap GetIcon() const;
  HANDLE GetProcess() const;

  bool SupportNetworkUsage() const {
    return network_usage_support_;
  }

  void SetSupportNetworkUsage() {
    network_usage_support_ = true;
  }

  // Returns the pid of the browser process.
  int process_id() const { return pid_; }

 private:
  HANDLE process_;
  int pid_;
  bool network_usage_support_;
  mutable std::wstring title_;

  static SkBitmap* default_icon_;

  DISALLOW_EVIL_CONSTRUCTORS(TaskManagerBrowserProcessResource);
};

class TaskManagerBrowserProcessResourceProvider
    : public TaskManager::ResourceProvider {
 public:
  explicit TaskManagerBrowserProcessResourceProvider(
      TaskManager* task_manager);
  virtual ~TaskManagerBrowserProcessResourceProvider();

  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id);
  virtual void StartUpdating();
  virtual void StopUpdating();

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

 private:
  void AddToTaskManager(PluginProcessInfo plugin_process_info);

  TaskManager* task_manager_;
  TaskManagerBrowserProcessResource resource_;

  DISALLOW_EVIL_CONSTRUCTORS(TaskManagerBrowserProcessResourceProvider);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_RESOURCE_PROVIDERS_H__
