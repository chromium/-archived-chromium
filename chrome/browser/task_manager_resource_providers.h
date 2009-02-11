// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_RESOURCE_PROVIDERS_H_
#define CHROME_BROWSER_TASK_MANAGER_RESOURCE_PROVIDERS_H_

#include "base/basictypes.h"
#include "chrome/browser/task_manager.h"
#include "chrome/common/child_process_info.h"
#include "chrome/common/notification_observer.h"

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
  TabContents* GetTabContents() const;

  // WebContents always provide the network usage.
  bool SupportNetworkUsage() const { return true; }
  void SetSupportNetworkUsage() { };

 private:
  WebContents* web_contents_;
  HANDLE process_;
  int pid_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerWebContentsResource);
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

  DISALLOW_COPY_AND_ASSIGN(TaskManagerWebContentsResourceProvider);
};

class TaskManagerChildProcessResource : public TaskManager::Resource {
 public:
  explicit TaskManagerChildProcessResource(ChildProcessInfo child_proc);
  ~TaskManagerChildProcessResource();

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

  // Returns the pid of the child process.
  int process_id() const { return pid_; }

 private:
  ChildProcessInfo child_process_;
  int pid_;
  mutable std::wstring title_;
  bool network_usage_support_;

  // The icon painted for the child processs.
  // TODO (jcampan): we should have plugin specific icons for well-known
  // plugins.
  static SkBitmap* default_icon_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerChildProcessResource);
};

class TaskManagerChildProcessResourceProvider
    : public TaskManager::ResourceProvider,
      public NotificationObserver {
 public:
  explicit TaskManagerChildProcessResourceProvider(TaskManager* task_manager);
  virtual ~TaskManagerChildProcessResourceProvider();

  virtual TaskManager::Resource* GetResource(int origin_pid,
                                             int render_process_host_id,
                                             int routing_id);
  virtual void StartUpdating();
  virtual void StopUpdating();

  // NotificationObserver method:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Retrieves the current ChildProcessInfo (performed in the IO thread).
  virtual void RetrieveChildProcessInfo();

  // Notifies the UI thread that the ChildProcessInfo have been retrieved.
  virtual void ChildProcessInfoRetreived();

  // Whether we are currently reporting to the task manager. Used to ignore
  // notifications sent after StopUpdating().
  bool updating_;

  // The list of ChildProcessInfo retrieved when starting the update.
  std::vector<ChildProcessInfo> existing_child_process_info_;

 private:
  void Add(ChildProcessInfo child_process_info);
  void Remove(ChildProcessInfo child_process_info);

  void AddToTaskManager(ChildProcessInfo child_process_info);

  TaskManager* task_manager_;

  MessageLoop* ui_loop_;

  // Maps the actual resources (the ChildProcessInfo) to the Task Manager
  // resources.
  std::map<ChildProcessInfo, TaskManagerChildProcessResource*> resources_;

  // Maps the pids to the resources (used for quick access to the resource on
  // byte read notifications).
  std::map<int, TaskManagerChildProcessResource*> pid_to_resources_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerChildProcessResourceProvider);
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

  DISALLOW_COPY_AND_ASSIGN(TaskManagerBrowserProcessResource);
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
  void AddToTaskManager(ChildProcessInfo child_process_info);

  TaskManager* task_manager_;
  TaskManagerBrowserProcessResource resource_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerBrowserProcessResourceProvider);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_RESOURCE_PROVIDERS_H_
