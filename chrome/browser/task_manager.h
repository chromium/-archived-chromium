// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_H_
#define CHROME_BROWSER_TASK_MANAGER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/process_util.h"
#include "base/ref_counted.h"
#include "base/singleton.h"
#include "base/timer.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "net/url_request/url_request_job_tracker.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class MessageLoop;
class SkBitmap;
class TaskManager;
class TaskManagerModel;

struct BytesReadParam;

namespace base {
class ProcessMetrics;
}

// This class is a singleton.
class TaskManager {
 public:
  // A resource represents one row in the task manager.
  // Resources from similar processes are grouped together by the task manager.
  class Resource {
   public:
    virtual ~Resource() {}

    virtual std::wstring GetTitle() const = 0;
    virtual SkBitmap GetIcon() const = 0;
    virtual base::ProcessHandle GetProcess() const = 0;

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
    virtual ~ResourceProvider() {}

    // Should return the resource associated to the specified ids, or NULL if
    // the resource does not belong to this provider.
    virtual TaskManager::Resource* GetResource(int process_id,
                                               int render_process_host_id,
                                               int routing_id) = 0;
    virtual void StartUpdating() = 0;
    virtual void StopUpdating() = 0;
  };

  static void RegisterPrefs(PrefService* prefs);

  // Returns true if the process at the specified index is the browser process.
  bool IsBrowserProcess(int index) const;

  // Terminates the process at the specified index.
  void KillProcess(int index);

  // Activates the browser tab associated with the process in the specified
  // index.
  void ActivateProcess(int index);

  void AddResourceProvider(ResourceProvider* provider);
  void RemoveResourceProvider(ResourceProvider* provider);

  // These methods are invoked by the resource providers to add/remove resources
  // to the Task Manager. Note that the resources are owned by the
  // ResourceProviders and are not valid after StopUpdating() has been called
  // on the ResourceProviders.
  void AddResource(Resource* resource);
  void RemoveResource(Resource* resource);

  void OnWindowClosed();

  // Returns the singleton instance (and initializes it if necessary).
  static TaskManager* GetInstance();

  TaskManagerModel* model() const { return model_.get(); }

 private:
  FRIEND_TEST(TaskManagerTest, Basic);
  FRIEND_TEST(TaskManagerTest, Resources);

  // Obtain an instance via GetInstance().
  TaskManager();
  friend struct DefaultSingletonTraits<TaskManager>;

  ~TaskManager();

  // The model used for gathering and processing task data. It is ref counted
  // because it is passed as a parameter to MessageLoop::InvokeLater().
  scoped_refptr<TaskManagerModel> model_;

  DISALLOW_COPY_AND_ASSIGN(TaskManager);
};

class TaskManagerModelObserver {
 public:
  virtual ~TaskManagerModelObserver() {}

  // Invoked when the model has been completely changed.
  virtual void OnModelChanged() = 0;

  // Invoked when a range of items has changed.
  virtual void OnItemsChanged(int start, int length) = 0;

  // Invoked when new items are added.
  virtual void OnItemsAdded(int start, int length) = 0;

  // Invoked when a range of items has been removed.
  virtual void OnItemsRemoved(int start, int length) = 0;
};

