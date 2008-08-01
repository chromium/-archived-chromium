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

#ifndef CHROME_BROWSER_TASK_MANAGER_H__
#define CHROME_BROWSER_TASK_MANAGER_H__

#include "base/lock.h"
#include "base/singleton.h"
#include "base/ref_counted.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/group_table_view.h"
#include "chrome/browser/cache_manager_host.h"
#include "net/url_request/url_request_job_tracker.h"

class MessageLoop;
class ModelEntry;
class PrefService;
class SkBitmap;
class Task;
class TaskManager;
class TaskManagerContents;
class TaskManagerTableModel;
class TaskManagerWindow;
class Timer;

struct BytesReadParam;

namespace ChromeViews {
class View;
class Window;
}

namespace process_util {
class ProcessMetrics;
}

// This class is a singleton.
class TaskManager : public ChromeViews::DialogDelegate {
 public:
  // A resource represents one row in the task manager.
  // Resources from similar processes are grouped together by the task manager.
  class Resource {
   public:
    virtual std::wstring GetTitle() const = 0;
    virtual SkBitmap GetIcon() const = 0;
    virtual HANDLE GetProcess() const = 0;

    // Whether this resource does report the network usage accurately.
    // This controls whether 0 or N/A is displayed when no bytes have been
    // reported as being read. This is because some plugins do not report the
    // bytes read and we don't want to display a misleading 0 value in that
    // case.
    virtual bool SupportNetworkUsage() const = 0;

    // Called when some bytes have been read and support_network_usage returns
    // false(meaning we do have network usage support).
    virtual void SetSupportNetworkUsage() = 0;
  };

  // ResourceProviders are responsible for adding/removing resources to the task
  // manager. The task manager notifies the ResourceProvider that it is ready
  // to receive resource creation/termination notifications with a call to
  // StartUpdating(). At that point, the resource provider should call
  // AddResource with all the existing resources, and after that it should call
  // AddResource/RemoveResource as resources are created/terminated.
  // The provider remains the owner of the resource objects and is responsible
  // for deleting them (when StopUpdating() is called).
  // After StopUpdating() is called the provider should also stop reporting
  // notifications to the task manager.
  // Note: ResourceProviders have to be ref counted as they are used in
  // MessageLoop::InvokeLater().
  class ResourceProvider : public base::RefCounted<ResourceProvider> {
   public:
    // Should return the resource associated to the specified ids, or NULL if
    // the resource does not belong to this provider.
    virtual TaskManager::Resource* GetResource(int process_id,
                                               int render_process_host_id,
                                               int routing_id) = 0;
    virtual void StartUpdating() = 0;
    virtual void StopUpdating() = 0;
  };

  static void RegisterPrefs(PrefService* prefs);


  // Call this method to show the Task Manager.
  // Only one instance of Task Manager is created, so if the Task Manager has
  // already be opened, it is reopened. If it is currently opened, then it is
  // moved to the front.
  static void Open();

  // Close the task manager.
  void Close();

  // Returns true if the current selection includes the browser process.
  bool BrowserProcessIsSelected();

  // Terminates the selected tab(s) in the list.
  void KillSelectedProcesses();

  void AddResourceProvider(ResourceProvider* provider);
  void RemoveResourceProvider(ResourceProvider* provider);

  // These methods are invoked by the resource providers to add/remove resources
  // to the Task Manager. Note that the resources are owned by the
  // ResourceProviders and are not valid after StopUpdating() has been called
  // on the ResourceProviders.
  void AddResource(Resource* resource);
  void RemoveResource(Resource* resource);

  // ChromeViews::DialogDelegate methods:
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool ShouldShowWindowIcon() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void SaveWindowPosition(const CRect& bounds,
                                  bool maximized,
                                  bool always_on_top);
  virtual bool RestoreWindowPosition(CRect* bounds,
                                     bool* maximized,
                                     bool* always_on_top);
  virtual int GetDialogButtons() const;
  virtual void WindowClosing();
  virtual ChromeViews::View* GetContentsView();

 private:
  // Obtain an instance via GetInstance().
  TaskManager();
  friend DefaultSingletonTraits<TaskManager>;

  ~TaskManager();

  void Init();

  // Returns the singleton instance (and initializes it if necessary).
  static TaskManager* GetInstance();

  // The model used for the list in the table that displays the list of tab
  // processes. It is ref counted because it is passed as a parameter to
  // MessageLoop::InvokeLater().
  scoped_refptr<TaskManagerTableModel> table_model_;

