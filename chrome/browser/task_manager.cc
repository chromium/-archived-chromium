// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/process_util.h"
#include "base/stats_table.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/task_manager_resource_providers.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "views/accelerator.h"
#include "views/background.h"
#include "views/controls/button/native_button.h"
#include "views/controls/link.h"
#include "views/controls/menu/menu.h"
#include "views/controls/table/group_table_view.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

// The task manager window default size.
static const int kDefaultWidth = 460;
static const int kDefaultHeight = 270;

// The delay between updates of the information (in ms).
static const int kUpdateTimeMs = 1000;

// An id for the most important column, made sufficiently large so as not to
// collide with anything else.
static const int64 kNuthMagicNumber = 1737350766;
static const int kBitMask = 0x7FFFFFFF;
static const int kGoatsTeleportedColumn =
    (94024 * kNuthMagicNumber) & kBitMask;

template <class T>
static int ValueCompare(T value1, T value2) {
  if (value1 < value2)
    return -1;
  if (value1 == value2)
    return 0;
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerModel class
////////////////////////////////////////////////////////////////////////////////

// static
int TaskManagerModel::goats_teleported_ = 0;

TaskManagerModel::TaskManagerModel(TaskManager* task_manager)
    : observer_(NULL),
      ui_loop_(MessageLoop::current()),
      update_state_(IDLE) {

  TaskManagerBrowserProcessResourceProvider* browser_provider =
      new TaskManagerBrowserProcessResourceProvider(task_manager);
  browser_provider->AddRef();
  providers_.push_back(browser_provider);
  TaskManagerTabContentsResourceProvider* wc_provider =
      new TaskManagerTabContentsResourceProvider(task_manager);
  wc_provider->AddRef();
  providers_.push_back(wc_provider);
  TaskManagerChildProcessResourceProvider* child_process_provider =
      new TaskManagerChildProcessResourceProvider(task_manager);
  child_process_provider->AddRef();
  providers_.push_back(child_process_provider);
}

TaskManagerModel::~TaskManagerModel() {
  for (ResourceProviderList::iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    (*iter)->Release();
  }
}

int TaskManagerModel::ResourceCount() const {
  return resources_.size();
}

void TaskManagerModel::SetObserver(TaskManagerModelObserver* observer) {
  DCHECK(!observer_) << "can set observer only once";
  observer_ = observer;
}

std::wstring TaskManagerModel::GetResourceTitle(int index) const {
  DCHECK(index < ResourceCount());
  return resources_[index]->GetTitle();
}

std::wstring TaskManagerModel::GetResourceNetworkUsage(int index) const {
  DCHECK(index < ResourceCount());
  int64 net_usage = GetNetworkUsage(resources_[index]);
  if (net_usage == -1)
    return l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT);
  if (net_usage == 0)
    return std::wstring(L"0");
  std::wstring net_byte =
      FormatSpeed(net_usage, GetByteDisplayUnits(net_usage), true);
  // Force number string to have LTR directionality.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    l10n_util::WrapStringWithLTRFormatting(&net_byte);
  return net_byte;
}

std::wstring TaskManagerModel::GetResourceCPUUsage(int index) const {
  DCHECK(index < ResourceCount());
  return IntToWString(GetCPUUsage(resources_[index]));
}

std::wstring TaskManagerModel::GetResourcePrivateMemory(int index) const {
  DCHECK(index < ResourceCount());
  // We report committed (working set + paged) private usage. This is NOT
  // going to match what Windows Task Manager shows (which is working set).
  MetricsMap::const_iterator iter =
      metrics_map_.find(resources_[index]->GetProcess());
  DCHECK(iter != metrics_map_.end());
  base::ProcessMetrics* process_metrics = iter->second;
  std::wstring number = FormatNumber(GetPrivateMemory(process_metrics));
  return GetMemCellText(&number);
}

std::wstring TaskManagerModel::GetResourceSharedMemory(int index) const {
  DCHECK(index < ResourceCount());
  MetricsMap::const_iterator iter =
      metrics_map_.find(resources_[index]->GetProcess());
  DCHECK(iter != metrics_map_.end());
  base::ProcessMetrics* process_metrics = iter->second;
  std::wstring number = FormatNumber(GetSharedMemory(process_metrics));
  return GetMemCellText(&number);
}

std::wstring TaskManagerModel::GetResourcePhysicalMemory(int index) const {
  DCHECK(index < ResourceCount());
  MetricsMap::const_iterator iter =
      metrics_map_.find(resources_[index]->GetProcess());
  DCHECK(iter != metrics_map_.end());
  base::ProcessMetrics* process_metrics = iter->second;
  std::wstring number = FormatNumber(GetPhysicalMemory(process_metrics));
  return GetMemCellText(&number);
}

std::wstring TaskManagerModel::GetResourceProcessId(int index) const {
  DCHECK(index < ResourceCount());
  return IntToWString(base::GetProcId(resources_[index]->GetProcess()));
}

std::wstring TaskManagerModel::GetResourceStatsValue(int index, int col_id)
    const {
  DCHECK(index < ResourceCount());
  return IntToWString(GetStatsValue(resources_[index], col_id));
}

