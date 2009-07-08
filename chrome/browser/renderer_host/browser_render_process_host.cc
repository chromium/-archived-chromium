// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.

#include "chrome/browser/renderer_host/browser_render_process_host.h"

#include "build/build_config.h"

#include <algorithm>

#include "app/app_switches.h"
#if defined(OS_WIN)
#include "app/win_util.h"
#endif
#include "base/command_line.h"
#include "base/field_trial.h"
#include "base/linked_ptr.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/visitedlink_master.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/common/child_process_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/result_codes.h"
#include "chrome/renderer/render_process.h"
#include "chrome/installer/util/google_update_settings.h"
#include "grit/generated_resources.h"

#if defined(OS_LINUX)
#include "chrome/browser/zygote_host_linux.h"
#include "chrome/browser/renderer_host/render_crash_handler_host_linux.h"
#include "chrome/browser/renderer_host/render_sandbox_host_linux.h"
#endif

using WebKit::WebCache;

#if defined(OS_WIN)

// TODO(port): see comment by the only usage of RenderViewHost in this file.
#include "chrome/browser/renderer_host/render_view_host.h"

#include "chrome/browser/spellchecker.h"

// Once the above TODO is finished, then this block is all Windows-specific
// files.
#include "base/win_util.h"
#include "chrome/browser/sandbox_policy.h"
#include "sandbox/src/sandbox.h"
#elif defined(OS_POSIX)
// TODO(port): Remove temporary scaffolding after porting the above headers.
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

#include "third_party/skia/include/core/SkBitmap.h"


// This class creates the IO thread for the renderer when running in
// single-process mode.  It's not used in multi-process mode.
class RendererMainThread : public base::Thread {
 public:
  explicit RendererMainThread(const std::string& channel_id)
      : base::Thread("Chrome_InProcRendererThread"),
        channel_id_(channel_id),
        render_process_(NULL) {
  }

  ~RendererMainThread() {
    Stop();
  }

 protected:
  virtual void Init() {
#if defined(OS_WIN)
    CoInitialize(NULL);
#endif

    render_process_ = new RenderProcess(channel_id_);
    // It's a little lame to manually set this flag.  But the single process
    // RendererThread will receive the WM_QUIT.  We don't need to assert on
    // this thread, so just force the flag manually.
    // If we want to avoid this, we could create the InProcRendererThread
    // directly with _beginthreadex() rather than using the Thread class.
    base::Thread::SetThreadWasQuitProperly(true);
  }

  virtual void CleanUp() {
    delete render_process_;

#if defined(OS_WIN)
    CoUninitialize();
#endif
  }

 private:
  std::string channel_id_;
  // Deleted in CleanUp() on the renderer thread, so don't use a smart pointer.
  RenderProcess* render_process_;
};


// Size of the buffer after which individual link updates deemed not warranted
// and the overall update should be used instead.
static const unsigned kVisitedLinkBufferThreshold = 50;

// This class manages buffering and sending visited link hashes (fingerprints)
// to renderer based on widget visibility.
// As opposed to the VisitedLinkEventListener in profile.cc, which coalesces to
// reduce the rate of messages being send to render processes, this class
// ensures that the updates occur only when explicitly requested. This is
// used by BrowserRenderProcessHost to only send Add/Reset link events to the
// renderers when their tabs are visible.
class VisitedLinkUpdater {
 public:
  VisitedLinkUpdater() : threshold_reached_(false) {}

  void Buffer(const VisitedLinkCommon::Fingerprints& links) {
    if (threshold_reached_)
      return;

    if (pending_.size() + links.size() > kVisitedLinkBufferThreshold) {
      threshold_reached_ = true;
      // Once the threshold is reached, there's no need to store pending visited
      // links.
      pending_.clear();
      return;
    }

    pending_.insert(pending_.end(), links.begin(), links.end());
  }

  void Clear() {
    pending_.clear();
  }

  void Update(IPC::Channel::Sender* sender) {
    if (threshold_reached_) {
      sender->Send(new ViewMsg_VisitedLink_Reset());
      threshold_reached_ = false;
      return;
    }

    if (pending_.size() == 0)
      return;

    sender->Send(new ViewMsg_VisitedLink_Add(pending_));

    pending_.clear();
  }

