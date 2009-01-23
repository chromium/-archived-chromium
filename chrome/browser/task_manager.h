// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_H_
#define CHROME_BROWSER_TASK_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/lock.h"
#include "base/singleton.h"
#include "base/ref_counted.h"
#include "base/timer.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/group_table_view.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
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

struct BytesReadParam;

namespace views {
class View;
class Window;
}

namespace base {
class ProcessMetrics;
}

// This class is a singleton.
class TaskManager : public views::DialogDelegate {
 public:
  // A resource represents one row in the task manager.
  // Resources from similar processes are grouped together by the task manager.
  class Resource {
   public:
    virtual ~Resource() {}

    virtual std::wstring GetTitle() const = 0;
    virtual SkBitmap GetIcon() const = 0;
    virtual HANDLE GetProcess() const = 0;

    // A helper function for ActivateFocusedTab.  Returns NULL by default
    // because not all resources have an assoiciated tab.
    virtual TabContents* GetTabContents() const {return NULL;}

    // Whether this resource does report the network usage accurately.
    // This controls whether 0 or N/A is displayed when no bytes have been
    // reported as being read. This is because some plugins do not report the
    // bytes read and we don't want to display a misleading 0 value in that
    // case.
    virtual bool SupportNetworkUsage() const = 0;

    // Called when some bytes have been read and support_network_usage returns
    // false (meaning we do have network usage support).
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

  // Activates the browser tab associated with the focused row in the task
  // manager table.  This happens when the user double clicks or hits return.
  void ActivateFocusedTab();

  void AddResourceProvider(ResourceProvider* provider);
  void RemoveResourceProvider(ResourceProvider* provider);

  // These methods are invoked by the resource providers to add/remove resources
  // to the Task Manager. Note that the resources are owned by the
  // ResourceProviders and are not valid after StopUpdating() has been called
  // on the ResourceProviders.
  void AddResource(Resource* resource);
  void RemoveResource(Resource* resource);

  // views::DialogDelegate methods:
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool ShouldShowWindowIcon() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetWindowName() const;
  virtual int GetDialogButtons() const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();

 private:
  // Obtain an instance via GetInstance().
  TaskManager();
  friend DefaultSingletonTraits<TaskManager>;

  ~TaskManager();

  void Init();

  // Returns the singleton instance (and initializes it if necessary).
  static TaskManager* GetInstance();

  // The model used for the list in the table that displays the list of tab
  // processes.  It is ref counted because it is passed as a parameter to
  // MessageLoop::InvokeLater().
  scoped_refptr<TaskManagerTableModel> table_model_;

  // A container containing the buttons and table.
  scoped_ptr<TaskManagerContents> contents_;

  DISALLOW_COPY_AND_ASSIGN(TaskManager);
};

// The model that the table is using.
class TaskManagerTableModel : public views::GroupTableModel,
                              public URLRequestJobTracker::JobObserver,
                              public base::RefCounted<TaskManagerTableModel> {
 public:
  explicit TaskManagerTableModel(TaskManager* task_manager);
  ~TaskManagerTableModel();

  // GroupTableModel methods:
  int RowCount();
  std::wstring GetText(int row, int column);
  SkBitmap GetIcon(int row);
  void GetGroupRangeForItem(int item, views::GroupRange* range);
  void SetObserver(views::TableModelObserver* observer);
  virtual int CompareValues(int row1, int row2, int column_id);

  // Returns the index at the specified row.
  HANDLE GetProcessAt(int index);

  // JobObserver methods:
  void OnJobAdded(URLRequestJob* job);
  void OnJobRemoved(URLRequestJob* job);
  void OnJobDone(URLRequestJob* job, const URLRequestStatus& status);
  void OnJobRedirect(URLRequestJob* job, const GURL& location, int status_code);
  void OnBytesRead(URLRequestJob* job, int byte_count);

  void AddResourceProvider(TaskManager::ResourceProvider* provider);
  void RemoveResourceProvider(TaskManager::ResourceProvider* provider);

  void AddResource(TaskManager::Resource* resource);
  void RemoveResource(TaskManager::Resource* resource);

 private:
  friend class TaskManager;

  enum UpdateState {
    IDLE = 0,      // Currently not updating.
    TASK_PENDING,  // An update task is pending.
    STOPPING       // A update task is pending and it should stop the update.
  };

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
  typedef std::map<HANDLE, base::ProcessMetrics*> MetricsMap;
  typedef std::map<HANDLE, int> CPUUsageMap;
  typedef std::map<TaskManager::Resource*, int64> ResourceValueMap;
  typedef std::vector<TaskManager::Resource*> ResourceList;
  typedef std::vector<TaskManager::ResourceProvider*> ResourceProviderList;

  void StartUpdating();
  void StopUpdating();

  // Updates the values for all rows.
  void Refresh();

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

  // Returns the network usage (in byte per second) that should be displayed for
  // the passed |resource|.  -1 means the information is not available for that
  // resource.
  int64 GetNetworkUsage(TaskManager::Resource* resource);

  // Returns the CPU usage (in %) that should be displayed for the passed
  // |resource|.
  int GetCPUUsage(TaskManager::Resource* resource);

  // Retrieves the private memory (in KB) that should be displayed from the
  // passed |process_metrics|.
  size_t GetPrivateMemory(base::ProcessMetrics* process_metrics);

  // Returns the shared memory (in KB) that should be displayed from the passed
  // |process_metrics|.
  size_t GetSharedMemory(base::ProcessMetrics* process_metrics);

  // Returns the pysical memory (in KB) that should be displayed from the passed
  // |process_metrics|.
  size_t GetPhysicalMemory(base::ProcessMetrics* process_metrics);

  // Returns the stat value at the column |col_id| that should be displayed from
  // the passed |process_metrics|.
  int GetStatsValue(TaskManager::Resource* resource, int col_id);

  // Retrieves the ProcessMetrics for the resources at the specified rows.
  // Returns true if there was a ProcessMetrics available for both rows.
  bool GetProcessMetricsForRows(int row1,
                                int row2,
                                base::ProcessMetrics** proc_metrics1,
                                base::ProcessMetrics** proc_metrics2);

  // Given a string containing a number, this function returns the formatted
  // string that should be displayed in the task manager's memory cell.
  std::wstring GetMemCellText(std::wstring* number) const;

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

  // A map that contains the CPU usage (in %) for a process since last refresh.
  CPUUsageMap cpu_usage_map_;

  views::TableModelObserver* observer_;

  MessageLoop* ui_loop_;

  // Whether we are currently in the process of updating.
  UpdateState update_state_;

  // See design doc at http://go/at-teleporter for more information.
  static int goats_teleported_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerTableModel);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_H_