std::wstring TaskManagerModel::GetResourceGoatsTeleported(int index) const {
  DCHECK(index < ResourceCount());
  goats_teleported_ += rand();
  return FormatNumber(goats_teleported_);
}

bool TaskManagerModel::IsResourceFirstInGroup(int index) const {
  DCHECK(index < ResourceCount());
  TaskManager::Resource* resource = resources_[index];
  GroupMap::const_iterator iter = group_map_.find(resource->GetProcess());
  DCHECK(iter != group_map_.end());
  const ResourceList* group = iter->second;
  return ((*group)[0] == resource);
}

SkBitmap TaskManagerModel::GetResourceIcon(int index) const {
  DCHECK(index < ResourceCount());
  SkBitmap icon = resources_[index]->GetIcon();
  if (!icon.isNull())
    return icon;

  static SkBitmap* default_icon = ResourceBundle::GetSharedInstance().
      GetBitmapNamed(IDR_DEFAULT_FAVICON);
  return *default_icon;
}

std::pair<int, int> TaskManagerModel::GetGroupRangeForResource(int index)
    const {
  DCHECK(index < ResourceCount());
  TaskManager::Resource* resource = resources_[index];
  GroupMap::const_iterator group_iter =
      group_map_.find(resource->GetProcess());
  DCHECK(group_iter != group_map_.end());
  ResourceList* group = group_iter->second;
  DCHECK(group);
  if (group->size() == 1) {
    return std::make_pair(index, 1);
  } else {
    ResourceList::const_iterator iter =
        std::find(resources_.begin(), resources_.end(), (*group)[0]);
    DCHECK(iter != resources_.end());
    return std::make_pair(iter - resources_.begin(), group->size());
  }
}

int TaskManagerModel::CompareValues(int row1, int row2, int col_id) const {
  DCHECK(row1 < ResourceCount() && row2 < ResourceCount());
  switch (col_id) {
    case IDS_TASK_MANAGER_PAGE_COLUMN: {
      // Let's do the default, string compare on the resource title.
      static Collator* collator = NULL;
      if (!collator) {
        UErrorCode create_status = U_ZERO_ERROR;
        collator = Collator::createInstance(create_status);
        if (!U_SUCCESS(create_status)) {
          collator = NULL;
          NOTREACHED();
        }
      }
      std::wstring title1 = GetResourceTitle(row1);
      std::wstring title2 = GetResourceTitle(row2);
      UErrorCode compare_status = U_ZERO_ERROR;
      UCollationResult compare_result = collator->compare(
          static_cast<const UChar*>(title1.c_str()),
          static_cast<int>(title1.length()),
          static_cast<const UChar*>(title2.c_str()),
          static_cast<int>(title2.length()),
          compare_status);
      DCHECK(U_SUCCESS(compare_status));
      return compare_result;
    }

    case IDS_TASK_MANAGER_NET_COLUMN:
      return ValueCompare<int64>(GetNetworkUsage(resources_[row1]),
                                 GetNetworkUsage(resources_[row2]));

    case IDS_TASK_MANAGER_CPU_COLUMN:
      return ValueCompare<int>(GetCPUUsage(resources_[row1]),
                               GetCPUUsage(resources_[row2]));

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN: {
      base::ProcessMetrics* pm1;
      base::ProcessMetrics* pm2;
      if (!GetProcessMetricsForRows(row1, row2, &pm1, &pm2))
        return 0;
      return ValueCompare<size_t>(GetPrivateMemory(pm1),
                                  GetPrivateMemory(pm2));
    }

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN: {
      base::ProcessMetrics* pm1;
      base::ProcessMetrics* pm2;
      if (!GetProcessMetricsForRows(row1, row2, &pm1, &pm2))
        return 0;
      return ValueCompare<size_t>(GetSharedMemory(pm1),
                                  GetSharedMemory(pm2));
    }

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN: {
      base::ProcessMetrics* pm1;
      base::ProcessMetrics* pm2;
      if (!GetProcessMetricsForRows(row1, row2, &pm1, &pm2))
        return 0;
      return ValueCompare<size_t>(GetPhysicalMemory(pm1),
                                  GetPhysicalMemory(pm2));
    }

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN: {
      int proc1_id = base::GetProcId(resources_[row1]->GetProcess());
      int proc2_id = base::GetProcId(resources_[row2]->GetProcess());
      return ValueCompare<int>(proc1_id, proc2_id);
    }

    default:
      return ValueCompare<int>(GetStatsValue(resources_[row1], col_id),
                               GetStatsValue(resources_[row2], col_id));
  }
}

base::ProcessHandle TaskManagerModel::GetResourceProcessHandle(int index)
    const {
  DCHECK(index < ResourceCount());
  return resources_[index]->GetProcess();
}

TabContents* TaskManagerModel::GetResourceTabContents(int index) const {
  DCHECK(index < ResourceCount());
  return resources_[index]->GetTabContents();
}

int64 TaskManagerModel::GetNetworkUsage(TaskManager::Resource* resource)
    const {
  int64 net_usage = GetNetworkUsageForResource(resource);
  if (net_usage == 0 && !resource->SupportNetworkUsage())
    return -1;
  return net_usage;
}