// The model that the TaskManager is using.
class TaskManagerModel : public URLRequestJobTracker::JobObserver,
                         public base::RefCounted<TaskManagerModel> {
 public:
  explicit TaskManagerModel(TaskManager* task_manager);
  ~TaskManagerModel();

  // Set object to be notified on model changes.
  void SetObserver(TaskManagerModelObserver* observer);

  // Returns number of registered resources.
  int ResourceCount() const;

  // Methods to return formatted resource information.
  std::wstring GetResourceTitle(int index) const;
  std::wstring GetResourceNetworkUsage(int index) const;
  std::wstring GetResourceCPUUsage(int index) const;
  std::wstring GetResourcePrivateMemory(int index) const;
  std::wstring GetResourceSharedMemory(int index) const;
  std::wstring GetResourcePhysicalMemory(int index) const;
  std::wstring GetResourceProcessId(int index) const;
  std::wstring GetResourceStatsValue(int index, int col_id) const;
  std::wstring GetResourceGoatsTeleported(int index) const;

  // Returns true if the resource is first in its group (resources
  // rendered by the same process are groupped together).
  bool IsResourceFirstInGroup(int index) const;

  // Returns icon to be used for resource (for example a favicon).
  SkBitmap GetResourceIcon(int index) const;

  // Returns a pair (start, length) of the group range of resource.
  std::pair<int, int> GetGroupRangeForResource(int index) const;

  // Compares values in column |col_id| and rows |row1|, |row2|.
  // Returns -1 if value in |row1| is less than value in |row2|,
  // 0 if they are equal, and 1 otherwise.
  int CompareValues(int row1, int row2, int col_id) const;

  // Returns process handle for given resource.
  base::ProcessHandle GetResourceProcessHandle(int index) const;

  // Returns TabContents of given resource or NULL if not applicable.
  TabContents* GetResourceTabContents(int index) const;

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

  void StartUpdating();
  void StopUpdating();

  void Clear();  // Removes all items.

 private:
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

  typedef std::vector<TaskManager::Resource*> ResourceList;
  typedef std::vector<TaskManager::ResourceProvider*> ResourceProviderList;
  typedef std::map<base::ProcessHandle, ResourceList*> GroupMap;
  typedef std::map<base::ProcessHandle, base::ProcessMetrics*> MetricsMap;
  typedef std::map<base::ProcessHandle, int> CPUUsageMap;
  typedef std::map<TaskManager::Resource*, int64> ResourceValueMap;

  // Updates the values for all rows.
  void Refresh();

  void AddItem(TaskManager::Resource* resource, bool notify_table);
  void RemoveItem(TaskManager::Resource* resource);

  // Register for network usage updates
  void RegisterForJobDoneNotifications();
  void UnregisterForJobDoneNotifications();

  // Returns the network usage (in bytes per seconds) for the specified
  // resource. That's the value retrieved at the last timer's tick.
  int64 GetNetworkUsageForResource(TaskManager::Resource* resource) const;

  // Called on the UI thread when some bytes are read.
  void BytesRead(BytesReadParam param);

  // Returns the network usage (in byte per second) that should be displayed for
  // the passed |resource|.  -1 means the information is not available for that
  // resource.
  int64 GetNetworkUsage(TaskManager::Resource* resource) const;

  // Returns the CPU usage (in %) that should be displayed for the passed
  // |resource|.
  int GetCPUUsage(TaskManager::Resource* resource) const;

  // Retrieves the private memory (in KB) that should be displayed from the
  // passed |process_metrics|.
  size_t GetPrivateMemory(const base::ProcessMetrics* process_metrics) const;

  // Returns the shared memory (in KB) that should be displayed from the passed
  // |process_metrics|.
  size_t GetSharedMemory(const base::ProcessMetrics* process_metrics) const;

  // Returns the pysical memory (in KB) that should be displayed from the passed
  // |process_metrics|.
  size_t GetPhysicalMemory(const base::ProcessMetrics* process_metrics) const;

  // Returns the stat value at the column |col_id| that should be displayed from
  // the passed |process_metrics|.
  int GetStatsValue(const TaskManager::Resource* resource, int col_id) const;

  // Retrieves the ProcessMetrics for the resources at the specified rows.
  // Returns true if there was a ProcessMetrics available for both rows.
  bool GetProcessMetricsForRows(int row1,
                                int row2,
                                base::ProcessMetrics** proc_metrics1,
                                base::ProcessMetrics** proc_metrics2) const;

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

  TaskManagerModelObserver* observer_;

  MessageLoop* ui_loop_;

  // Whether we are currently in the process of updating.
  UpdateState update_state_;

  // See design doc at http://go/at-teleporter for more information.
  static int goats_teleported_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerModel);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_H_