  // A container containing the buttons and table.
  scoped_ptr<TaskManagerContents> contents_;

  DISALLOW_EVIL_CONSTRUCTORS(TaskManager);
};

// The model that the table is using.
class TaskManagerTableModel : public ChromeViews::GroupTableModel,
                              public URLRequestJobTracker::JobObserver,
                              public base::RefCounted<TaskManagerTableModel> {
 public:
  explicit TaskManagerTableModel(TaskManager* task_manager);
  ~TaskManagerTableModel();

  // GroupTableModel methods:
  int RowCount();
  std::wstring GetText(int row, int column);
  SkBitmap GetIcon(int row);
  void GetGroupRangeForItem(int item, ChromeViews::GroupRange* range);
  void SetObserver(ChromeViews::TableModelObserver* observer);

  // Returns the index at the specified row.
  HANDLE GetProcessAt(int index);

  // JobObserver methods:
  void OnJobAdded(URLRequestJob* job);
  void OnJobRemoved(URLRequestJob* job);
  void OnJobDone(URLRequestJob* job, const URLRequestStatus& status);
  void OnJobRedirect(URLRequestJob* job, const GURL& location, int status_code);
  void OnBytesRead(URLRequestJob* job, int byte_count);

  void Refresh();

  void AddResourceProvider(TaskManager::ResourceProvider* provider);
  void RemoveResourceProvider(TaskManager::ResourceProvider* provider);

  void AddResource(TaskManager::Resource* resource);
  void RemoveResource(TaskManager::Resource* resource);

 private:
  friend class TaskManager;

  // This struct is used to exchange information between the io and ui threads.
  struct BytesReadParam {
    BytesReadParam(int origin_pid, int render_process_host_id,
                   int routing_id, int byte_count)
      : origin_pid(origin_pid),
        render_process_host_id(render_process_host_id),
        routing_id(routing_id),
        byte_count(byte_count) { }

    int origin_pid;
    int render_process_host_id;
    int routing_id;
    int byte_count;
  };

  typedef std::map<HANDLE, std::vector<TaskManager::Resource*>*> GroupMap;
  typedef std::map<HANDLE, process_util::ProcessMetrics*> MetricsMap;
  typedef std::map<TaskManager::Resource*, int64> ResourceValueMap;
  typedef std::vector<TaskManager::Resource*> ResourceList;
  typedef std::vector<TaskManager::ResourceProvider*> ResourceProviderList;

  void StartUpdating();
  void StopUpdating();

  // Removes all items.
  void Clear();
  void AddItem(TaskManager::Resource* resource, bool notify_table);
  void RemoveItem(TaskManager::Resource* resource);

  // Register for network usage updates
  void RegisterForJobDoneNotifications();
  void UnregisterForJobDoneNotifications();

  // Returns the network usage (in bytes per seconds) for the specified
  // resource. That's the value retrieved at the last timer's tick.
  int64 GetNetworkUsageForResource(TaskManager::Resource* resource);


  // Called on the UI thread when some bytes are read.
  void BytesRead(BytesReadParam param);

  // The list of providers to the task manager. They are ref counted.
  ResourceProviderList providers_;

  // The list of all the resources displayed in the task manager. They are owned
  // by the ResourceProviders.
  ResourceList resources_;

  // A map to keep tracks of the grouped resources (they are grouped if they
  // share the same process). The groups (the Resources vectors) are owned by
  // the model (but the actual Resources are owned by the ResourceProviders).
  GroupMap group_map_;

  // A map to retrieve the process metrics for a process. The ProcessMetrics are
  // owned by the model.
  MetricsMap metrics_map_;

  // A map that keeps track of the number of bytes read per process since last
  // tick. The Resources are owned by the ResourceProviders.
  ResourceValueMap current_byte_count_map_;

  // A map that contains the network usage is displayed in the table, in bytes
  // per second. It is computed every time the timer ticks. The Resources are
  // owned by the ResourceProviders.
  ResourceValueMap displayed_network_usage_map_;

  // Is true only when the timer is running and we are periodically retrieving
  // the information.
  bool is_updating_;

  ChromeViews::TableModelObserver* observer_;

  // The timer controlling the updates of the information. The timer is
  // allocated every time the task manager is shown and deleted when it is
  // hidden/closed.
  Timer* timer_;

  scoped_ptr<Task> update_task_;
  MessageLoop* ui_loop_;

  // See design doc at http://go/at-teleporter for more information.
  static int goats_teleported_;

  DISALLOW_EVIL_CONSTRUCTORS(TaskManagerTableModel);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_H__