int TaskManagerModel::GetCPUUsage(TaskManager::Resource* resource) const {
  CPUUsageMap::const_iterator iter =
      cpu_usage_map_.find(resource->GetProcess());
  if (iter == cpu_usage_map_.end())
    return 0;
  return iter->second;
}

size_t TaskManagerModel::GetPrivateMemory(
    const base::ProcessMetrics* process_metrics) const {
  return process_metrics->GetPrivateBytes() / 1024;
}

size_t TaskManagerModel::GetSharedMemory(
    const base::ProcessMetrics* process_metrics) const {
  base::WorkingSetKBytes ws_usage;
  process_metrics->GetWorkingSetKBytes(&ws_usage);
  return ws_usage.shared;
}

size_t TaskManagerModel::GetPhysicalMemory(
    const base::ProcessMetrics* process_metrics) const {
  // Memory = working_set.private + working_set.shareable.
  // We exclude the shared memory.
  size_t total_kbytes = process_metrics->GetWorkingSetSize() / 1024;
  base::WorkingSetKBytes ws_usage;
  process_metrics->GetWorkingSetKBytes(&ws_usage);
  total_kbytes -= ws_usage.shared;
  return total_kbytes;
}

int TaskManagerModel::GetStatsValue(const TaskManager::Resource* resource,
                                         int col_id) const {
  StatsTable* table = StatsTable::current();
  if (table != NULL) {
    const char* counter = table->GetRowName(col_id);
    if (counter != NULL && counter[0] != '\0') {
      return table->GetCounterValue(counter,
          base::GetProcId(resource->GetProcess()));
     } else {
        NOTREACHED() << "Invalid column.";
     }
  }
  return 0;
}

std::wstring TaskManagerModel::GetMemCellText(
    std::wstring* number) const {
  // Adjust number string if necessary.
  l10n_util::AdjustStringForLocaleDirection(*number, number);
  return l10n_util::GetStringF(IDS_TASK_MANAGER_MEM_CELL_TEXT, *number);
}

void TaskManagerModel::StartUpdating() {
  DCHECK_NE(TASK_PENDING, update_state_);

  // If update_state_ is STOPPING, it means a task is still pending.  Setting
  // it to TASK_PENDING ensures the tasks keep being posted (by Refresh()).
  if (update_state_ == IDLE) {
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          NewRunnableMethod(this, &TaskManagerModel::Refresh),
          kUpdateTimeMs);
  }
  update_state_ = TASK_PENDING;

  // Register jobs notifications so we can compute network usage (it must be
  // done from the IO thread).
  base::Thread* thread = g_browser_process->io_thread();
  if (thread)
    thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &TaskManagerModel::RegisterForJobDoneNotifications));

  // Notify resource providers that we are updating.
  for (ResourceProviderList::iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    (*iter)->StartUpdating();
  }
}

void TaskManagerModel::StopUpdating() {
  DCHECK_EQ(TASK_PENDING, update_state_);
  update_state_ = STOPPING;

  // Notify resource providers that we are done updating.
  for (ResourceProviderList::const_iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    (*iter)->StopUpdating();
  }

  // Unregister jobs notification (must be done from the IO thread).
  base::Thread* thread = g_browser_process->io_thread();
  if (thread)
    thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &TaskManagerModel::UnregisterForJobDoneNotifications));
}

void TaskManagerModel::AddResourceProvider(
    TaskManager::ResourceProvider* provider) {
  DCHECK(provider);
  providers_.push_back(provider);
}

void TaskManagerModel::RemoveResourceProvider(
    TaskManager::ResourceProvider* provider) {
  DCHECK(provider);
  ResourceProviderList::iterator iter = std::find(providers_.begin(),
                                                  providers_.end(),
                                                  provider);
  DCHECK(iter != providers_.end());
  providers_.erase(iter);
}

void TaskManagerModel::RegisterForJobDoneNotifications() {
  g_url_request_job_tracker.AddObserver(this);
}

void TaskManagerModel::UnregisterForJobDoneNotifications() {
  g_url_request_job_tracker.RemoveObserver(this);
}

void TaskManagerModel::AddResource(TaskManager::Resource* resource) {
  base::ProcessHandle process = resource->GetProcess();

  ResourceList* group_entries = NULL;
  GroupMap::const_iterator group_iter = group_map_.find(process);
  int new_entry_index = 0;
  if (group_iter == group_map_.end()) {
    group_entries = new ResourceList();
    group_map_[process] = group_entries;
    group_entries->push_back(resource);

    // Not part of a group, just put at the end of the list.
    resources_.push_back(resource);
    new_entry_index = static_cast<int>(resources_.size() - 1);
  } else {
    group_entries = group_iter->second;
    group_entries->push_back(resource);

    // Insert the new entry right after the last entry of its group.
    ResourceList::iterator iter =
        std::find(resources_.begin(),
                  resources_.end(),
                  (*group_entries)[group_entries->size() - 2]);
    DCHECK(iter != resources_.end());
    new_entry_index = static_cast<int>(iter - resources_.begin());
    resources_.insert(++iter, resource);
  }

  // Create the ProcessMetrics for this process if needed (not in map).
  if (metrics_map_.find(process) == metrics_map_.end()) {
    base::ProcessMetrics* pm =
        base::ProcessMetrics::CreateProcessMetrics(process);
    metrics_map_[process] = pm;
  }

  // Notify the table that the contents have changed for it to redraw.
  DCHECK(observer_);
  observer_->OnItemsAdded(new_entry_index, 1);
}

