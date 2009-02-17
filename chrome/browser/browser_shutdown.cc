// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_shutdown.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/jankometer.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/resource_bundle.h"
#include "net/dns_global.h"

// TODO(port): Get rid of this section and finish porting.
#if defined(OS_WIN)
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/plugin_service.h"
#else
#include "chrome/common/temp_scaffolding_stubs.h"
#endif


using base::Time;
using base::TimeDelta;

namespace browser_shutdown {

Time shutdown_started_;
ShutdownType shutdown_type_ = NOT_VALID;
int shutdown_num_processes_;
int shutdown_num_processes_slow_;

bool delete_resources_on_shutdown = true;

const char* const kShutdownMsFile = "chrome_shutdown_ms.txt";

void RegisterPrefs(PrefService* local_state) {
  local_state->RegisterIntegerPref(prefs::kShutdownType, NOT_VALID);
  local_state->RegisterIntegerPref(prefs::kShutdownNumProcesses, 0);
  local_state->RegisterIntegerPref(prefs::kShutdownNumProcessesSlow, 0);
}

void OnShutdownStarting(ShutdownType type) {
  // TODO(erikkay): http://b/753080 when onbeforeunload is supported at
  // shutdown, fix this to allow these variables to be reset.
  if (shutdown_type_ == NOT_VALID) {
    shutdown_type_ = type;
    // For now, we're only counting the number of renderer processes
    // since we can't safely count the number of plugin processes from this
    // thread, and we'd really like to avoid anything which might add further
    // delays to shutdown time.
    shutdown_num_processes_ = static_cast<int>(RenderProcessHost::size());
    shutdown_started_ = Time::Now();

    // Call FastShutdown on all of the RenderProcessHosts.  This will be
    // a no-op in some cases, so we still need to go through the normal
    // shutdown path for the ones that didn't exit here.
    shutdown_num_processes_slow_ = 0;
    for (RenderProcessHost::iterator hosts = RenderProcessHost::begin();
         hosts != RenderProcessHost::end();
         ++hosts) {
      RenderProcessHost* rph = hosts->second;
      if (!rph->FastShutdownIfPossible())
        // TODO(ojan): I think now that we deal with beforeunload/unload
        // higher up, it's not possible to get here. Confirm this and change
        // FastShutdownIfPossible to just be FastShutdown.
        shutdown_num_processes_slow_++;
    }
  }
}

FilePath GetShutdownMsPath() {
  FilePath shutdown_ms_file;
  PathService::Get(base::DIR_TEMP, &shutdown_ms_file);
  return shutdown_ms_file.AppendASCII(kShutdownMsFile);
}

void Shutdown() {
  // Unload plugins. This needs to happen on the IO thread.
  if (g_browser_process->io_thread()) {
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
        NewRunnableFunction(&ChromePluginLib::UnloadAllPlugins));
  }

  // WARNING: During logoff/shutdown (WM_ENDSESSION) we may not have enough
  // time to get here. If you have something that *must* happen on end session,
  // consider putting it in BrowserProcessImpl::EndSession.
  DCHECK(g_browser_process);

  // Notifies we are going away.
  g_browser_process->shutdown_event()->Signal();

  PluginService* plugin_service = PluginService::GetInstance();
  if (plugin_service) {
    plugin_service->Shutdown();
  }

  PrefService* prefs = g_browser_process->local_state();

  chrome_browser_net::SaveHostNamesForNextStartup(prefs);
  // TODO(jar): Trimming should be done more regularly, such as every 48 hours
  // of physical time, or perhaps after 48 hours of running (excluding time
  // between sessions possibly).
  // For now, we'll just trim at shutdown.
  chrome_browser_net::TrimSubresourceReferrers();
  chrome_browser_net::SaveSubresourceReferrers(prefs);

  MetricsService* metrics = g_browser_process->metrics_service();
  if (metrics) {
    metrics->RecordCleanShutdown();
    metrics->RecordCompletedSessionEnd();
  }

  if (shutdown_type_ > NOT_VALID && shutdown_num_processes_ > 0) {
    // Record the shutdown info so that we can put it into a histogram at next
    // startup.
    prefs->SetInteger(prefs::kShutdownType, shutdown_type_);
    prefs->SetInteger(prefs::kShutdownNumProcesses, shutdown_num_processes_);
    prefs->SetInteger(prefs::kShutdownNumProcessesSlow,
                      shutdown_num_processes_slow_);
  }