 private:
  bool threshold_reached_;
  VisitedLinkCommon::Fingerprints pending_;
};


// Used for a View_ID where the renderer has not been attached yet
const int32 kInvalidViewID = -1;

// Get the path to the renderer executable, which is the same as the
// current executable.
bool GetRendererPath(std::wstring* cmd_line) {
  return PathService::Get(base::FILE_EXE, cmd_line);
}

BrowserRenderProcessHost::BrowserRenderProcessHost(Profile* profile)
    : RenderProcessHost(profile),
      visible_widgets_(0),
      backgrounded_(true),
      ALLOW_THIS_IN_INITIALIZER_LIST(cached_dibs_cleaner_(
            base::TimeDelta::FromSeconds(5),
            this, &BrowserRenderProcessHost::ClearTransportDIBCache)),
      zygote_child_(false) {
  widget_helper_ = new RenderWidgetHelper();

  registrar_.Add(this, NotificationType::USER_SCRIPTS_UPDATED,
                 NotificationService::AllSources());

  if (run_renderer_in_process()) {
    // We need a "renderer pid", but we don't have one when there's no renderer
    // process.  So pick a value that won't clash with other child process pids.
    // Linux has PID_MAX_LIMIT which is 2^22.  Windows always uses pids that are
    // divisible by 4.  So...
    static int next_pid = 4 * 1024 * 1024;
    next_pid += 3;
    SetProcessID(next_pid);
  }

  visited_link_updater_.reset(new VisitedLinkUpdater());

  // Note: When we create the BrowserRenderProcessHost, it's technically
  //       backgrounded, because it has no visible listeners.  But the process
  //       doesn't actually exist yet, so we'll Background it later, after
  //       creation.
}

BrowserRenderProcessHost::~BrowserRenderProcessHost() {
  if (pid() >= 0) {
    WebCacheManager::GetInstance()->Remove(pid());
    ChildProcessSecurityPolicy::GetInstance()->Remove(pid());
  }

  // We may have some unsent messages at this point, but that's OK.
  channel_.reset();

  // Destroy the AudioRendererHost properly.
  if (audio_renderer_host_.get())
    audio_renderer_host_->Destroy();

  if (process_.handle() && !run_renderer_in_process()) {
    if (zygote_child_) {
#if defined(OS_LINUX)
      Singleton<ZygoteHost>()->EnsureProcessTerminated(process_.handle());
#endif
    } else {
      ProcessWatcher::EnsureProcessTerminated(process_.handle());
    }
  }

  ClearTransportDIBCache();
}