void TaskManagerModel::RemoveResource(TaskManager::Resource* resource) {
  base::ProcessHandle process = resource->GetProcess();

  // Find the associated group.
  GroupMap::iterator group_iter = group_map_.find(process);
  DCHECK(group_iter != group_map_.end());
  ResourceList* group_entries = group_iter->second;

  // Remove the entry from the group map.
  ResourceList::iterator iter = std::find(group_entries->begin(),
                                          group_entries->end(),
                                          resource);
  DCHECK(iter != group_entries->end());
  group_entries->erase(iter);

  // If there are no more entries for that process, do the clean-up.
  if (group_entries->empty()) {
    delete group_entries;
    group_map_.erase(process);

    // Nobody is using this process, we don't need the process metrics anymore.
    MetricsMap::iterator pm_iter = metrics_map_.find(process);
    DCHECK(pm_iter != metrics_map_.end());
    if (pm_iter != metrics_map_.end()) {
      delete pm_iter->second;
      metrics_map_.erase(process);
    }
    // And we don't need the CPU usage anymore either.
    CPUUsageMap::iterator cpu_iter = cpu_usage_map_.find(process);
    if (cpu_iter != cpu_usage_map_.end())
      cpu_usage_map_.erase(cpu_iter);
  }

  // Remove the entry from the model list.
  iter = std::find(resources_.begin(), resources_.end(), resource);
  DCHECK(iter != resources_.end());
  int index = static_cast<int>(iter - resources_.begin());
  resources_.erase(iter);

  // Remove the entry from the network maps.
  ResourceValueMap::iterator net_iter =
      current_byte_count_map_.find(resource);
  if (net_iter != current_byte_count_map_.end())
    current_byte_count_map_.erase(net_iter);
  net_iter = displayed_network_usage_map_.find(resource);
  if (net_iter != displayed_network_usage_map_.end())
    displayed_network_usage_map_.erase(net_iter);

  // Notify the table that the contents have changed.
  observer_->OnItemsRemoved(index, 1);
}

void TaskManagerModel::Clear() {
  int size = ResourceCount();
  if (size > 0) {
    resources_.clear();

    // Clear the groups.
    for (GroupMap::iterator iter = group_map_.begin();
         iter != group_map_.end(); ++iter) {
      delete iter->second;
    }
    group_map_.clear();

    // Clear the process related info.
    for (MetricsMap::iterator iter = metrics_map_.begin();
         iter != metrics_map_.end(); ++iter) {
      delete iter->second;
    }
    metrics_map_.clear();
    cpu_usage_map_.clear();

    // Clear the network maps.
    current_byte_count_map_.clear();
    displayed_network_usage_map_.clear();

    observer_->OnItemsRemoved(0, size);
  }
}

void TaskManagerModel::Refresh() {
  DCHECK_NE(IDLE, update_state_);

  if (update_state_ == STOPPING) {
    // We have been asked to stop.
    update_state_ = IDLE;
    return;
  }

  // Compute the CPU usage values.
  // Note that we compute the CPU usage for all resources (instead of doing it
  // lazily) as process_util::GetCPUUsage() returns the CPU usage since the last
  // time it was called, and not calling it everytime would skew the value the
  // next time it is retrieved (as it would be for more than 1 cycle).
  cpu_usage_map_.clear();
  for (ResourceList::iterator iter = resources_.begin();
       iter != resources_.end(); ++iter) {
    base::ProcessHandle process = (*iter)->GetProcess();
    CPUUsageMap::iterator cpu_iter = cpu_usage_map_.find(process);
    if (cpu_iter != cpu_usage_map_.end())
      continue;  // Already computed.

    MetricsMap::iterator metrics_iter = metrics_map_.find(process);
    DCHECK(metrics_iter != metrics_map_.end());
    cpu_usage_map_[process] = metrics_iter->second->GetCPUUsage();
  }

  // Compute the new network usage values.
  displayed_network_usage_map_.clear();
  for (ResourceValueMap::iterator iter = current_byte_count_map_.begin();
       iter != current_byte_count_map_.end(); ++iter) {
    if (kUpdateTimeMs > 1000) {
      int divider = (kUpdateTimeMs / 1000);
      displayed_network_usage_map_[iter->first] = iter->second / divider;
    } else {
      displayed_network_usage_map_[iter->first] = iter->second *
          (1000 / kUpdateTimeMs);
    }

    // Then we reset the current byte count.
    iter->second = 0;
  }
  if (!resources_.empty())
    observer_->OnItemsChanged(0, ResourceCount());

  // Schedule the next update.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      NewRunnableMethod(this, &TaskManagerModel::Refresh),
      kUpdateTimeMs);
}

int64 TaskManagerModel::GetNetworkUsageForResource(
    TaskManager::Resource* resource) const {
  ResourceValueMap::const_iterator iter =
      displayed_network_usage_map_.find(resource);
  if (iter == displayed_network_usage_map_.end())
    return 0;
  return iter->second;
}

