// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager_resource_providers.h"

#include "base/file_version_info.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/resource_message_filter.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/common/gfx/icon_util.h"

#include "generated_resources.h"

////////////////////////////////////////////////////////////////////////////////
// TaskManagerWebContentsResource class
////////////////////////////////////////////////////////////////////////////////

TaskManagerWebContentsResource::TaskManagerWebContentsResource(
    WebContents* web_contents)
    : web_contents_(web_contents) {
  // We cache the process as when the WebContents is closed the process
  // becomes NULL and the TaskManager still needs it.
  process_ = web_contents_->process()->process().handle();
  pid_ = base::GetProcId(process_);
}

TaskManagerWebContentsResource::~TaskManagerWebContentsResource() {
}

std::wstring TaskManagerWebContentsResource::GetTitle() const {
  // GetTitle() and GetURL() can only be invoked when the WebContents has a
  // controller.
  if (!web_contents_->controller())
    return std::wstring();

  // Fall back on the URL if there's no title.
  std::wstring tab_title(web_contents_->GetTitle());
  if (tab_title.empty()) {
    tab_title = UTF8ToWide(web_contents_->GetURL().spec());
    // Force URL to be LTR.
    if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
      l10n_util::WrapStringWithLTRFormatting(&tab_title);
  } else {
    // Since the tab_title will be concatenated with
    // IDS_TASK_MANAGER_TAB_PREFIX, we need to explicitly set the tab_title to
    // be LTR format if there is no strong RTL charater in it. Otherwise, if
    // IDS_TASK_MANAGER_TAB_PREFIX is an RTL word, the concatenated result
    // might be wrong. For example, http://mail.yahoo.com, whose title is
    // "Yahoo! Mail: The best web-based Email!", without setting it explicitly
    // as LTR format, the concatenated result will be "!Yahoo! Mail: The best
    // web-based Email :BAT", in which the capital letters "BAT" stands for
    // the Hebrew word for "tab".
    l10n_util::AdjustStringForLocaleDirection(tab_title, &tab_title);
  }

  return l10n_util::GetStringF(IDS_TASK_MANAGER_TAB_PREFIX, tab_title);
}

SkBitmap TaskManagerWebContentsResource::GetIcon() const {
  return web_contents_->GetFavIcon();
}

HANDLE TaskManagerWebContentsResource::GetProcess() const {
  return process_;
}

TabContents* TaskManagerWebContentsResource::GetTabContents() const {
  return dynamic_cast<TabContents*>(web_contents_);
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerWebContentsResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerWebContentsResourceProvider::
    TaskManagerWebContentsResourceProvider(TaskManager* task_manager)
    : task_manager_(task_manager),
      updating_(false) {
}

TaskManagerWebContentsResourceProvider::
    ~TaskManagerWebContentsResourceProvider() {
}

TaskManager::Resource* TaskManagerWebContentsResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {

  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  if (!web_contents)  // Not one of our resource.
    return NULL;

  if (!web_contents->process()->process().handle()) {
    // We should not be holding on to a dead tab (it should have been removed
    // through the NOTIFY_WEB_CONTENTS_DISCONNECTED notification.
    NOTREACHED();
    return NULL;
  }

  int pid = web_contents->process()->process().pid();
  if (pid != origin_pid)
    return NULL;

  std::map<WebContents*, TaskManagerWebContentsResource*>::iterator
      res_iter = resources_.find(web_contents);
  if (res_iter == resources_.end())
    // Can happen if the tab was closed while a network request was being
    // performed.
    return NULL;

  return res_iter->second;
}

void TaskManagerWebContentsResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;
  // Add all the existing WebContents.
  for (WebContentsIterator iterator; !iterator.done(); iterator++) {
    WebContents* web_contents = *iterator;
    // Don't add dead tabs or tabs that haven't yet connected.
    if (web_contents->process()->process().handle() &&
        web_contents->notify_disconnection())
      AddToTaskManager(web_contents);
  }
  // Then we register for notifications to get new tabs.
  NotificationService* service = NotificationService::current();
  service->AddObserver(this, NOTIFY_WEB_CONTENTS_CONNECTED,
                       NotificationService::AllSources());
  service->AddObserver(this, NOTIFY_WEB_CONTENTS_SWAPPED,
                       NotificationService::AllSources());
  service->AddObserver(this, NOTIFY_WEB_CONTENTS_DISCONNECTED,
                       NotificationService::AllSources());
}

void TaskManagerWebContentsResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Then we unregister for notifications to get new tabs.
  NotificationService* service = NotificationService::current();
  service->RemoveObserver(this, NOTIFY_WEB_CONTENTS_CONNECTED,
                          NotificationService::AllSources());
  service->RemoveObserver(this, NOTIFY_WEB_CONTENTS_SWAPPED,
                          NotificationService::AllSources());
  service->RemoveObserver(this, NOTIFY_WEB_CONTENTS_DISCONNECTED,
                          NotificationService::AllSources());

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());

  resources_.clear();
}

void TaskManagerWebContentsResourceProvider::AddToTaskManager(
    WebContents* web_contents) {
  TaskManagerWebContentsResource* resource =
      new TaskManagerWebContentsResource(web_contents);
  resources_[web_contents] = resource;
  task_manager_->AddResource(resource);
}

void TaskManagerWebContentsResourceProvider::Add(WebContents* web_contents) {
  if (!updating_)
    return;

  if (!web_contents->process()->process().handle()) {
    // Don't add sad tabs, we would have no information to show for them since
    // they have no associated process.
    return;
  }

  std::map<WebContents*, TaskManagerWebContentsResource*>::const_iterator
      iter = resources_.find(web_contents);
  if (iter != resources_.end()) {
    // The case may happen that we have added a WebContents as part of the
    // iteration performed during StartUpdating() call but the notification that
    // it has connected was not fired yet. So when the notification happens, we
    // already know about this tab and just ignore it.
    return;
  }
  AddToTaskManager(web_contents);
}

void TaskManagerWebContentsResourceProvider::Remove(WebContents* web_contents) {
  if (!updating_)
    return;
  std::map<WebContents*, TaskManagerWebContentsResource*>::iterator
      iter = resources_.find(web_contents);
  if (iter == resources_.end()) {
    // Since TabContents are destroyed asynchronously (see TabContentsCollector
    // in navigation_controller.cc), we can be notified of a tab being removed
    // that we don't know.  This can happen if the user closes a tab and quickly
    // opens the task manager, before the tab is actually destroyed.
    return;
  }

  // Remove the resource from the Task Manager.
  TaskManagerWebContentsResource* resource = iter->second;
  task_manager_->RemoveResource(resource);
  // And from the provider.
  resources_.erase(iter);
  // Finally, delete the resource.
  delete resource;
}

void TaskManagerWebContentsResourceProvider::Observe(NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case NOTIFY_WEB_CONTENTS_CONNECTED:
      Add(Source<WebContents>(source).ptr());
      break;
    case NOTIFY_WEB_CONTENTS_SWAPPED:
      Remove(Source<WebContents>(source).ptr());
      Add(Source<WebContents>(source).ptr());
      break;
    case NOTIFY_WEB_CONTENTS_DISCONNECTED:
      Remove(Source<WebContents>(source).ptr());
      break;
    default:
      NOTREACHED() << "Unexpected notification.";
      return;
  }
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerPluginProcessResource class
////////////////////////////////////////////////////////////////////////////////
SkBitmap* TaskManagerPluginProcessResource::default_icon_ = NULL;

TaskManagerPluginProcessResource::TaskManagerPluginProcessResource(
    PluginProcessInfo plugin_proc)
    : plugin_process_(plugin_proc),
      title_(),
      network_usage_support_(false) {
  pid_ = base::GetProcId(plugin_proc.process());
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetBitmapNamed(IDR_PLUGIN);
  }
}

TaskManagerPluginProcessResource::~TaskManagerPluginProcessResource() {
}

// TaskManagerResource methods:
std::wstring TaskManagerPluginProcessResource::GetTitle() const {
  if (title_.empty()) {
    std::wstring plugin_name;
    WebPluginInfo info;
    if (PluginService::GetInstance()->
            GetPluginInfoByPath(plugin_process_.plugin_path(), &info))
      plugin_name = info.name;
    else
      plugin_name = l10n_util::GetString(IDS_TASK_MANAGER_UNKNOWN_PLUGIN_NAME);
    // Explicitly mark plugin_name as LTR if there is no strong RTL character,
    // to avoid the wrong concatenation result similar to "!Yahoo! Mail: the
    // best web-based Email: NIGULP", in which "NIGULP" stands for the Hebrew
    // or Arabic word for "plugin".
    l10n_util::AdjustStringForLocaleDirection(plugin_name, &plugin_name);
    title_ = l10n_util::GetStringF(IDS_TASK_MANAGER_PLUGIN_PREFIX,
                                   plugin_name);
  }
  return title_;
}