bool BrowserRenderProcessHost::Init() {
  // calling Init() more than once does nothing, this makes it more convenient
  // for the view host which may not be sure in some cases
  if (channel_.get())
    return true;

  // run the IPC channel on the shared IO thread.
  base::Thread* io_thread = g_browser_process->io_thread();

  // Construct the AudioRendererHost with the IO thread.
  audio_renderer_host_ =
      new AudioRendererHost(io_thread->message_loop());

  scoped_refptr<ResourceMessageFilter> resource_message_filter =
      new ResourceMessageFilter(g_browser_process->resource_dispatcher_host(),
                                audio_renderer_host_.get(),
                                PluginService::GetInstance(),
                                g_browser_process->print_job_manager(),
                                profile(),
                                widget_helper_,
                                profile()->GetSpellChecker());

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();

  // setup IPC channel
  const std::string channel_id =
      ChildProcessInfo::GenerateRandomChannelID(this);
  channel_.reset(
      new IPC::SyncChannel(channel_id, IPC::Channel::MODE_SERVER, this,
                           resource_message_filter,
                           io_thread->message_loop(), true,
                           g_browser_process->shutdown_event()));
  // As a preventive mesure, we DCHECK if someone sends a synchronous message
  // with no time-out, which in the context of the browser process we should not
  // be doing.
  channel_->set_sync_messages_with_no_timeout_allowed(false);

  // Build command line for renderer, we have to quote the executable name to
  // deal with spaces.
  std::wstring renderer_path =
      browser_command_line.GetSwitchValue(switches::kBrowserSubprocessPath);
  if (renderer_path.empty()) {
    if (!GetRendererPath(&renderer_path)) {
      // Need to reset the channel we created above or others might think the
      // connection is live.
      channel_.reset();
      return false;
    }
  }
  CommandLine cmd_line(renderer_path);
  if (logging::DialogsAreSuppressed())
    cmd_line.AppendSwitch(switches::kNoErrorDialogs);

  // propagate the following switches to the renderer command line
  // (along with any associated values) if present in the browser command line
  static const wchar_t* const switch_names[] = {
    switches::kRendererAssertTest,
    switches::kRendererCrashTest,
    switches::kRendererStartupDialog,
    switches::kNoSandbox,
    switches::kTestSandbox,
#if !defined (GOOGLE_CHROME_BUILD)
    // This is an unsupported and not fully tested mode, so don't enable it for
    // official Chrome builds.
    switches::kInProcessPlugins,
#endif
    switches::kDomAutomationController,
    switches::kUserAgent,
    switches::kJavaScriptFlags,
    switches::kRecordMode,
    switches::kPlaybackMode,
    switches::kNoJsRandomness,
    switches::kDisableBreakpad,
    switches::kFullMemoryCrashReport,
    switches::kEnableLogging,
    switches::kDumpHistogramsOnExit,
    switches::kDisableLogging,
    switches::kLoggingLevel,
    switches::kDebugPrint,
    switches::kAllowAllActiveX,
    switches::kMemoryProfiling,
    switches::kEnableWatchdog,
    switches::kMessageLoopHistogrammer,
    switches::kEnableDCHECK,
    switches::kSilentDumpOnDCHECK,
    switches::kUseLowFragHeapCrt,
    switches::kEnableStatsTable,
    switches::kAutoSpellCorrect,
    switches::kDisableAudio,
    switches::kSimpleDataSource,
    switches::kEnableBenchmarking,
    switches::kIsolatedWorld,
  };

  for (size_t i = 0; i < arraysize(switch_names); ++i) {
    if (browser_command_line.HasSwitch(switch_names[i])) {
      cmd_line.AppendSwitchWithValue(switch_names[i],
          browser_command_line.GetSwitchValue(switch_names[i]));
    }
  }

  // Tell the renderer to enable extensions if there are any extensions loaded.
  //
  // NOTE: This is subtly different than just passing along whether
  // --enable-extenisons is present in the browser process. For example, there
  // is also an extensions.enabled preference, and there may be various special
  // cases about whether to allow extensions to load.
  //
  // This introduces a race condition where the first renderer never gets
  // extensions enabled, so we also set the flag if extensions_enabled(). This
  // isn't perfect though, because of the special cases above.
  //
  // TODO(aa): We need to get rid of the need to pass this flag at all. It is
  // only used in one place in the renderer.
  if (profile()->GetExtensionsService()) {
    if (profile()->GetExtensionsService()->extensions()->size() > 0 ||
        profile()->GetExtensionsService()->extensions_enabled())
      cmd_line.AppendSwitch(switches::kEnableExtensions);
  }

  // Pass on the browser locale.
  const std::string locale = g_browser_process->GetApplicationLocale();
  cmd_line.AppendSwitchWithValue(switches::kLang, ASCIIToWide(locale));

  // If we run FieldTrials, we want to pass to their state to the renderer so
  // that it can act in accordance with each state, or record histograms
  // relating to the FieldTrial states.
  std::string field_trial_states;
  FieldTrialList::StatesToString(&field_trial_states);
  if (!field_trial_states.empty())
    cmd_line.AppendSwitchWithValue(switches::kForceFieldTestNameAndValue,
        ASCIIToWide(field_trial_states));

#if defined(OS_POSIX)
  const bool has_cmd_prefix =
      browser_command_line.HasSwitch(switches::kRendererCmdPrefix);
  if (has_cmd_prefix) {
    // launch the renderer child with some prefix (usually "gdb --args")
    const std::wstring prefix =
        browser_command_line.GetSwitchValue(switches::kRendererCmdPrefix);
    cmd_line.PrependWrapper(prefix);
  }
#endif  // OS_POSIX

#if defined(OS_LINUX)
  if (GoogleUpdateSettings::GetCollectStatsConsent())
    cmd_line.AppendSwitch(switches::kRendererCrashDump);
#endif

  cmd_line.AppendSwitchWithValue(switches::kProcessType,
                                 switches::kRendererProcess);

  cmd_line.AppendSwitchWithValue(switches::kProcessChannelID,
                                 ASCIIToWide(channel_id));

  const std::wstring& profile_path =
      browser_command_line.GetSwitchValue(switches::kUserDataDir);
  if (!profile_path.empty())
    cmd_line.AppendSwitchWithValue(switches::kUserDataDir,
                                   profile_path);

  if (run_renderer_in_process()) {
    // Crank up a thread and run the initialization there.  With the way that
    // messages flow between the browser and renderer, this thread is required
    // to prevent a deadlock in single-process mode.  When using multiple
    // processes, the primordial thread in the renderer process has a message
    // loop which is used for sending messages asynchronously to the io thread
    // in the browser process.  If we don't create this thread, then the
    // RenderThread is both responsible for rendering and also for
    // communicating IO.  This can lead to deadlocks where the RenderThread is
    // waiting for the IO to complete, while the browsermain is trying to pass
    // an event to the RenderThread.
    in_process_renderer_.reset(new RendererMainThread(channel_id));

    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    in_process_renderer_->StartWithOptions(options);
  } else {
    base::ProcessHandle process = 0;
#if defined(OS_WIN)
    process = sandbox::StartProcess(&cmd_line);
#elif defined(OS_POSIX)
#if defined(OS_LINUX)
    if (!has_cmd_prefix) {
      base::GlobalDescriptors::Mapping mapping;
      const int ipcfd = channel_->GetClientFileDescriptor();
      mapping.push_back(std::pair<uint32_t, int>(kPrimaryIPCChannel, ipcfd));
      const int crash_signal_fd =
          Singleton<RenderCrashHandlerHostLinux>()->GetDeathSignalSocket();
      if (crash_signal_fd >= 0) {
        mapping.push_back(std::pair<uint32_t, int>(kCrashDumpSignal,
                                                   crash_signal_fd));
      }
      process = Singleton<ZygoteHost>()->ForkRenderer(cmd_line.argv(), mapping);
      zygote_child_ = true;
    } else {
#endif
      // NOTE: This code is duplicated with plugin_process_host.cc, but
      // there's not a good place to de-duplicate it.
      base::file_handle_mapping_vector fds_to_map;
      const int ipcfd = channel_->GetClientFileDescriptor();
      fds_to_map.push_back(std::make_pair(ipcfd, kPrimaryIPCChannel + 3));
#if defined(OS_LINUX)
      const int crash_signal_fd =
          Singleton<RenderCrashHandlerHostLinux>()->GetDeathSignalSocket();
      if (crash_signal_fd >= 0) {
        fds_to_map.push_back(std::make_pair(crash_signal_fd,
                                            kCrashDumpSignal + 3));
      }
      const int sandbox_fd =
          Singleton<RenderSandboxHostLinux>()->GetRendererSocket();
      fds_to_map.push_back(std::make_pair(sandbox_fd, kSandboxIPCChannel + 3));
#endif
      base::LaunchApp(cmd_line.argv(), fds_to_map, false, &process);
      zygote_child_ = false;
#if defined(OS_LINUX)
    }
#endif
#endif

    if (!process) {
      channel_.reset();
      return false;
    }
    process_.set_handle(process);
    SetProcessID(process_.pid());
  }

  resource_message_filter->Init(pid());
  WebCacheManager::GetInstance()->Add(pid());
  ChildProcessSecurityPolicy::GetInstance()->Add(pid());

  // Now that the process is created, set its backgrounding accordingly.
  SetBackgrounded(backgrounded_);

  InitVisitedLinks();
  InitUserScripts();
  InitExtensions();

  if (max_page_id_ != -1)
    channel_->Send(new ViewMsg_SetNextPageID(max_page_id_ + 1));

  return true;
}

