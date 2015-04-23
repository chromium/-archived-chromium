// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/plugin_process_host.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <vector>

#include "app/app_switches.h"
#include "base/command_line.h"
#if defined(OS_POSIX)
#include "base/global_descriptors_posix.h"
#endif
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/gfx/native_widget_types.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_plugin_browsing_context.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/ipc_channel_handle.h"
#include "net/base/cookie_monster.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/plugins/plugin_constants_win.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#include "chrome/browser/sandbox_policy.h"
#include "sandbox/src/sandbox.h"
#endif

#if defined(OS_POSIX)
#include "chrome/common/ipc_channel_posix.h"
#endif

#if defined(OS_LINUX)
#include "base/gfx/gtk_native_view_id_manager.h"
#endif

static const char kDefaultPluginFinderURL[] =
    "http://dl.google.com/chrome/plugins/plugins2.xml";

#if defined(OS_WIN)

// The PluginDownloadUrlHelper is used to handle one download URL request
// from the plugin. Each download request is handled by a new instance
// of this class.
class PluginDownloadUrlHelper : public URLRequest::Delegate {
  static const int kDownloadFileBufferSize = 32768;
 public:
  PluginDownloadUrlHelper(const std::string& download_url,
                          int source_pid, gfx::NativeWindow caller_window);
  ~PluginDownloadUrlHelper();

  void InitiateDownload();

  // URLRequest::Delegate
  virtual void OnReceivedRedirect(URLRequest* request,
                                  const GURL& new_url);
  virtual void OnAuthRequired(URLRequest* request,
                              net::AuthChallengeInfo* auth_info);
  virtual void OnSSLCertificateError(URLRequest* request,
                                     int cert_error,
                                     net::X509Certificate* cert);
  virtual void OnResponseStarted(URLRequest* request);
  virtual void OnReadCompleted(URLRequest* request, int bytes_read);

  void OnDownloadCompleted(URLRequest* request);

 protected:
  void DownloadCompletedHelper(bool success);

  // The download file request initiated by the plugin.
  URLRequest* download_file_request_;
  // Handle to the downloaded file.
  scoped_ptr<net::FileStream> download_file_;
  // The full path of the downloaded file.
  FilePath download_file_path_;
  // The buffer passed off to URLRequest::Read.
  scoped_refptr<net::IOBuffer> download_file_buffer_;
  // TODO(port): this comment doesn't describe the situation on Posix.
  // The window handle for sending the WM_COPYDATA notification,
  // indicating that the download completed.
  gfx::NativeWindow download_file_caller_window_;

  std::string download_url_;
  int download_source_pid_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginDownloadUrlHelper);
};

PluginDownloadUrlHelper::PluginDownloadUrlHelper(
    const std::string& download_url,
    int source_pid, gfx::NativeWindow caller_window)
    : download_file_request_(NULL),
      download_file_buffer_(new net::IOBuffer(kDownloadFileBufferSize)),
      download_file_caller_window_(caller_window),
      download_url_(download_url),
      download_source_pid_(source_pid) {
  DCHECK(::IsWindow(caller_window));
  memset(download_file_buffer_->data(), 0, kDownloadFileBufferSize);
  download_file_.reset(new net::FileStream());
}

PluginDownloadUrlHelper::~PluginDownloadUrlHelper() {
  if (download_file_request_) {
    delete download_file_request_;
    download_file_request_ = NULL;
  }
}

void PluginDownloadUrlHelper::InitiateDownload() {
  download_file_request_= new URLRequest(GURL(download_url_), this);
  download_file_request_->set_origin_pid(download_source_pid_);
  download_file_request_->set_context(Profile::GetDefaultRequestContext());
  download_file_request_->Start();
}

void PluginDownloadUrlHelper::OnReceivedRedirect(URLRequest* request,
                                                 const GURL& new_url) {
}

void PluginDownloadUrlHelper::OnAuthRequired(
    URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  URLRequest::Delegate::OnAuthRequired(request, auth_info);
  DownloadCompletedHelper(false);
}