  prefs->SavePersistentPrefs(g_browser_process->file_thread());

  // Cleanup any statics created by RLZ. Must be done before NotificationService
  // is destroyed.
  RLZTracker::CleanupRlz();

  // The jank'o'meter requires that the browser process has been destroyed
  // before calling UninstallJankometer().
  delete g_browser_process;
  g_browser_process = NULL;

  // Uninstall Jank-O-Meter here after the IO thread is no longer running.
  UninstallJankometer();

  if (delete_resources_on_shutdown)
    ResourceBundle::CleanupSharedInstance();

  if (!Upgrade::IsBrowserAlreadyRunning()) {
    Upgrade::SwapNewChromeExeIfPresent();
  }

  if (shutdown_type_ > NOT_VALID && shutdown_num_processes_ > 0) {
    // Measure total shutdown time as late in the process as possible
    // and then write it to a file to be read at startup.
    // We can't use prefs since all services are shutdown at this point.
    TimeDelta shutdown_delta = Time::Now() - shutdown_started_;
    std::string shutdown_ms = Int64ToString(shutdown_delta.InMilliseconds());
    int len = static_cast<int>(shutdown_ms.length()) + 1;
    FilePath shutdown_ms_file = GetShutdownMsPath();
    file_util::WriteFile(shutdown_ms_file, shutdown_ms.c_str(), len);
  }

  UnregisterURLRequestChromeJob();
}

void ReadLastShutdownInfo() {
  FilePath shutdown_ms_file = GetShutdownMsPath();
  std::string shutdown_ms_str;
  int64 shutdown_ms = 0;
  if (file_util::ReadFileToString(shutdown_ms_file, &shutdown_ms_str))
    shutdown_ms = StringToInt64(shutdown_ms_str);
  file_util::Delete(shutdown_ms_file, false);

  PrefService* prefs = g_browser_process->local_state();
  ShutdownType type =
      static_cast<ShutdownType>(prefs->GetInteger(prefs::kShutdownType));
  int num_procs = prefs->GetInteger(prefs::kShutdownNumProcesses);
  int num_procs_slow = prefs->GetInteger(prefs::kShutdownNumProcessesSlow);
  // clear the prefs immediately so we don't pick them up on a future run
  prefs->SetInteger(prefs::kShutdownType, NOT_VALID);
  prefs->SetInteger(prefs::kShutdownNumProcesses, 0);
  prefs->SetInteger(prefs::kShutdownNumProcessesSlow, 0);

  if (type > NOT_VALID && shutdown_ms > 0 && num_procs > 0) {
    const wchar_t *time_fmt = L"Shutdown.%ls.time";
    const wchar_t *time_per_fmt = L"Shutdown.%ls.time_per_process";
    std::wstring time;
    std::wstring time_per;
    if (type == WINDOW_CLOSE) {
      time = StringPrintf(time_fmt, L"window_close");
      time_per = StringPrintf(time_per_fmt, L"window_close");
    } else if (type == BROWSER_EXIT) {
      time = StringPrintf(time_fmt, L"browser_exit");
      time_per = StringPrintf(time_per_fmt, L"browser_exit");
    } else if (type == END_SESSION) {
      time = StringPrintf(time_fmt, L"end_session");
      time_per = StringPrintf(time_per_fmt, L"end_session");
    } else {
      NOTREACHED();
    }
    if (time.length()) {
      // TODO(erikkay): change these to UMA histograms after a bit more testing.
      UMA_HISTOGRAM_TIMES(time.c_str(),
                          TimeDelta::FromMilliseconds(shutdown_ms));
      UMA_HISTOGRAM_TIMES(time_per.c_str(),
                          TimeDelta::FromMilliseconds(shutdown_ms / num_procs));
      UMA_HISTOGRAM_COUNTS_100(L"Shutdown.renderers.total", num_procs);
      UMA_HISTOGRAM_COUNTS_100(L"Shutdown.renderers.slow", num_procs_slow);
    }
  }
}

}  // namespace browser_shutdown