int BrowserRenderProcessHost::GetNextRoutingID() {
  return widget_helper_->GetNextRoutingID();
}

void BrowserRenderProcessHost::CancelResourceRequests(int render_widget_id) {
  widget_helper_->CancelResourceRequests(render_widget_id);
}

void BrowserRenderProcessHost::CrossSiteClosePageACK(
    int new_render_process_host_id,
    int new_request_id) {
  widget_helper_->CrossSiteClosePageACK(new_render_process_host_id,
                                        new_request_id);
}

bool BrowserRenderProcessHost::WaitForPaintMsg(int render_widget_id,
                                               const base::TimeDelta& max_delay,
                                               IPC::Message* msg) {
  return widget_helper_->WaitForPaintMsg(render_widget_id, max_delay, msg);
}

void BrowserRenderProcessHost::ReceivedBadMessage(uint16 msg_type) {
  BadMessageTerminateProcess(msg_type, process_.handle());
}

void BrowserRenderProcessHost::WidgetRestored() {
  // Verify we were properly backgrounded.
  DCHECK(backgrounded_ == (visible_widgets_ == 0));
  visible_widgets_++;
  visited_link_updater_->Update(this);
  SetBackgrounded(false);
}

void BrowserRenderProcessHost::WidgetHidden() {
  // On startup, the browser will call Hide
  if (backgrounded_)
    return;

  DCHECK(backgrounded_ == (visible_widgets_ == 0));
  visible_widgets_--;
  DCHECK(visible_widgets_ >= 0);
  if (visible_widgets_ == 0) {
    DCHECK(!backgrounded_);
    SetBackgrounded(true);
  }
}