void PluginDownloadUrlHelper::OnSSLCertificateError(
    URLRequest* request,
    int cert_error,
    net::X509Certificate* cert) {
  URLRequest::Delegate::OnSSLCertificateError(request, cert_error, cert);
  DownloadCompletedHelper(false);
}

void PluginDownloadUrlHelper::OnResponseStarted(URLRequest* request) {
  if (!download_file_->IsOpen()) {
    file_util::GetTempDir(&download_file_path_);

    GURL request_url = request->url();
    download_file_path_ = download_file_path_.Append(
        UTF8ToWide(request_url.ExtractFileName()));
    download_file_->Open(download_file_path_,
                         base::PLATFORM_FILE_CREATE_ALWAYS |
                         base::PLATFORM_FILE_READ | base::PLATFORM_FILE_WRITE);
    if (!download_file_->IsOpen()) {
      NOTREACHED();
      OnDownloadCompleted(request);
      return;
    }
  }
  if (!request->status().is_success()) {
    OnDownloadCompleted(request);
  } else {
    // Initiate a read.
    int bytes_read = 0;
    if (!request->Read(download_file_buffer_, kDownloadFileBufferSize,
                       &bytes_read)) {
      // If the error is not an IO pending, then we're done
      // reading.
      if (!request->status().is_io_pending()) {
        OnDownloadCompleted(request);
      }
    } else if (bytes_read == 0) {
      OnDownloadCompleted(request);
    } else {
      OnReadCompleted(request, bytes_read);
    }
  }
}

void PluginDownloadUrlHelper::OnReadCompleted(URLRequest* request,
                                              int bytes_read) {
  DCHECK(download_file_->IsOpen());

  if (bytes_read == 0) {
    OnDownloadCompleted(request);
    return;
  }

  int request_bytes_read = bytes_read;

  while (request->status().is_success()) {
    int bytes_written = download_file_->Write(download_file_buffer_->data(),
        request_bytes_read, NULL);
    DCHECK((bytes_written < 0) || (bytes_written == request_bytes_read));

    if ((bytes_written < 0) || (bytes_written != request_bytes_read)) {
      DownloadCompletedHelper(false);
      break;
    }

    // Start reading
    request_bytes_read = 0;
    if (!request->Read(download_file_buffer_, kDownloadFileBufferSize,
                       &request_bytes_read)) {
      if (!request->status().is_io_pending()) {
        // If the error is not an IO pending, then we're done
        // reading.
        OnDownloadCompleted(request);
      }
      break;
    } else if (request_bytes_read == 0) {
      OnDownloadCompleted(request);
      break;
    }
  }
}

void PluginDownloadUrlHelper::OnDownloadCompleted(URLRequest* request) {
  bool success = true;
  if (!request->status().is_success()) {
    success = false;
  } else if (!download_file_->IsOpen()) {
    success = false;
  }

  DownloadCompletedHelper(success);
}

void PluginDownloadUrlHelper::DownloadCompletedHelper(bool success) {
  if (download_file_->IsOpen()) {
      download_file_.reset();
  }

  std::wstring path = download_file_path_.value();
  COPYDATASTRUCT download_file_data = {0};
  download_file_data.cbData =
      static_cast<unsigned long>((path.length() + 1) * sizeof(wchar_t));
  download_file_data.lpData = const_cast<wchar_t *>(path.c_str());
  download_file_data.dwData = success;

  if (::IsWindow(download_file_caller_window_)) {
    ::SendMessage(download_file_caller_window_, WM_COPYDATA, NULL,
                  reinterpret_cast<LPARAM>(&download_file_data));
  }

  // Don't access any members after this.
  delete this;
}

void PluginProcessHost::OnPluginWindowDestroyed(HWND window, HWND parent) {
  // The window is destroyed at this point, we just care about its parent, which
  // is the intermediate window we created.
  std::set<HWND>::iterator window_index =
      plugin_parent_windows_set_.find(parent);
  if (window_index == plugin_parent_windows_set_.end())
    return;

  plugin_parent_windows_set_.erase(window_index);
  PostMessage(parent, WM_CLOSE, 0, 0);
}