void TaskManagerModel::BytesRead(BytesReadParam param) {
  if (update_state_ != TASK_PENDING) {
    // A notification sneaked in while we were stopping the updating, just
    // ignore it.
    return;
  }

  if (param.byte_count == 0) {
    // Nothing to do if no bytes were actually read.
    return;
  }

  // TODO(jcampan): this should be improved once we have a better way of
  // linking a network notification back to the object that initiated it.
  TaskManager::Resource* resource = NULL;
  for (ResourceProviderList::iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    resource = (*iter)->GetResource(param.origin_pid,
                                    param.render_process_host_id,
                                    param.routing_id);
    if (resource)
      break;
  }
  if (resource == NULL) {
    // We may not have that resource anymore (example: close a tab while a
    // a network resource is being retrieved), in which case we just ignore the
    // notification.
    return;
  }

  // We do support network usage, mark the resource as such so it can report 0
  // instead of N/A.
  if (!resource->SupportNetworkUsage())
    resource->SetSupportNetworkUsage();

  ResourceValueMap::const_iterator iter_res =
      current_byte_count_map_.find(resource);
  if (iter_res == current_byte_count_map_.end())
    current_byte_count_map_[resource] = param.byte_count;
  else
    current_byte_count_map_[resource] = iter_res->second + param.byte_count;
}


// In order to retrieve the network usage, we register for URLRequestJob
// notifications. Every time we get notified some bytes were read we bump a
// counter of read bytes for the associated resource. When the timer ticks,
// we'll compute the actual network usage (see the Refresh method).
void TaskManagerModel::OnJobAdded(URLRequestJob* job) {
}

void TaskManagerModel::OnJobRemoved(URLRequestJob* job) {
}

void TaskManagerModel::OnJobDone(URLRequestJob* job,
                                      const URLRequestStatus& status) {
}

void TaskManagerModel::OnJobRedirect(URLRequestJob* job,
                                          const GURL& location,
                                          int status_code) {
}

void TaskManagerModel::OnBytesRead(URLRequestJob* job, int byte_count) {
  int render_process_host_id = -1, routing_id = -1;
  tab_util::GetTabContentsID(job->request(),
                             &render_process_host_id, &routing_id);
  // This happens in the IO thread, post it to the UI thread.
  ui_loop_->PostTask(FROM_HERE,
                     NewRunnableMethod(
                        this,
                        &TaskManagerModel::BytesRead,
                        BytesReadParam(job->request()->origin_pid(),
                                       render_process_host_id, routing_id,
                                       byte_count)));
}