void BrowserRenderProcessHost::AddWord(const std::wstring& word) {
#if !defined(OS_WIN)
  // TODO(port): reimplement when we get the spell checker up and running on
  // other platforms.
  NOTIMPLEMENTED();
#else
  base::Thread* io_thread = g_browser_process->io_thread();
  if (profile()->GetSpellChecker()) {
    io_thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        profile()->GetSpellChecker(), &SpellChecker::AddWord, word));
  }
#endif // !defined(OS_WIN)
}

void BrowserRenderProcessHost::AddVisitedLinks(
    const VisitedLinkCommon::Fingerprints& links) {
  visited_link_updater_->Buffer(links);
  if (visible_widgets_ == 0)
    return;

  visited_link_updater_->Update(this);
}

void BrowserRenderProcessHost::ResetVisitedLinks() {
  visited_link_updater_->Clear();
  Send(new ViewMsg_VisitedLink_Reset());
}

base::ProcessHandle BrowserRenderProcessHost::GetRendererProcessHandle() {
  if (run_renderer_in_process())
    return base::Process::Current().handle();
  return process_.handle();
}

void BrowserRenderProcessHost::InitVisitedLinks() {
  VisitedLinkMaster* visitedlink_master = profile()->GetVisitedLinkMaster();
  if (!visitedlink_master) {
    return;
  }

  base::SharedMemoryHandle handle_for_process;
  bool r = visitedlink_master->ShareToProcess(GetRendererProcessHandle(),
                                              &handle_for_process);
  DCHECK(r);

  if (base::SharedMemory::IsHandleValid(handle_for_process)) {
    channel_->Send(new ViewMsg_VisitedLink_NewTable(handle_for_process));
  }
}

void BrowserRenderProcessHost::InitUserScripts() {
  UserScriptMaster* user_script_master = profile()->GetUserScriptMaster();
  DCHECK(user_script_master);

  if (!user_script_master->ScriptsReady()) {
    // No scripts ready.  :(
    return;
  }

  // Update the renderer process with the current scripts.
  SendUserScriptsUpdate(user_script_master->GetSharedMemory());
}

void BrowserRenderProcessHost::InitExtensions() {
  // TODO(aa): Should only bother sending these function names if this is an
  // extension process.
  std::vector<std::string> function_names;
  ExtensionFunctionDispatcher::GetAllFunctionNames(&function_names);
  Send(new ViewMsg_Extension_SetFunctionNames(function_names));
}

void BrowserRenderProcessHost::SendUserScriptsUpdate(
    base::SharedMemory *shared_memory) {
  base::SharedMemoryHandle handle_for_process;
  if (!shared_memory->ShareToProcess(GetRendererProcessHandle(),
                                     &handle_for_process)) {
    // This can legitimately fail if the renderer asserts at startup.
    return;
  }

  if (base::SharedMemory::IsHandleValid(handle_for_process)) {
    channel_->Send(new ViewMsg_UserScripts_UpdatedScripts(handle_for_process));
  }
}