void PluginProcessHost::OnDownloadUrl(const std::string& url,
                                      int source_pid,
                                      gfx::NativeWindow caller_window) {
  PluginDownloadUrlHelper* download_url_helper =
      new PluginDownloadUrlHelper(url, source_pid, caller_window);
  download_url_helper->InitiateDownload();
}

void PluginProcessHost::AddWindow(HWND window) {
  plugin_parent_windows_set_.insert(window);
}

#endif  // defined(OS_WIN)

#if defined(OS_LINUX)
void PluginProcessHost::OnMapNativeViewId(gfx::NativeViewId id,
                                          gfx::PluginWindowHandle* output) {
  *output = 0;
  Singleton<GtkNativeViewManager>()->GetXIDForId(output, id);
}
#endif  // defined(OS_LINUX)

PluginProcessHost::PluginProcessHost()
    : ChildProcessHost(
          PLUGIN_PROCESS,
          PluginService::GetInstance()->resource_dispatcher_host()),
      ALLOW_THIS_IN_INITIALIZER_LIST(resolve_proxy_msg_helper_(this, NULL)) {
}

PluginProcessHost::~PluginProcessHost() {
#if defined(OS_WIN)
  // We erase HWNDs from the plugin_parent_windows_set_ when we receive a
  // notification that the window is being destroyed. If we don't receive this
  // notification and the PluginProcessHost instance is being destroyed, it
  // means that the plugin process crashed. We paint a sad face in this case in
  // the renderer process. To ensure that the sad face shows up, and we don't
  // leak HWNDs, we should destroy existing plugin parent windows.
  std::set<HWND>::iterator window_index;
  for (window_index = plugin_parent_windows_set_.begin();
       window_index != plugin_parent_windows_set_.end();
       window_index++) {
    PostMessage(*window_index, WM_CLOSE, 0, 0);
  }
#endif
}