SkBitmap TaskManagerPluginProcessResource::GetIcon() const {
  return *default_icon_;
}

HANDLE TaskManagerPluginProcessResource::GetProcess() const {
  return plugin_process_.process();
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerPluginProcessResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerPluginProcessResourceProvider::
    TaskManagerPluginProcessResourceProvider(TaskManager* task_manager)
    : task_manager_(task_manager),
      updating_(false),
      ui_loop_(MessageLoop::current()) {
}

TaskManagerPluginProcessResourceProvider::
    ~TaskManagerPluginProcessResourceProvider() {
}

TaskManager::Resource* TaskManagerPluginProcessResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  std::map<int, TaskManagerPluginProcessResource*>::iterator iter =
      pid_to_resources_.find(origin_pid);
  if (iter != pid_to_resources_.end())
    return iter->second;
  else
    return NULL;
}

void TaskManagerPluginProcessResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // Register for notifications to get new plugin processes.
  NotificationService* service = NotificationService::current();
  service->AddObserver(this, NOTIFY_PLUGIN_PROCESS_HOST_CONNECTED,
                       NotificationService::AllSources());
  service->AddObserver(this, NOTIFY_PLUGIN_PROCESS_HOST_DISCONNECTED,
                       NotificationService::AllSources());

  // Get the existing plugins
  MessageLoop* io_loop_ = g_browser_process->io_thread()->message_loop();
  io_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &TaskManagerPluginProcessResourceProvider::RetrievePluginProcessInfo));
}

void TaskManagerPluginProcessResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Unregister for notifications to get new plugin processes.
  NotificationService* service = NotificationService::current();
  service->RemoveObserver(this, NOTIFY_PLUGIN_PROCESS_HOST_CONNECTED,
                          NotificationService::AllSources());
  service->RemoveObserver(this, NOTIFY_PLUGIN_PROCESS_HOST_DISCONNECTED,
                          NotificationService::AllSources());

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());

  resources_.clear();
  pid_to_resources_.clear();
  existing_plugin_process_info.clear();
}

void TaskManagerPluginProcessResourceProvider::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case NOTIFY_PLUGIN_PROCESS_HOST_CONNECTED:
      Add(*Details<PluginProcessInfo>(details).ptr());
      break;
    case NOTIFY_PLUGIN_PROCESS_HOST_DISCONNECTED:
      Remove(*Details<PluginProcessInfo>(details).ptr());
      break;
    default:
      NOTREACHED() << "Unexpected notification.";
      return;
  }
}

void TaskManagerPluginProcessResourceProvider::Add(
    PluginProcessInfo plugin_process_info) {
  if (!updating_)
    return;
  std::map<PluginProcessInfo, TaskManagerPluginProcessResource*>::
      const_iterator iter = resources_.find(plugin_process_info);
  if (iter != resources_.end()) {
    // The case may happen that we have added a plugin_process_host as part of
    // the iteration performed during StartUpdating() call but the notification
    // that it has connected was not fired yet. So when the notification
    // happens, we already know about this plugin and just ignore it.
    return;
  }
  AddToTaskManager(plugin_process_info);
}

void TaskManagerPluginProcessResourceProvider::Remove(
    PluginProcessInfo plugin_process_info) {
  if (!updating_)
    return;
  std::map<PluginProcessInfo, TaskManagerPluginProcessResource*>
      ::iterator iter = resources_.find(plugin_process_info);
  if (iter == resources_.end()) {
    // PluginProcessHost disconnection notifications are asynchronous, so we
    // might be notified for a plugin we don't know anything about (if it was
    // closed before the task manager was shown and destroyed after that).
    return;
  }
  // Remove the resource from the Task Manager.
  TaskManagerPluginProcessResource* resource = iter->second;
  task_manager_->RemoveResource(resource);
  // Remove it from the provider.
  resources_.erase(iter);
  // Remove it from our pid map.
  std::map<int, TaskManagerPluginProcessResource*>::iterator pid_iter =
      pid_to_resources_.find(resource->process_id());
  DCHECK(pid_iter != pid_to_resources_.end());
  if (pid_iter != pid_to_resources_.end())
    pid_to_resources_.erase(pid_iter);

  // Finally, delete the resource.
  delete resource;
}