bool BrowserRenderProcessHost::FastShutdownIfPossible() {
  if (!process_.handle())
    return false;  // Render process is probably crashed.
  if (BrowserRenderProcessHost::run_renderer_in_process())
    return false;  // Since process mode can't do fast shutdown.

  // Test if there's an unload listener.
  // NOTE: It's possible that an onunload listener may be installed
  // while we're shutting down, so there's a small race here.  Given that
  // the window is small, it's unlikely that the web page has much
  // state that will be lost by not calling its unload handlers properly.
  if (!sudden_termination_allowed())
    return false;

  // Check for any external tab containers, since they may still be running even
  // though this window closed.
  BrowserRenderProcessHost::listeners_iterator iter;
  // NOTE: This is a bit dangerous.  We know that for now, listeners are
  // always RenderWidgetHosts.  But in theory, they don't have to be.
  for (iter = listeners_begin(); iter != listeners_end(); ++iter) {
    RenderWidgetHost* widget = static_cast<RenderWidgetHost*>(iter->second);
    DCHECK(widget);
    if (!widget || !widget->IsRenderView())
      continue;
    RenderViewHost* rvh = static_cast<RenderViewHost*>(widget);
    if (rvh->delegate()->IsExternalTabContainer())
      return false;
  }

  // Otherwise, we're allowed to just terminate the process. Using exit code 0
  // means that UMA won't treat this as a renderer crash.
  process_.Terminate(ResultCodes::NORMAL_EXIT);
  return true;
}

bool BrowserRenderProcessHost::SendWithTimeout(IPC::Message* msg,
                                               int timeout_ms) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }
  return channel_->SendWithTimeout(msg, timeout_ms);
}

// This is a platform specific function for mapping a transport DIB given its id
TransportDIB* BrowserRenderProcessHost::MapTransportDIB(
    TransportDIB::Id dib_id) {
#if defined(OS_WIN)
  // On Windows we need to duplicate the handle from the remote process
  HANDLE section = win_util::GetSectionFromProcess(
      dib_id.handle, GetRendererProcessHandle(), false /* read write */);
  return TransportDIB::Map(section);
#elif defined(OS_MACOSX)
  // On OSX, the browser allocates all DIBs and keeps a file descriptor around
  // for each.
  return widget_helper_->MapTransportDIB(dib_id);
#elif defined(OS_LINUX)
  return TransportDIB::Map(dib_id);
#endif  // defined(OS_LINUX)
}

TransportDIB* BrowserRenderProcessHost::GetTransportDIB(
    TransportDIB::Id dib_id) {
  const std::map<TransportDIB::Id, TransportDIB*>::iterator
      i = cached_dibs_.find(dib_id);
  if (i != cached_dibs_.end()) {
    cached_dibs_cleaner_.Reset();
    return i->second;
  }

  TransportDIB* dib = MapTransportDIB(dib_id);
  if (!dib)
    return NULL;

  if (cached_dibs_.size() >= MAX_MAPPED_TRANSPORT_DIBS) {
    // Clean a single entry from the cache
    std::map<TransportDIB::Id, TransportDIB*>::iterator smallest_iterator;
    size_t smallest_size = std::numeric_limits<size_t>::max();

    for (std::map<TransportDIB::Id, TransportDIB*>::iterator
         i = cached_dibs_.begin(); i != cached_dibs_.end(); ++i) {
      if (i->second->size() <= smallest_size) {
        smallest_iterator = i;
        smallest_size = i->second->size();
      }
    }

    delete smallest_iterator->second;
    cached_dibs_.erase(smallest_iterator);
  }

  cached_dibs_[dib_id] = dib;
  cached_dibs_cleaner_.Reset();
  return dib;
}

void BrowserRenderProcessHost::ClearTransportDIBCache() {
  for (std::map<TransportDIB::Id, TransportDIB*>::iterator
       i = cached_dibs_.begin(); i != cached_dibs_.end(); ++i) {
    delete i->second;
  }

  cached_dibs_.clear();
}

bool BrowserRenderProcessHost::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }
  return channel_->Send(msg);
}

