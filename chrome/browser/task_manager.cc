// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/compiler_specific.h"
#include "base/process_util.h"
#include "base/stats_table.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/task_manager_resource_providers.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

// The delay between updates of the information (in ms).
static const int kUpdateTimeMs = 1000;

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
  TaskManagerExtensionProcessResourceProvider* extension_process_provider =
      new TaskManagerExtensionProcessResourceProvider(task_manager);
  extension_process_provider->AddRef();
  providers_.push_back(extension_process_provider);
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
      string16 title1 = WideToUTF16(GetResourceTitle(row1));
      string16 title2 = WideToUTF16(GetResourceTitle(row2));
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
  if (observer_)
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
  if (observer_)
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

    if (observer_)
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
  if (!resources_.empty() && observer_)
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
  ResourceDispatcherHost::RenderViewForRequest(job->request(),
                                               &render_process_host_id,
                                               &routing_id);
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
// TaskManager class
////////////////////////////////////////////////////////////////////////////////

// static
void TaskManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kTaskManagerWindowPlacement);
}

TaskManager::TaskManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(model_(new TaskManagerModel(this))) {
}

TaskManager::~TaskManager() {
}

bool TaskManager::IsBrowserProcess(int index) const {
  // If some of the selection is out of bounds, ignore. This may happen when
  // killing a process that manages several pages.
  return index < model_->ResourceCount() &&
      model_->GetResourceProcessHandle(index) ==
      base::GetCurrentProcessHandle();
}

void TaskManager::KillProcess(int index) {
  base::ProcessHandle process = model_->GetResourceProcessHandle(index);
  DCHECK(process);
  if (process != base::GetCurrentProcessHandle())
    base::KillProcess(process, base::PROCESS_END_KILLED_BY_USER, false);
}

void TaskManager::ActivateProcess(int index) {
  // GetResourceTabContents returns a pointer to the relevant tab contents for
  // the resource.  If the index doesn't correspond to a Tab (i.e. refers to
  // the Browser process or a plugin), GetTabContents will return NULL.
  TabContents* chosen_tab_contents = model_->GetResourceTabContents(index);
  if (chosen_tab_contents)
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

void TaskManager::OnWindowClosed() {
  model_->StopUpdating();
  model_->Clear();
}

// static
TaskManager* TaskManager::GetInstance() {
  return Singleton<TaskManager>::get();
}