bool TaskManagerModel::GetProcessMetricsForRows(
    int row1, int row2,
    base::ProcessMetrics** proc_metrics1,
    base::ProcessMetrics** proc_metrics2) const {
  DCHECK(row1 < ResourceCount() && row2 < ResourceCount());
  *proc_metrics1 = NULL;
  *proc_metrics2 = NULL;

  MetricsMap::const_iterator iter =
      metrics_map_.find(resources_[row1]->GetProcess());
  if (iter == metrics_map_.end())
    return false;
  *proc_metrics1 = iter->second;

  iter = metrics_map_.find(resources_[row2]->GetProcess());
  if (iter == metrics_map_.end())
    return false;
  *proc_metrics2 = iter->second;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerTableModel class
////////////////////////////////////////////////////////////////////////////////

class TaskManagerTableModel : public views::GroupTableModel,
                              public TaskManagerModelObserver {
 public:
  explicit TaskManagerTableModel(TaskManagerModel* model)
      : model_(model),
        observer_(NULL) {
    model->SetObserver(this);
  }
  ~TaskManagerTableModel() {}

  // GroupTableModel.
  int RowCount();
  std::wstring GetText(int row, int column);
  SkBitmap GetIcon(int row);
  void GetGroupRangeForItem(int item, views::GroupRange* range);
  void SetObserver(views::TableModelObserver* observer);
  virtual int CompareValues(int row1, int row2, int column_id);

  // TaskManagerModelObserver.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

 private:
  const TaskManagerModel* model_;
  views::TableModelObserver* observer_;
};

int TaskManagerTableModel::RowCount() {
  return model_->ResourceCount();
}

std::wstring TaskManagerTableModel::GetText(int row, int col_id) {
  switch (col_id) {
    case IDS_TASK_MANAGER_PAGE_COLUMN:  // Process
      return model_->GetResourceTitle(row);

    case IDS_TASK_MANAGER_NET_COLUMN:  // Net
      return model_->GetResourceNetworkUsage(row);

    case IDS_TASK_MANAGER_CPU_COLUMN:  // CPU
      if (!model_->IsResourceFirstInGroup(row))
        return std::wstring();
      return model_->GetResourceCPUUsage(row);

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::wstring();
      return model_->GetResourcePrivateMemory(row);

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::wstring();
      return model_->GetResourceSharedMemory(row);

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::wstring();
      return model_->GetResourcePhysicalMemory(row);

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      if (!model_->IsResourceFirstInGroup(row))
        return std::wstring();
      return model_->GetResourceProcessId(row);

    case kGoatsTeleportedColumn:  // Goats Teleported!
      return model_->GetResourceGoatsTeleported(row);

    default:
      return model_->GetResourceStatsValue(row, col_id);
  }
}

SkBitmap TaskManagerTableModel::GetIcon(int row) {
  return model_->GetResourceIcon(row);
}

void TaskManagerTableModel::GetGroupRangeForItem(int item,
                                                 views::GroupRange* range) {
  std::pair<int, int> range_pair = model_->GetGroupRangeForResource(item);
  range->start = range_pair.first;
  range->length = range_pair.second;
}

void TaskManagerTableModel::SetObserver(views::TableModelObserver* observer) {
  observer_ = observer;
}

int TaskManagerTableModel::CompareValues(int row1, int row2, int column_id) {
  return model_->CompareValues(row1, row2, column_id);
}

void TaskManagerTableModel::OnModelChanged() {
  if (observer_)
    observer_->OnModelChanged();
}

void TaskManagerTableModel::OnItemsChanged(int start, int length) {
  if (observer_)
    observer_->OnItemsChanged(start, length);
}

void TaskManagerTableModel::OnItemsAdded(int start, int length) {
  if (observer_)
    observer_->OnItemsAdded(start, length);
}

void TaskManagerTableModel::OnItemsRemoved(int start, int length) {
  if (observer_)
    observer_->OnItemsRemoved(start, length);
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerContents class
//
// The view containing the different widgets.
//
////////////////////////////////////////////////////////////////////////////////

class TaskManagerContents : public views::View,
                            public views::ButtonListener,
                            public views::DialogDelegate,
                            public views::TableViewObserver,
                            public views::LinkController,
                            public views::ContextMenuController,
                            public Menu::Delegate {
 public:
  TaskManagerContents(TaskManager* task_manager,
                      TaskManagerModel* model);
  virtual ~TaskManagerContents();

  void Init(TaskManagerModel* model);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);
  void GetSelection(std::vector<int>* selection);
  void GetFocused(std::vector<int>* focused);

  // ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender);

  // views::DialogDelegate
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetWindowName() const;
  virtual int GetDialogButtons() const;
  virtual void WindowClosing();
  virtual void DeleteDelegate();
  virtual views::View* GetContentsView();

  // views::TableViewObserver implementation.
  virtual void OnSelectionChanged();
  virtual void OnDoubleClick();
  virtual void OnKeyDown(unsigned short virtual_keycode);

  // views::LinkController implementation.
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Called by the column picker to pick up any new stat counters that
  // may have appeared since last time.
  void UpdateStatsCounters();

  // Menu::Delegate
  virtual void ShowContextMenu(views::View* source,
                               int x,
                               int y,
                               bool is_mouse_gesture);
  virtual bool IsItemChecked(int id) const;
  virtual void ExecuteCommand(int id);

 private:
  scoped_ptr<views::NativeButton> kill_button_;
  scoped_ptr<views::Link> about_memory_link_;
  views::GroupTableView* tab_table_;

  TaskManager* task_manager_;

  // all possible columns, not necessarily visible
  std::vector<views::TableColumn> columns_;

  scoped_ptr<TaskManagerTableModel> table_model_;

  DISALLOW_EVIL_CONSTRUCTORS(TaskManagerContents);
};

TaskManagerContents::TaskManagerContents(TaskManager* task_manager,
                                         TaskManagerModel* model)
    : task_manager_(task_manager) {
  Init(model);
}

TaskManagerContents::~TaskManagerContents() {
}

void TaskManagerContents::Init(TaskManagerModel* model) {
  table_model_.reset(new TaskManagerTableModel(model));

  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_PAGE_COLUMN,
                                        views::TableColumn::LEFT, -1, 1));
  columns_.back().sortable = true;
  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN,
                                        views::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_SHARED_MEM_COLUMN,
                                        views::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN,
                                        views::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_CPU_COLUMN,
                                        views::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_NET_COLUMN,
                                        views::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.push_back(views::TableColumn(IDS_TASK_MANAGER_PROCESS_ID_COLUMN,
                                        views::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;

  tab_table_ = new views::GroupTableView(table_model_.get(), columns_,
                                         views::ICON_AND_TEXT, false, true,
                                         true);

  // Hide some columns by default
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PROCESS_ID_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_SHARED_MEM_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN, false);

  UpdateStatsCounters();
  views::TableColumn col(kGoatsTeleportedColumn, L"Goats Teleported",
                         views::TableColumn::RIGHT, -1, 0);
  col.sortable = true;
  columns_.push_back(col);
  tab_table_->AddColumn(col);
  tab_table_->SetObserver(this);
  SetContextMenuController(this);
  kill_button_.reset(new views::NativeButton(
      this, l10n_util::GetString(IDS_TASK_MANAGER_KILL)));
  kill_button_->AddAccelerator(views::Accelerator('E', false, false, false));
  kill_button_->SetAccessibleKeyboardShortcut(L"E");
  about_memory_link_.reset(new views::Link(
      l10n_util::GetString(IDS_TASK_MANAGER_ABOUT_MEMORY_LINK)));
  about_memory_link_->SetController(this);

  AddChildView(tab_table_);

  // Makes sure our state is consistent.
  OnSelectionChanged();
}