void BrowserRenderProcessHost::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // dispatch control messages
    bool msg_is_ok = true;
    IPC_BEGIN_MESSAGE_MAP_EX(BrowserRenderProcessHost, msg, msg_is_ok)
      IPC_MESSAGE_HANDLER(ViewHostMsg_PageContents, OnPageContents)
      IPC_MESSAGE_HANDLER(ViewHostMsg_UpdatedCacheStats,
                          OnUpdatedCacheStats)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SuddenTerminationChanged,
                          SuddenTerminationChanged);
      IPC_MESSAGE_HANDLER(ViewHostMsg_ExtensionAddListener,
                          OnExtensionAddListener)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ExtensionRemoveListener,
                          OnExtensionRemoveListener)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ExtensionCloseChannel,
                          OnExtensionCloseChannel)
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP_EX()

    if (!msg_is_ok) {
      // The message had a handler, but its de-serialization failed.
      // We consider this a capital crime. Kill the renderer if we have one.
      ReceivedBadMessage(msg.type());
    }
    return;
  }

  // dispatch incoming messages to the appropriate TabContents
  IPC::Channel::Listener* listener = GetListenerByID(msg.routing_id());
  if (!listener) {
    if (msg.is_sync()) {
      // The listener has gone away, so we must respond or else the caller will
      // hang waiting for a reply.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
      reply->set_reply_error();
      Send(reply);
    }
    return;
  }
  listener->OnMessageReceived(msg);
}

void BrowserRenderProcessHost::OnChannelConnected(int32 peer_pid) {
  // process_ is not NULL if we created the renderer process
  if (!process_.handle()) {
    if (base::GetCurrentProcId() == peer_pid) {
      // We are in single-process mode. In theory we should have access to
      // ourself but it may happen that we don't.
      process_.set_handle(base::GetCurrentProcessHandle());
    } else {
#if defined(OS_WIN)
      // Request MAXIMUM_ALLOWED to match the access a handle
      // returned by CreateProcess() has to the process object.
      process_.set_handle(OpenProcess(MAXIMUM_ALLOWED, FALSE, peer_pid));
#elif defined(OS_POSIX)
      // ProcessHandle is just a pid.
      process_.set_handle(peer_pid);
#endif
      DCHECK(process_.handle());
    }
  } else {
    // Need to verify that the peer_pid is actually the process we know, if
    // it is not, we need to panic now. See bug 1002150.
    if (peer_pid != process_.pid()) {
      // In the case that we are running the renderer in a wrapper, this check
      // is invalid as it's the wrapper PID that we'll have, not the actual
      // renderer
      const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
      if (cmd_line.HasSwitch(switches::kRendererCmdPrefix))
        return;
      CHECK(peer_pid == process_.pid()) << peer_pid << " " << process_.pid();
    }
  }
}

// Static. This function can be called from the IO Thread or from the UI thread.
void BrowserRenderProcessHost::BadMessageTerminateProcess(
    uint16 msg_type,
    base::ProcessHandle process) {
  LOG(ERROR) << "bad message " << msg_type << " terminating renderer.";
  if (BrowserRenderProcessHost::run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just crash.
    CHECK(false);
  }
  NOTREACHED();
  base::KillProcess(process, ResultCodes::KILLED_BAD_MESSAGE, false);
}

void BrowserRenderProcessHost::OnChannelError() {
  // Our child process has died.  If we didn't expect it, it's a crash.
  // In any case, we need to let everyone know it's gone.

  DCHECK(process_.handle());
  DCHECK(channel_.get());

  bool child_exited;
  bool did_crash;
  if (zygote_child_) {
#if defined(OS_LINUX)
    did_crash = Singleton<ZygoteHost>()->DidProcessCrash(
        process_.handle(), &child_exited);
#else
    NOTREACHED();
    did_crash = true;
#endif
  } else {
    did_crash = base::DidProcessCrash(&child_exited, process_.handle());
  }

  NotificationService::current()->Notify(
      NotificationType::RENDERER_PROCESS_CLOSED,
      Source<RenderProcessHost>(this),
      Details<bool>(&did_crash));

  // POSIX: If the process crashed, then the kernel closed the socket for it
  // and so the child has already died by the time we get here. Since
  // DidProcessCrash called waitpid with WNOHANG, it'll reap the process.
  // However, if DidProcessCrash didn't reap the child, we'll need to in
  // ~BrowserRenderProcessHost via ProcessWatcher. So we can't close the handle
  // here.
  //
  // This is moot on Windows where |child_exited| will always be true.
  if (child_exited)
    process_.Close();

  WebCacheManager::GetInstance()->Remove(pid());
  ChildProcessSecurityPolicy::GetInstance()->Remove(pid());

  channel_.reset();

  // This process should detach all the listeners, causing the object to be
  // deleted. We therefore need a stack copy of the web view list to avoid
  // crashing when checking for the termination condition the last time.
  IDMap<IPC::Channel::Listener> local_listeners(listeners_);
  for (listeners_iterator i = local_listeners.begin();
       i != local_listeners.end(); ++i) {
    i->second->OnMessageReceived(ViewHostMsg_RenderViewGone(i->first));
  }

  ClearTransportDIBCache();

  // this object is not deleted at this point and may be reused later.
  // TODO(darin): clean this up
}

