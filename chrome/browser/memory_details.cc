// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"
#include <psapi.h>

#include "base/file_version_info.h"
#include "base/histogram.h"
#include "base/image_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_trial.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/web_contents.h"

class RenderViewHostDelegate;

// Template of static data we use for finding browser process information.
// These entries must match the ordering for MemoryDetails::BrowserProcess.
static ProcessData g_process_template[] = {
    { L"Chromium", L"chrome.exe", },
    { L"IE", L"iexplore.exe", },
    { L"Firefox", L"firefox.exe", },
    { L"Opera", L"opera.exe", },
    { L"Safari", L"safari.exe", },
  };


// About threading:
//
// This operation will hit no fewer than 3 threads.
//
// The PluginHostIterator can only be accessed from the IO thread.
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
      NewRunnableMethod(this, &MemoryDetails::CollectPluginInformation));
}

void MemoryDetails::CollectPluginInformation() {
  DCHECK(MessageLoop::current() ==
      ChromeThread::GetMessageLoop(ChromeThread::IO));

  // Collect the list of plugins.
  for (PluginProcessHostIterator plugin_iter;
       !plugin_iter.Done(); ++plugin_iter) {
    PluginProcessHost* plugin = const_cast<PluginProcessHost*>(*plugin_iter);
    DCHECK(plugin);
    if (!plugin || !plugin->process())
      continue;

    PluginProcessInformation info;
    info.pid = base::GetProcId(plugin->process());
    if (info.pid != 0) {
      info.plugin_path = plugin->plugin_path();
      plugins_.push_back(info);
    }
  }

  // Now go do expensive memory lookups from the file thread.
  ChromeThread::GetMessageLoop(ChromeThread::FILE)->PostTask(FROM_HERE,
      NewRunnableMethod(this, &MemoryDetails::CollectProcessData));
}

void MemoryDetails::CollectProcessData() {
  DCHECK(MessageLoop::current() ==
      ChromeThread::GetMessageLoop(ChromeThread::FILE));

  int array_size = 32;
  DWORD* process_list = NULL;
  DWORD bytes_used = 0;
  do {
    array_size *= 2;
    process_list = static_cast<DWORD*>(
        realloc(process_list, sizeof(*process_list) * array_size));
    // EnumProcesses doesn't return an error if the array is too small.
    // We have to check if the return buffer is full, and if so, call it
    // again.  See msdn docs for more info.
    if (!EnumProcesses(process_list, array_size * sizeof(*process_list),
                       &bytes_used)) {
      LOG(ERROR) << "EnumProcesses failed: " << GetLastError();
      return;
    }
  } while (bytes_used == (array_size * sizeof(*process_list)));

  int num_processes = bytes_used / sizeof(*process_list);

  // Clear old data.
  for (int index = 0; index < arraysize(g_process_template); index++)
    process_data_[index].processes.clear();

  for (int index = 0; index < num_processes; index++) {
    HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                FALSE, process_list[index]);
    if (handle) {
      TCHAR name[MAX_PATH];
      if (GetModuleBaseName(handle, NULL, name, MAX_PATH-1)) {
        for (int index2 = 0; index2 < arraysize(g_process_template); index2++) {
          if (_wcsicmp(process_data_[index2].process_name, name) == 0) {
            // Get Memory Information.
            ProcessMemoryInformation info;
            info.pid = process_list[index];
            scoped_ptr<base::ProcessMetrics> metrics;
            metrics.reset(base::ProcessMetrics::CreateProcessMetrics(handle));
            metrics->GetCommittedKBytes(&info.committed);
            metrics->GetWorkingSetKBytes(&info.working_set);

            // Get Version Information.
            if (index2 == 0) {  // Chrome
              scoped_ptr<FileVersionInfo> version_info(
                 FileVersionInfo::CreateFileVersionInfoForCurrentModule());
              if (version_info != NULL)
                info.version = version_info->file_version();
            } else if (GetModuleFileNameEx(handle, NULL, name, MAX_PATH-1)) {
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
        }
      }
      CloseHandle(handle);
    }
  }
  free(process_list);

  // Finally return to the browser thread.
  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &MemoryDetails::CollectRenderHostInformation));
}