void TaskManagerPluginProcessResourceProvider::AddToTaskManager(
    PluginProcessInfo plugin_process_info) {
  TaskManagerPluginProcessResource* resource =
      new TaskManagerPluginProcessResource(plugin_process_info);
  resources_[plugin_process_info] = resource;
  pid_to_resources_[base::GetProcId(plugin_process_info.process())] =
      resource;
  task_manager_->AddResource(resource);
}

// The PluginProcessIterator has to be used from the IO thread.
void TaskManagerPluginProcessResourceProvider::RetrievePluginProcessInfo() {
  for (PluginProcessHostIterator iter; !iter.Done(); ++iter) {
    PluginProcessHost* plugin = const_cast<PluginProcessHost*>(*iter);
    DCHECK(plugin->process());
    PluginProcessInfo plugin_info(plugin->plugin_path(), plugin->process());
    existing_plugin_process_info.push_back(plugin_info);
  }
  // Now notify the UI thread that we have retrieved the PluginProcessHosts.
  ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &TaskManagerPluginProcessResourceProvider::PluginProcessInfoRetreived));
}

// This is called on the UI thread.
void TaskManagerPluginProcessResourceProvider::PluginProcessInfoRetreived() {
  std::vector<PluginProcessInfo>::const_iterator iter;
  for (iter = existing_plugin_process_info.begin();
       iter != existing_plugin_process_info.end(); ++iter) {
    Add(*iter);
  }
  existing_plugin_process_info.clear();
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerBrowserProcessResource class
////////////////////////////////////////////////////////////////////////////////

SkBitmap* TaskManagerBrowserProcessResource::default_icon_ = NULL;

TaskManagerBrowserProcessResource::TaskManagerBrowserProcessResource()
:   title_(),
     network_usage_support_(false) {
  pid_ = GetCurrentProcessId();
  process_ = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                         FALSE,
                         pid_);
  DCHECK(process_);
  if (!default_icon_) {
    HICON icon = LoadIcon(_AtlBaseModule.GetResourceInstance(),
                          MAKEINTRESOURCE(IDR_MAINFRAME));
    if (icon) {
      ICONINFO icon_info = {0};
      BITMAP bitmap_info = {0};

      GetIconInfo(icon, &icon_info);
      GetObject(icon_info.hbmMask, sizeof(bitmap_info), &bitmap_info);

      gfx::Size icon_size(bitmap_info.bmWidth, bitmap_info.bmHeight);
      default_icon_ = IconUtil::CreateSkBitmapFromHICON(icon, icon_size);
    }
  }
}

TaskManagerBrowserProcessResource::~TaskManagerBrowserProcessResource() {
  CloseHandle(process_);
}

// TaskManagerResource methods:
std::wstring TaskManagerBrowserProcessResource::GetTitle() const {
  if (title_.empty()) {
    title_ = l10n_util::GetString(IDS_TASK_MANAGER_WEB_BROWSER_CELL_TEXT);
  }
  return title_;
}

SkBitmap TaskManagerBrowserProcessResource::GetIcon() const {
  return *default_icon_;
}

HANDLE TaskManagerBrowserProcessResource::GetProcess() const {
  return GetCurrentProcess();  // process_;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerBrowserProcessResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerBrowserProcessResourceProvider::
    TaskManagerBrowserProcessResourceProvider(TaskManager* task_manager)
    : task_manager_(task_manager) {
}

TaskManagerBrowserProcessResourceProvider::
    ~TaskManagerBrowserProcessResourceProvider() {
}

TaskManager::Resource* TaskManagerBrowserProcessResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  if (origin_pid != resource_.process_id()) {
    return NULL;
  }

  return &resource_;
}

void TaskManagerBrowserProcessResourceProvider::StartUpdating() {
  task_manager_->AddResource(&resource_);
}

void TaskManagerBrowserProcessResourceProvider::StopUpdating() {
}


