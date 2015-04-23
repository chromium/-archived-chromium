// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"
#include <psapi.h>

#include "base/file_version_info.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/url_constants.h"

class RenderViewHostDelegate;

// Template of static data we use for finding browser process information.
// These entries must match the ordering for MemoryDetails::BrowserProcess.
static ProcessData
    g_process_template[MemoryDetails::MAX_BROWSERS] = {
    { L"Chromium", L"chrome.exe", },
    { L"IE", L"iexplore.exe", },
    { L"Firefox", L"firefox.exe", },
    { L"Opera", L"opera.exe", },
    { L"Safari", L"safari.exe", },
    { L"IE (64bit)", L"iexplore.exe", },
    { L"Konqueror", L"konqueror.exe", },
  };


// About threading:
//
// This operation will hit no fewer than 3 threads.
//
// The ChildProcessInfo::Iterator can only be accessed from the IO thread.
//
// The RenderProcessHostIterator can only be accessed from the UI thread.
//
// This operation can take 30-100ms to complete.  We never want to have
// one task run for that long on the UI or IO threads.  So, we run the
// expensive parts of this operation over on the file thread.
//

MemoryDetails::MemoryDetails()
  : ui_loop_(NULL) {
  for (int index = 0; index < arraysize(g_process_template); ++index) {
    process_data_[index].name = g_process_template[index].name;
    process_data_[index].process_name = g_process_template[index].process_name;
  }
}

void MemoryDetails::StartFetch() {
  ui_loop_ = MessageLoop::current();

  DCHECK(ui_loop_ != g_browser_process->io_thread()->message_loop());
  DCHECK(ui_loop_ != g_browser_process->file_thread()->message_loop());

  // In order to process this request, we need to use the plugin information.
  // However, plugin process information is only available from the IO thread.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &MemoryDetails::CollectChildInfoOnIOThread));
}

void MemoryDetails::CollectChildInfoOnIOThread() {
  DCHECK(MessageLoop::current() ==
      ChromeThread::GetMessageLoop(ChromeThread::IO));

  std::vector<ProcessMemoryInformation> child_info;

  // Collect the list of child processes.
  for (ChildProcessHost::Iterator iter; !iter.Done(); ++iter) {
    ProcessMemoryInformation info;
    info.pid = iter->GetProcessId();
    if (!info.pid)
      continue;

    info.type = iter->type();
    info.titles.push_back(iter->name());
    child_info.push_back(info);
  }

  // Now go do expensive memory lookups from the file thread.
  ChromeThread::GetMessageLoop(ChromeThread::FILE)->PostTask(FROM_HERE,
      NewRunnableMethod(this, &MemoryDetails::CollectProcessData, child_info));
}

void MemoryDetails::CollectProcessData(
    std::vector<ProcessMemoryInformation> child_info) {
  DCHECK(MessageLoop::current() ==
      ChromeThread::GetMessageLoop(ChromeThread::FILE));

  // Clear old data.
  for (int index = 0; index < arraysize(g_process_template); index++)
    process_data_[index].processes.clear();

  SYSTEM_INFO system_info;
  GetNativeSystemInfo(&system_info);
  bool is_64bit_os =
      system_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;

  ScopedHandle snapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  PROCESSENTRY32 process_entry = {sizeof(PROCESSENTRY32)};
  if (!snapshot.Get()) {
    LOG(ERROR) << "CreateToolhelp32Snaphot failed: " << GetLastError();
    return;
  }
  if (!::Process32First(snapshot, &process_entry)) {
    LOG(ERROR) << "Process32First failed: " << GetLastError();
    return;
  }
  do {
    int pid = process_entry.th32ProcessID;
    ScopedHandle handle(::OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));
    if (!handle.Get())
      continue;
    bool is_64bit_process = false;
    // IsWow64Process() returns FALSE for a 32bit process on a 32bit OS.
    // We need to check if the real OS is 64bit.
    if (is_64bit_os) {
      BOOL is_wow64 = FALSE;
      // IsWow64Process() is supported by Windows XP SP2 or later.
      IsWow64Process(handle, &is_wow64);
      is_64bit_process = !is_wow64;
    }
    for (int index2 = 0; index2 < arraysize(g_process_template); index2++) {
      if (_wcsicmp(process_data_[index2].process_name,
          process_entry.szExeFile) != 0)
        continue;
      if (index2 == IE_BROWSER && is_64bit_process)
        continue;  // Should use IE_64BIT_BROWSER
      // Get Memory Information.
      ProcessMemoryInformation info;
      info.pid = pid;
      if (info.pid == GetCurrentProcessId())
        info.type = ChildProcessInfo::BROWSER_PROCESS;
      else
        info.type = ChildProcessInfo::UNKNOWN_PROCESS;

      scoped_ptr<base::ProcessMetrics> metrics;
      metrics.reset(base::ProcessMetrics::CreateProcessMetrics(handle));
      metrics->GetCommittedKBytes(&info.committed);
      metrics->GetWorkingSetKBytes(&info.working_set);

      // Get Version Information.
      TCHAR name[MAX_PATH];
      if (index2 == CHROME_BROWSER) {
        scoped_ptr<FileVersionInfo> version_info(
            FileVersionInfo::CreateFileVersionInfoForCurrentModule());
        if (version_info != NULL)
          info.version = version_info->file_version();
        // Check if this is one of the child processes whose data we collected
        // on the IO thread, and if so copy over that data.
        for (size_t child = 0; child < child_info.size(); child++) {
          if (child_info[child].pid != info.pid)
            continue;
          info.titles = child_info[child].titles;
          info.type = child_info[child].type;
          break;
        }
      } else if (GetModuleFileNameEx(handle, NULL, name, MAX_PATH - 1)) {
        std::wstring str_name(name);
        scoped_ptr<FileVersionInfo> version_info(
            FileVersionInfo::CreateFileVersionInfo(str_name));
        if (version_info != NULL) {
          info.version = version_info->product_version();
          info.product_name = version_info->product_name();
        }
      }

      // Add the process info to our list.
      process_data_[index2].processes.push_back(info);
      break;
    }
  } while (::Process32Next(snapshot, &process_entry));

  // Finally return to the browser thread.
  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &MemoryDetails::CollectChildInfoOnUIThread));
}