void MemoryDetails::CollectRenderHostInformation() {
  DCHECK(MessageLoop::current() == ui_loop_);

  // Determine if this is a diagnostics-related process.  We skip all
  // diagnostics pages (e.g. "about:xxx" URLs).  Iterate the RenderProcessHosts
  // to find the tab contents.  If it is of type TAB_CONTENTS_ABOUT_UI, mark
  // the process as diagnostics related.
  for (size_t index = 0; index < process_data_[CHROME_BROWSER].processes.size();
      index++) {
    RenderProcessHost::iterator renderer_iter;
    for (renderer_iter = RenderProcessHost::begin(); renderer_iter !=
         RenderProcessHost::end(); ++renderer_iter) {
      DCHECK(renderer_iter->second);
      if (process_data_[CHROME_BROWSER].processes[index].pid ==
          renderer_iter->second->process().pid()) {
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
          TabContents* contents =
              static_cast<WebContents*>(host->delegate());
          DCHECK(contents);
          if (!contents)
            continue;

          if (contents->type() == TAB_CONTENTS_ABOUT_UI)
            process_data_[CHROME_BROWSER].processes[index].is_diagnostics =
                true;
        }
      }
    }
  }

  UpdateHistograms();

  OnDetailsAvailable();
}

void MemoryDetails::UpdateHistograms() {
  // Reports a set of memory metrics to UMA.
  // Memory is measured in units of 10KB.

  // If field trial is active, report results in special histograms.
  static scoped_refptr<FieldTrial> trial(
      FieldTrialList::Find(BrowserTrial::kMemoryModelFieldTrial));

  DWORD browser_pid = GetCurrentProcessId();
  ProcessData browser = process_data_[CHROME_BROWSER];
  size_t aggregate_memory = 0;
  for (size_t index = 0; index < browser.processes.size(); index++) {
    int sample = static_cast<int>(browser.processes[index].working_set.priv);
    aggregate_memory += sample;
    if (browser.processes[index].pid == browser_pid) {
      if (trial.get()) {
        if (trial->boolean_value())
          UMA_HISTOGRAM_MEMORY_KB(L"Memory.Browser_trial_high_memory", sample);
        else
          UMA_HISTOGRAM_MEMORY_KB(L"Memory.Browser_trial_med_memory", sample);
      } else {
        UMA_HISTOGRAM_MEMORY_KB(L"Memory.Browser", sample);
      }
    } else {
      bool is_plugin_process = false;
      for (size_t index2 = 0; index2 < plugins_.size(); index2++) {
        if (browser.processes[index].pid == plugins_[index2].pid) {
          UMA_HISTOGRAM_MEMORY_KB(L"Memory.Plugin", sample);
          is_plugin_process = true;
          break;
        }
      }
      if (!is_plugin_process) {
        if (trial.get()) {
          if (trial->boolean_value())
            UMA_HISTOGRAM_MEMORY_KB(L"Memory.Renderer_trial_high_memory",
                                    sample);
          else
            UMA_HISTOGRAM_MEMORY_KB(L"Memory.Renderer_trial_med_memory",
                                    sample);
        } else {
          UMA_HISTOGRAM_MEMORY_KB(L"Memory.Renderer", sample);
        }
      }
    }
  }
  UMA_HISTOGRAM_COUNTS_100(L"Memory.ProcessCount",
      static_cast<int>(browser.processes.size()));
  UMA_HISTOGRAM_COUNTS_100(L"Memory.PluginProcessCount",
      static_cast<int>(plugins_.size()));

  int total_sample = static_cast<int>(aggregate_memory / 1000);
  if (trial.get()) {
    if (trial->boolean_value())
      UMA_HISTOGRAM_MEMORY_MB(L"Memory.Total_trial_high_memory", total_sample);
    else
      UMA_HISTOGRAM_MEMORY_MB(L"Memory.Total_trial_med_memory", total_sample);
  } else {
    UMA_HISTOGRAM_MEMORY_MB(L"Memory.Total", total_sample);
  }
}