bool PluginProcessHost::Init(const WebPluginInfo& info,
                             const std::string& activex_clsid,
                             const std::wstring& locale) {
  info_ = info;
  set_name(info_.name);

  if (!CreateChannel())
    return false;

  // build command line for plugin, we have to quote the plugin's path to deal
  // with spaces.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  std::wstring exe_path =
      browser_command_line.GetSwitchValue(switches::kBrowserSubprocessPath);
  if (exe_path.empty() && !PathService::Get(base::FILE_EXE, &exe_path))
    return false;

  CommandLine cmd_line(exe_path);
  if (logging::DialogsAreSuppressed())
    cmd_line.AppendSwitch(switches::kNoErrorDialogs);

  // propagate the following switches to the plugin command line (along with
  // any associated values) if present in the browser command line
  static const wchar_t* const switch_names[] = {
    switches::kPluginStartupDialog,
    switches::kNoSandbox,
    switches::kSafePlugins,
    switches::kTestSandbox,
    switches::kUserAgent,
    switches::kDisableBreakpad,
    switches::kFullMemoryCrashReport,
    switches::kEnableLogging,
    switches::kDisableLogging,
    switches::kLoggingLevel,
    switches::kLogPluginMessages,
    switches::kUserDataDir,
    switches::kAllowAllActiveX,
    switches::kEnableDCHECK,
    switches::kSilentDumpOnDCHECK,
    switches::kMemoryProfiling,
    switches::kUseLowFragHeapCrt,
    switches::kEnableStatsTable,
  };

  for (size_t i = 0; i < arraysize(switch_names); ++i) {
    if (browser_command_line.HasSwitch(switch_names[i])) {
      cmd_line.AppendSwitchWithValue(
          switch_names[i],
          browser_command_line.GetSwitchValue(switch_names[i]));
    }
  }

  // If specified, prepend a launcher program to the command line.
  std::wstring plugin_launcher =
      browser_command_line.GetSwitchValue(switches::kPluginLauncher);
  if (!plugin_launcher.empty())
    cmd_line.PrependWrapper(plugin_launcher);

  if (!locale.empty()) {
    // Pass on the locale so the null plugin will use the right language in the
    // prompt to install the desired plugin.
    cmd_line.AppendSwitchWithValue(switches::kLang, locale);
  }

  // Gears requires the data dir to be available on startup.
  std::wstring data_dir =
    PluginService::GetInstance()->GetChromePluginDataDir().ToWStringHack();
  DCHECK(!data_dir.empty());
  cmd_line.AppendSwitchWithValue(switches::kPluginDataDir, data_dir);

  cmd_line.AppendSwitchWithValue(switches::kProcessType,
                                 switches::kPluginProcess);

  cmd_line.AppendSwitchWithValue(switches::kProcessChannelID,
                                 ASCIIToWide(channel_id()));

  cmd_line.AppendSwitchWithValue(switches::kPluginPath,
                                 info.path.ToWStringHack());

  base::ProcessHandle process = 0;
#if defined(OS_WIN)
  process = sandbox::StartProcess(&cmd_line);
#else
  // This code is duplicated with browser_render_process_host.cc, but
  // there's not a good place to de-duplicate it.
  base::file_handle_mapping_vector fds_to_map;
  const int ipcfd = channel().GetClientFileDescriptor();
  if (ipcfd > -1)
    fds_to_map.push_back(std::pair<int, int>(
        ipcfd, kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor));
  base::LaunchApp(cmd_line.argv(), fds_to_map, false, &process);
#endif

  if (!process)
    return false;
  SetHandle(process);

  FilePath gears_path;
  if (PathService::Get(chrome::FILE_GEARS_PLUGIN, &gears_path)) {
    FilePath::StringType gears_path_lc = StringToLowerASCII(gears_path.value());
    FilePath::StringType plugin_path_lc =
        StringToLowerASCII(info.path.value());
    if (plugin_path_lc == gears_path_lc) {
      // Give Gears plugins "background" priority.  See
      // http://b/issue?id=1280317.
      SetProcessBackgrounded();
    }
  }

  return true;
}

void PluginProcessHost::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PluginProcessHost, msg)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_ChannelCreated, OnChannelCreated)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_GetPluginFinderUrl,
                        OnGetPluginFinderUrl)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_PluginMessage, OnPluginMessage)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_GetCookies, OnGetCookies)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PluginProcessHostMsg_ResolveProxy,
                                    OnResolveProxy)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_PluginWindowDestroyed,
                        OnPluginWindowDestroyed)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_DownloadUrl, OnDownloadUrl)
#endif
#if defined(OS_LINUX)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_MapNativeViewId,
                        OnMapNativeViewId)
#endif
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void PluginProcessHost::OnChannelConnected(int32 peer_pid) {
  for (size_t i = 0; i < pending_requests_.size(); ++i) {
    RequestPluginChannel(pending_requests_[i].renderer_message_filter_.get(),
                         pending_requests_[i].mime_type,
                         pending_requests_[i].reply_msg);
  }

  pending_requests_.clear();
}

void PluginProcessHost::OnChannelError() {
  for (size_t i = 0; i < pending_requests_.size(); ++i) {
    ReplyToRenderer(pending_requests_[i].renderer_message_filter_.get(),
                    IPC::ChannelHandle(),
                    FilePath(),
                    pending_requests_[i].reply_msg);
  }

  pending_requests_.clear();
}

void PluginProcessHost::OpenChannelToPlugin(
    ResourceMessageFilter* renderer_message_filter,
    const std::string& mime_type,
    IPC::Message* reply_msg) {
  InstanceCreated();
  if (opening_channel()) {
    // The channel is already in the process of being opened.  Put
    // this "open channel" request into a queue of requests that will
    // be run once the channel is open.
    pending_requests_.push_back(
        ChannelRequest(renderer_message_filter, mime_type, reply_msg));
    return;
  }

  // We already have an open channel, send a request right away to plugin.
  RequestPluginChannel(renderer_message_filter, mime_type, reply_msg);
}