void TaskManagerContents::UpdateStatsCounters() {
  StatsTable* stats = StatsTable::current();
  if (stats != NULL) {
    int max = stats->GetMaxCounters();
    // skip the first row (it's header data)
    for (int i = 1; i < max; i++) {
      const char* row = stats->GetRowName(i);
      if (row != NULL && row[0] != '\0' && !tab_table_->HasColumn(i)) {
        // TODO(erikkay): Use l10n to get display names for stats.  Right
        // now we're just displaying the internal counter name.  Perhaps
        // stat names not in the string table would be filtered out.
        // TODO(erikkay): Width is hard-coded right now, so many column
        // names are clipped.
        views::TableColumn col(i, ASCIIToWide(row), views::TableColumn::RIGHT,
                               90, 0);
        col.sortable = true;
        columns_.push_back(col);
        tab_table_->AddColumn(col);
      }
    }
  }
}

void TaskManagerContents::ViewHierarchyChanged(bool is_add,
                                               views::View* parent,
                                               views::View* child) {
  // Since we want the Kill button and the Memory Details link to show up in
  // the same visual row as the close button, which is provided by the
  // framework, we must add the buttons to the non-client view, which is the
  // parent of this view. Similarly, when we're removed from the view
  // hierarchy, we must take care to clean up those items as well.
  if (child == this) {
    if (is_add) {
      parent->AddChildView(kill_button_.get());
      parent->AddChildView(about_memory_link_.get());
    } else {
      parent->RemoveChildView(kill_button_.get());
      parent->RemoveChildView(about_memory_link_.get());
      // Note that these items aren't deleted here, since this object is owned
      // by the TaskManager, whose lifetime surpasses the window, and the next
      // time we are inserted into a window these items will need to be valid.
    }
  }
}

void TaskManagerContents::Layout() {
  // kPanelHorizMargin is too big.
  const int kTableButtonSpacing = 12;

  gfx::Size size = kill_button_->GetPreferredSize();
  int prefered_width = size.width();
  int prefered_height = size.height();

  tab_table_->SetBounds(x() + kPanelHorizMargin,
                        y() + kPanelVertMargin,
                        width() - 2 * kPanelHorizMargin,
                        height() - 2 * kPanelVertMargin - prefered_height);

  // y-coordinate of button top left.
  gfx::Rect parent_bounds = GetParent()->GetLocalBounds(false);
  int y_buttons = parent_bounds.bottom() - prefered_height - kButtonVEdgeMargin;

  kill_button_->SetBounds(x() + width() - prefered_width - kPanelHorizMargin,
                          y_buttons,
                          prefered_width,
                          prefered_height);

  size = about_memory_link_->GetPreferredSize();
  int link_prefered_width = size.width();
  int link_prefered_height = size.height();
  // center between the two buttons horizontally, and line up with
  // bottom of buttons vertically.
  int link_y_offset = std::max(0, prefered_height - link_prefered_height) / 2;
  about_memory_link_->SetBounds(
      x() + kPanelHorizMargin,
      y_buttons + prefered_height - link_prefered_height - link_y_offset,
      link_prefered_width,
      link_prefered_height);
}

gfx::Size TaskManagerContents::GetPreferredSize() {
  return gfx::Size(kDefaultWidth, kDefaultHeight);
}

void TaskManagerContents::GetSelection(std::vector<int>* selection) {
  DCHECK(selection);
  for (views::TableSelectionIterator iter  = tab_table_->SelectionBegin();
       iter != tab_table_->SelectionEnd(); ++iter) {
    // The TableView returns the selection starting from the end.
    selection->insert(selection->begin(), *iter);
  }
}

void TaskManagerContents::GetFocused(std::vector<int>* focused) {
  DCHECK(focused);
  int row_count = tab_table_->RowCount();
  for (int i = 0; i < row_count; ++i) {
    // The TableView returns the selection starting from the end.
    if (tab_table_->ItemHasTheFocus(i))
      focused->insert(focused->begin(), i);
  }
}

// ButtonListener implementation.
void TaskManagerContents::ButtonPressed(views::Button* sender) {
  if (sender == kill_button_.get())
    task_manager_->KillSelectedProcesses();
}

// DialogDelegate implementation.
bool TaskManagerContents::CanResize() const {
  return true;
}

bool TaskManagerContents::CanMaximize() const {
  return true;
}

bool TaskManagerContents::IsAlwaysOnTop() const {
  return true;
}

bool TaskManagerContents::HasAlwaysOnTopMenu() const {
  return true;
};

std::wstring TaskManagerContents::GetWindowTitle() const {
  return l10n_util::GetString(IDS_TASK_MANAGER_TITLE);
}

std::wstring TaskManagerContents::GetWindowName() const {
  return prefs::kTaskManagerWindowPlacement;
}

int TaskManagerContents::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_NONE;
}

void TaskManagerContents::WindowClosing() {
  // Remove the view from its parent to trigger the contents'
  // ViewHierarchyChanged notification to unhook the extra buttons from the
  // non-client view.
  GetParent()->RemoveChildView(this);
  task_manager_->Close();
}