void BrowserRenderProcessHost::OnPageContents(const GURL& url,
                                       int32 page_id,
                                       const std::wstring& contents) {
  Profile* p = profile();
  if (!p || p->IsOffTheRecord())
    return;

  HistoryService* hs = p->GetHistoryService(Profile::IMPLICIT_ACCESS);
  if (hs)
    hs->SetPageContents(url, contents);
}

void BrowserRenderProcessHost::OnUpdatedCacheStats(
    const WebCache::UsageStats& stats) {
  WebCacheManager::GetInstance()->ObserveStats(pid(), stats);
}

void BrowserRenderProcessHost::SuddenTerminationChanged(bool enabled) {
  set_sudden_termination_allowed(enabled);
}

void BrowserRenderProcessHost::SetBackgrounded(bool backgrounded) {
  // If the process_ is NULL, the process hasn't been created yet.
  if (process_.handle()) {
    bool should_set_backgrounded = true;

#if defined(OS_WIN)
    // The cbstext.dll loads as a global GetMessage hook in the browser process
    // and intercepts/unintercepts the kernel32 API SetPriorityClass in a
    // background thread. If the UI thread invokes this API just when it is
    // intercepted the stack is messed up on return from the interceptor
    // which causes random crashes in the browser process. Our hack for now
    // is to not invoke the SetPriorityClass API if the dll is loaded.
    should_set_backgrounded = (GetModuleHandle(L"cbstext.dll") == NULL);
#endif // OS_WIN

    if (should_set_backgrounded) {
      bool rv = process_.SetProcessBackgrounded(backgrounded);
      if (!rv) {
        return;
      }
    }

    // Now tune the memory footprint of the renderer.
    // If the OS needs to page, we'd rather it page idle renderers.
    BrowserProcess::MemoryModel model = g_browser_process->memory_model();
    if (model < BrowserProcess::HIGH_MEMORY_MODEL) {
      if (backgrounded) {
        if (model == BrowserProcess::LOW_MEMORY_MODEL)
          process_.EmptyWorkingSet();
        else if (model == BrowserProcess::MEDIUM_MEMORY_MODEL)
          process_.ReduceWorkingSet();
      } else {
        if (model == BrowserProcess::MEDIUM_MEMORY_MODEL)
          process_.UnReduceWorkingSet();
      }
    }
  }

  // Note: we always set the backgrounded_ value.  If the process is NULL
  // (and hence hasn't been created yet), we will set the process priority
  // later when we create the process.
  backgrounded_ = backgrounded;
}

// NotificationObserver implementation.
void BrowserRenderProcessHost::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::USER_SCRIPTS_UPDATED: {
      base::SharedMemory* shared_memory =
          Details<base::SharedMemory>(details).ptr();
      if (shared_memory) {
        SendUserScriptsUpdate(shared_memory);
      }
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void BrowserRenderProcessHost::OnExtensionAddListener(
    const std::string& event_name) {
  ExtensionMessageService::GetInstance(profile()->GetRequestContext())->
      AddEventListener(event_name, pid());
}

void BrowserRenderProcessHost::OnExtensionRemoveListener(
    const std::string& event_name) {
  ExtensionMessageService::GetInstance(profile()->GetRequestContext())->
      RemoveEventListener(event_name, pid());
}

void BrowserRenderProcessHost::OnExtensionCloseChannel(int port_id) {
  ExtensionMessageService::GetInstance(profile()->GetRequestContext())->
      CloseChannel(port_id);
}