void PluginProcessHost::OnGetCookies(uint32 request_context,
                                     const GURL& url,
                                     std::string* cookies) {
  URLRequestContext* context = CPBrowsingContextManager::Instance()->
        ToURLRequestContext(request_context);
  // TODO(mpcomplete): remove fallback case when Gears support is prevalent.
  if (!context)
    context = Profile::GetDefaultRequestContext();

  // Note: We don't have a first_party_for_cookies check because plugins bypass
  // third-party cookie blocking.
  *cookies = context->cookie_store()->GetCookies(url);
}

void PluginProcessHost::OnResolveProxy(const GURL& url,
                                       IPC::Message* reply_msg) {
  resolve_proxy_msg_helper_.Start(url, reply_msg);
}

void PluginProcessHost::OnResolveProxyCompleted(IPC::Message* reply_msg,
                                                int result,
                                                const std::string& proxy_list) {
  PluginProcessHostMsg_ResolveProxy::WriteReplyParams(
      reply_msg, result, proxy_list);
  Send(reply_msg);
}

void PluginProcessHost::ReplyToRenderer(
    ResourceMessageFilter* renderer_message_filter,
    const IPC::ChannelHandle& channel,
    const FilePath& plugin_path,
    IPC::Message* reply_msg) {
  ViewHostMsg_OpenChannelToPlugin::WriteReplyParams(reply_msg, channel,
                                                    plugin_path);
  renderer_message_filter->Send(reply_msg);
}

URLRequestContext* PluginProcessHost::GetRequestContext(
    uint32 request_id,
    const ViewHostMsg_Resource_Request& request_data) {
  return CPBrowsingContextManager::Instance()->ToURLRequestContext(request_id);
}

void PluginProcessHost::RequestPluginChannel(
    ResourceMessageFilter* renderer_message_filter,
    const std::string& mime_type, IPC::Message* reply_msg) {
  // We can't send any sync messages from the browser because it might lead to
  // a hang.  However this async messages must be answered right away by the
  // plugin process (i.e. unblocks a Send() call like a sync message) otherwise
  // a deadlock can occur if the plugin creation request from the renderer is
  // a result of a sync message by the plugin process.
  PluginProcessMsg_CreateChannel* msg = new PluginProcessMsg_CreateChannel(
      renderer_message_filter->GetProcessId(),
      renderer_message_filter->off_the_record());
  msg->set_unblock(true);
  if (Send(msg)) {
    sent_requests_.push(ChannelRequest(
        renderer_message_filter, mime_type, reply_msg));
  } else {
    ReplyToRenderer(renderer_message_filter, IPC::ChannelHandle(), FilePath(),
                    reply_msg);
  }
}

void PluginProcessHost::OnChannelCreated(
    const IPC::ChannelHandle& channel_handle) {
  const ChannelRequest& request = sent_requests_.front();

  ReplyToRenderer(request.renderer_message_filter_.get(),
                  channel_handle,
                  info_.path,
                  request.reply_msg);
  sent_requests_.pop();
}

void PluginProcessHost::OnGetPluginFinderUrl(std::string* plugin_finder_url) {
  if (!plugin_finder_url) {
    NOTREACHED();
    return;
  }

  // TODO(iyengar)  Add the plumbing to retrieve the default
  // plugin finder URL.
  *plugin_finder_url = kDefaultPluginFinderURL;
}

void PluginProcessHost::OnPluginMessage(
    const std::vector<uint8>& data) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  ChromePluginLib *chrome_plugin = ChromePluginLib::Find(info_.path);
  if (chrome_plugin) {
    void *data_ptr = const_cast<void*>(reinterpret_cast<const void*>(&data[0]));
    uint32 data_len = static_cast<uint32>(data.size());
    chrome_plugin->functions().on_message(data_ptr, data_len);
  }
}