void MemoryDetails::CollectChildInfoOnUIThread() {
  DCHECK(MessageLoop::current() == ui_loop_);

  // Get more information about the process.
  for (size_t index = 0; index < process_data_[CHROME_BROWSER].processes.size();
      index++) {
    // Check if it's a renderer, if so get the list of page titles in it and
    // check if it's a diagnostics-related process.  We skip all diagnostics
    // pages (e.g. "about:xxx" URLs).  Iterate the RenderProcessHosts to find
    // the tab contents.
    RenderProcessHost::iterator renderer_iter;
    for (renderer_iter = RenderProcessHost::begin(); renderer_iter !=
         RenderProcessHost::end(); ++renderer_iter) {
      DCHECK(renderer_iter->second);
      ProcessMemoryInformation& process =
          process_data_[CHROME_BROWSER].processes[index];
      if (process.pid != renderer_iter->second->process().pid())
        continue;
      process.type = ChildProcessInfo::RENDER_PROCESS;
      // The RenderProcessHost may host multiple TabContents.  Any
      // of them which contain diagnostics information make the whole
      // process be considered a diagnostics process.
      //
      // NOTE: This is a bit dangerous.  We know that for now, listeners
      //       are always RenderWidgetHosts.  But in theory, they don't
      //       have to be.
      RenderProcessHost::listeners_iterator iter;
      for (iter = renderer_iter->second->listeners_begin();
           iter != renderer_iter->second->listeners_end(); ++iter) {
        RenderWidgetHost* widget =
            static_cast<RenderWidgetHost*>(iter->second);
        DCHECK(widget);
        if (!widget || !widget->IsRenderView())
          continue;

        RenderViewHost* host = static_cast<RenderViewHost*>(widget);
        TabContents* contents = NULL;
        if (host->delegate())
          contents = host->delegate()->GetAsTabContents();
        if (!contents)
          continue;
        std::wstring title = UTF16ToWideHack(contents->GetTitle());
        if (!title.length())
          title = L"Untitled";
        process.titles.push_back(title);

        // We need to check the pending entry as well as the display_url to
        // see if it's an about:memory URL (we don't want to count these in the
        // total memory usage of the browser).
        //
        // When we reach here, about:memory will be the pending entry since we
        // haven't responded with any data such that it would be committed. If
        // you have another about:memory tab open (which would be committed),
        // we don't want to count it either, so we also check the last committed
        // entry.
        //
        // Either the pending or last committed entries can be NULL.
        const NavigationEntry* pending_entry =
            contents->controller().pending_entry();
        const NavigationEntry* last_committed_entry =
            contents->controller().GetLastCommittedEntry();
        if ((last_committed_entry &&
             LowerCaseEqualsASCII(last_committed_entry->display_url().spec(),
                                  chrome::kAboutMemoryURL)) ||
            (pending_entry &&
             LowerCaseEqualsASCII(pending_entry->display_url().spec(),
                                  chrome::kAboutMemoryURL)))
          process.is_diagnostics = true;
      }
    }
  }

  // Get rid of other Chrome processes that are from a different profile.
  for (size_t index = 0; index < process_data_[CHROME_BROWSER].processes.size();
      index++) {
    if (process_data_[CHROME_BROWSER].processes[index].type ==
        ChildProcessInfo::UNKNOWN_PROCESS) {
      process_data_[CHROME_BROWSER].processes.erase(
          process_data_[CHROME_BROWSER].processes.begin() + index);
      index--;
    }
  }

  UpdateHistograms();

  OnDetailsAvailable();
}

void MemoryDetails::UpdateHistograms() {
  // Reports a set of memory metrics to UMA.
  // Memory is measured in units of 10KB.

  ProcessData browser = process_data_[CHROME_BROWSER];
  size_t aggregate_memory = 0;
  int plugin_count = 0;
  int worker_count = 0;
  for (size_t index = 0; index < browser.processes.size(); index++) {
    int sample = static_cast<int>(browser.processes[index].working_set.priv);
    aggregate_memory += sample;
    switch (browser.processes[index].type) {
     case ChildProcessInfo::BROWSER_PROCESS:
       UMA_HISTOGRAM_MEMORY_KB("Memory.Browser", sample);
       break;
     case ChildProcessInfo::RENDER_PROCESS:
       UMA_HISTOGRAM_MEMORY_KB("Memory.Renderer", sample);
       break;
     case ChildProcessInfo::PLUGIN_PROCESS:
       UMA_HISTOGRAM_MEMORY_KB("Memory.Plugin", sample);
       plugin_count++;
       break;
     case ChildProcessInfo::WORKER_PROCESS:
       UMA_HISTOGRAM_MEMORY_KB("Memory.Worker", sample);
       worker_count++;
       break;
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Memory.ProcessCount",
      static_cast<int>(browser.processes.size()));
  UMA_HISTOGRAM_COUNTS_100("Memory.PluginProcessCount", plugin_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.WorkerProcessCount", worker_count);

  int total_sample = static_cast<int>(aggregate_memory / 1000);
  UMA_HISTOGRAM_MEMORY_MB("Memory.Total", total_sample);
}