void TaskManagerContents::DeleteDelegate() {
  ReleaseWindow();
}

views::View* TaskManagerContents::GetContentsView() {
  return this;
}

// views::TableViewObserver implementation.
void TaskManagerContents::OnSelectionChanged() {
  kill_button_->SetEnabled(!task_manager_->BrowserProcessIsSelected() &&
                           tab_table_->SelectedRowCount() > 0);
}

void TaskManagerContents::OnDoubleClick() {
  task_manager_->ActivateFocusedTab();
}

void TaskManagerContents::OnKeyDown(unsigned short virtual_keycode) {
  if (virtual_keycode == VK_RETURN)
    task_manager_->ActivateFocusedTab();
}

// views::LinkController implementation
void TaskManagerContents::LinkActivated(views::Link* source,
                                        int event_flags) {
  DCHECK(source == about_memory_link_);
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  browser->OpenURL(GURL("about:memory"), GURL(), NEW_FOREGROUND_TAB,
                   PageTransition::LINK);
  // In case the browser window is minimzed, show it.
  browser->window()->Show();
}

void TaskManagerContents::ShowContextMenu(views::View* source,
                                          int x,
                                          int y,
                                          bool is_mouse_gesture) {
  UpdateStatsCounters();
  Menu menu(this, Menu::TOPLEFT, source->GetWidget()->GetNativeView());
  for (std::vector<views::TableColumn>::iterator i =
       columns_.begin(); i != columns_.end(); ++i) {
    menu.AppendMenuItem(i->id, i->title, Menu::CHECKBOX);
  }
  menu.RunMenuAt(x, y);
}

bool TaskManagerContents::IsItemChecked(int id) const {
  return tab_table_->IsColumnVisible(id);
}

void TaskManagerContents::ExecuteCommand(int id) {
  tab_table_->SetColumnVisibility(id, !tab_table_->IsColumnVisible(id));
}

////////////////////////////////////////////////////////////////////////////////
// TaskManager class
////////////////////////////////////////////////////////////////////////////////

// static
void TaskManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kTaskManagerWindowPlacement);
}

TaskManager::TaskManager() {
  model_ = new TaskManagerModel(this);
  contents_.reset(new TaskManagerContents(this, model_.get()));
}

TaskManager::~TaskManager() {
}

// static
void TaskManager::Open() {
  TaskManager* task_manager = GetInstance();
  if (task_manager->contents_->window()) {
    task_manager->contents_->window()->Activate();
  } else {
    views::Window::CreateChromeWindow(NULL, gfx::Rect(),
                                      task_manager->contents_.get());
    task_manager->model_->StartUpdating();
    task_manager->contents_->window()->Show();
  }
}

void TaskManager::Close() {
  model_->StopUpdating();
  model_->Clear();
}

bool TaskManager::BrowserProcessIsSelected() {
  if (!contents_.get())
    return false;
  std::vector<int> selection;
  contents_->GetSelection(&selection);
  for (std::vector<int>::const_iterator iter = selection.begin();
       iter != selection.end(); ++iter) {
    // If some of the selection is out of bounds, ignore. This may happen when
    // killing a process that manages several pages.
    if (*iter >= model_->ResourceCount())
      continue;
    if (model_->GetResourceProcessHandle(*iter) == GetCurrentProcess())
      return true;
  }
  return false;
}

void TaskManager::KillSelectedProcesses() {
  std::vector<int> selection;
  contents_->GetSelection(&selection);
  for (std::vector<int>::const_iterator iter = selection.begin();
       iter != selection.end(); ++iter) {
    base::ProcessHandle process = model_->GetResourceProcessHandle(*iter);
    DCHECK(process);
    if (process == GetCurrentProcess())
      continue;
    TerminateProcess(process, 0);
  }
}

void TaskManager::ActivateFocusedTab() {
  std::vector<int> focused;
  contents_->GetFocused(&focused);
  int focused_size = static_cast<int>(focused.size());

  DCHECK(focused_size == 1);

  // Gracefully return if there is not exactly one item in focus.
  if (focused_size != 1)
    return;

  // Otherwise, the one focused thing should be one the user intends to bring
  // forth, so get see if GetTabContents returns non-null.  If it does, activate
  // those contents.
  int index = focused[0];

  // GetResourceTabContents returns a pointer to the relevant tab contents for
  // the resource.  If the index doesn't correspond to a Tab (i.e. refers to
  // the Browser process or a plugin), GetTabContents will return NULL.
  TabContents* chosen_tab_contents = model_->GetResourceTabContents(index);

  if (!chosen_tab_contents)
    return;

  chosen_tab_contents->Activate();
}

void TaskManager::AddResourceProvider(ResourceProvider* provider) {
  model_->AddResourceProvider(provider);
}

void TaskManager::RemoveResourceProvider(ResourceProvider* provider) {
  model_->RemoveResourceProvider(provider);
}

void TaskManager::AddResource(Resource* resource) {
  model_->AddResource(resource);
}

void TaskManager::RemoveResource(Resource* resource) {
  model_->RemoveResource(resource);
}

// static
TaskManager* TaskManager::GetInstance() {
  return Singleton<TaskManager>::get();
}
