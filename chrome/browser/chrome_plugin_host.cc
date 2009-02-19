// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_plugin_host.h"

#include <set>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/gfx/png_encoder.h"
#include "base/histogram.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/perftimer.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chrome_plugin_browsing_context.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/html_dialog_contents.h"
#include "chrome/browser/gears_integration.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_counters.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gears_api.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/net/url_request_intercept_job.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "net/base/base64.h"
#include "net/base/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "skia/include/SkBitmap.h"

using base::TimeDelta;

// This class manages the interception of network requests.  It queries the
// plugin on every request, and creates an intercept job if the plugin can
// intercept the request.
// NOTE: All methods must be called on the IO thread.
class PluginRequestInterceptor
    : public PluginHelper, public URLRequest::Interceptor {
 public:
  static URLRequestJob* UninterceptedProtocolHandler(
      URLRequest* request, const std::string& scheme) {
    // This will get called if a plugin failed to intercept a request for a
    // protocol it has registered.  In that case, we return NULL and the request
    // will result in an error.
    return new URLRequestErrorJob(request, net::ERR_FILE_NOT_FOUND);
  }

  explicit PluginRequestInterceptor(ChromePluginLib* plugin)
      : PluginHelper(plugin) {
    URLRequest::RegisterRequestInterceptor(this);
  }

  virtual ~PluginRequestInterceptor() {
    URLRequest::UnregisterRequestInterceptor(this);

    // Unregister our protocols.
    for (HandledProtocolList::iterator it = registered_protocols_.begin();
         it != registered_protocols_.end(); ++it) {
      URLRequest::ProtocolFactory* factory =
          URLRequest::RegisterProtocolFactory(*it, NULL);
      DCHECK(factory == UninterceptedProtocolHandler);
    }
  }

  void RegisterProtocol(const std::string& scheme) {
    DCHECK(CalledOnValidThread());

    std::string lower_scheme = StringToLowerASCII(scheme);
    handled_protocols_.insert(lower_scheme);

    // Only add a protocol factory if the URLRequest doesn't already handle
    // it.  If we fail to intercept, the request will be treated as an error.
    if (!URLRequest::IsHandledProtocol(lower_scheme)) {
      registered_protocols_.insert(lower_scheme);
      URLRequest::RegisterProtocolFactory(lower_scheme,
                                          &UninterceptedProtocolHandler);
    }
  }

  // URLRequest::Interceptor
  virtual URLRequestJob* MaybeIntercept(URLRequest* request) {
    // TODO(darin): This DCHECK fails in the unit tests because our interceptor
    // is being persisted across unit tests.  As a result, each time we get
    // poked on a different thread, but never from more than one thread at a
    // time.  We need a way to have the URLRequestJobManager get reset between
    // unit tests.
    //DCHECK(CalledOnValidThread());

    if (!IsHandledProtocol(request->url().scheme()))
      return NULL;

    CPBrowsingContext context =
        CPBrowsingContextManager::Instance()->Lookup(request->context());
    scoped_ptr<ScopableCPRequest> cprequest(
        new ScopableCPRequest(request->url().spec().c_str(),
                              request->method().c_str(),
                              context));

    PerfTimer timer;
    if (!plugin_->functions().should_intercept_request(cprequest.get())) {
      LogInterceptMissTime(timer.Elapsed());
      return NULL;
    }
    LogInterceptHitTime(timer.Elapsed());
    return new URLRequestInterceptJob(request, plugin_, cprequest.release());
  }

 private:
  bool IsHandledProtocol(const std::string& scheme) {
    return handled_protocols_.find(scheme) != handled_protocols_.end();
  }

  void LogInterceptHitTime(const TimeDelta& time) {
    UMA_HISTOGRAM_TIMES(L"Gears.InterceptHit", time);
  }

  void LogInterceptMissTime(const TimeDelta& time) {
    UMA_HISTOGRAM_TIMES(L"Gears.InterceptMiss", time);
  }

  typedef std::set<std::string> HandledProtocolList;
  HandledProtocolList handled_protocols_;
  HandledProtocolList registered_protocols_;
};

// This class manages a network request made by the plugin, also acting as
// the URLRequest delegate.
// NOTE: All methods must be called on the IO thread.
class PluginRequestHandler : public PluginHelper, public URLRequest::Delegate {
 public:
  static PluginRequestHandler* FromCPRequest(CPRequest* request) {
    return ScopableCPRequest::GetData<PluginRequestHandler*>(request);
  }

  PluginRequestHandler(ChromePluginLib* plugin, ScopableCPRequest* cprequest)
      : PluginHelper(plugin), cprequest_(cprequest), user_buffer_(NULL) {
    cprequest_->data = this;  // see FromCPRequest().

    URLRequestContext* context = CPBrowsingContextManager::Instance()->
        ToURLRequestContext(cprequest_->context);
    // TODO(mpcomplete): remove fallback case when Gears support is prevalent.
    if (!context)
      context = Profile::GetDefaultRequestContext();

    GURL gurl(cprequest_->url);
    request_.reset(new URLRequest(gurl, this));
    request_->set_context(context);
    request_->set_method(cprequest_->method);
    request_->set_load_flags(PluginResponseUtils::CPLoadFlagsToNetFlags(0));
  }

  URLRequest* request() { return request_.get(); }

  // Wraper of URLRequest::Read()
  bool Read(char* dest, int dest_size, int *bytes_read) {
    CHECK(!my_buffer_.get());
    // We'll use our own buffer until the read actually completes.
    user_buffer_ = dest;
    my_buffer_ = new net::IOBuffer(dest_size);

    if (request_->Read(my_buffer_, dest_size, bytes_read)) {
      memcpy(dest, my_buffer_->data(), *bytes_read);
      my_buffer_ = NULL;
      return true;
    }

    if (!request_->status().is_io_pending())
      my_buffer_ = NULL;

    return false;
  }

  // URLRequest::Delegate
  virtual void OnReceivedRedirect(URLRequest* request, const GURL& new_url) {
    plugin_->functions().response_funcs->received_redirect(
        cprequest_.get(), new_url.spec().c_str());
  }

  virtual void OnResponseStarted(URLRequest* request) {
    // TODO(mpcomplete): better error codes
    CPError result =
        request_->status().is_success() ? CPERR_SUCCESS : CPERR_FAILURE;
    plugin_->functions().response_funcs->start_completed(
        cprequest_.get(), result);
  }

  virtual void OnReadCompleted(URLRequest* request, int bytes_read) {
    CHECK(my_buffer_.get());
    CHECK(user_buffer_);
    if (bytes_read > 0) {
      memcpy(user_buffer_, my_buffer_->data(), bytes_read);
    } else if (bytes_read < 0) {
      // TODO(mpcomplete): better error codes
      bytes_read = CPERR_FAILURE;
    }
    my_buffer_ = NULL;
    plugin_->functions().response_funcs->read_completed(
        cprequest_.get(), bytes_read);
  }

 private:
  scoped_ptr<ScopableCPRequest> cprequest_;
  scoped_ptr<URLRequest> request_;
  scoped_refptr<net::IOBuffer> my_buffer_;
  char* user_buffer_;
};

// This class manages plugins that want to handle UI commands.  Right now, we
// only allow 1 plugin to do this, so there's only ever 1 instance of this
// class at once.
// NOTE: All methods must be called on the IO thread.
class PluginCommandHandler : public PluginHelper {
 public:
  static void HandleCommand(int command,
                            CPCommandInterface* data,
                            int32 context_as_int32) {
    CPBrowsingContext context =
        static_cast<CPBrowsingContext>(context_as_int32);
    // Ensure plugins are loaded before we try to talk to it.  This will be a
    // noop if plugins are loaded already.
    ChromePluginLib::LoadChromePlugins(GetCPBrowserFuncsForBrowser());

    DCHECK(ChromePluginLib::IsPluginThread());
    CPError rv = CPERR_INVALID_VERSION;
    if (instance_ && instance_->plugin_->functions().handle_command) {
      rv = instance_->plugin_->functions().handle_command(
          context, command, data ? data->GetData() : NULL);
    }
    if (data)
      data->OnCommandInvoked(rv);
  }

  static void RegisterPlugin(ChromePluginLib* plugin) {
    DCHECK(ChromePluginLib::IsPluginThread());
    // TODO(mpcomplete): We only expect to have Gears register a command handler
    // at the moment.  We should either add support for other plugins to do
    // this, or verify that the plugin is Gears.
    DCHECK(!instance_) <<
        "We only support a single plugin handling UI commands.";
    if (instance_)
      return;
    // Deleted in response to a notification in PluginHelper.
    new PluginCommandHandler(plugin);
  }

 private:
  explicit PluginCommandHandler(ChromePluginLib* plugin)
      : PluginHelper(plugin) {
    DCHECK(instance_ == NULL);
    instance_ = this;
  }

  virtual ~PluginCommandHandler() {
    instance_ = NULL;
  }

  static PluginCommandHandler* instance_;
};

PluginCommandHandler* PluginCommandHandler::instance_ = NULL;

// This class acts as a helper to display the HTML dialog.  It is created
// on demand on the plugin thread, and proxies calls to and from the UI thread
// to display the UI.
class ModelessHtmlDialogDelegate : public HtmlDialogContentsDelegate {
 public:
  ModelessHtmlDialogDelegate(const GURL& url,
                             int width, int height,
                             const std::string& json_arguments,
                             void* plugin_context,
                             ChromePluginLib* plugin,
                             MessageLoop* main_message_loop,
                             HWND parent_hwnd)
      : plugin_(plugin),
        plugin_context_(plugin_context),
        main_message_loop_(main_message_loop),
        io_message_loop_(MessageLoop::current()),
        parent_hwnd_(parent_hwnd) {
    DCHECK(ChromePluginLib::IsPluginThread());
    params_.url = url;
    params_.height = height;
    params_.width = width;
    params_.json_input = json_arguments;

    main_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &ModelessHtmlDialogDelegate::Show));
  }
  ~ModelessHtmlDialogDelegate() {
    DCHECK(ChromePluginLib::IsPluginThread());
  }

  // The following public methods are called from the UI thread.

  // views::WindowDelegate implementation:
  virtual bool IsModal() const { return false; }
  virtual std::wstring GetWindowTitle() const { return L"Google Gears"; }

  // HtmlDialogContentsDelegate implementation:
  virtual GURL GetDialogContentURL() const { return params_.url; }
  virtual void GetDialogSize(CSize* size) const {
    size->cx = params_.width;
    size->cy = params_.height;
  }
  virtual std::string GetDialogArgs() const { return params_.json_input; }
  virtual void OnDialogClosed(const std::string& json_retval) {
    io_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &ModelessHtmlDialogDelegate::ReportResults, json_retval));
  }

 private:
  // Actually shows the dialog on the UI thread.
  void Show() {
    DCHECK(MessageLoop::current() == main_message_loop_);
    Browser* browser = BrowserList::GetLastActive();
    browser->ShowHtmlDialog(this, parent_hwnd_);
  }

  // Gives the JSON result string back to the plugin.
  void ReportResults(const std::string& json_retval) {
    DCHECK(MessageLoop::current() == io_message_loop_);
    // The plugin may have unloaded before it was time to report the results.
    if (plugin_->is_loaded())
      plugin_->functions().html_dialog_closed(plugin_context_,
                                              json_retval.c_str());
    delete this;
  }

  // The parameters needed to display a modal HTML dialog.
  HtmlDialogContents::HtmlDialogParams params_;

  // Message loops for sending messages between UI and IO threads.
  MessageLoop* main_message_loop_;
  MessageLoop* io_message_loop_;

  // The plugin object that requested the dialog.  This can only be accessed on
  // the IO thread.
  scoped_refptr<ChromePluginLib> plugin_;

  // The plugin's context argument to CPB_ShowHtmlDialog.
  void* plugin_context_;

  // The window this dialog box should be parented to, or NULL for the last
  // active browser window.
  HWND parent_hwnd_;

  DISALLOW_EVIL_CONSTRUCTORS(ModelessHtmlDialogDelegate);
};

// Allows InvokeLater without adding refcounting.  The object is only deleted
// when its last InvokeLater is run anyway.
void RunnableMethodTraits<ModelessHtmlDialogDelegate>::RetainCallee(
    ModelessHtmlDialogDelegate* remover) {
}
void RunnableMethodTraits<ModelessHtmlDialogDelegate>::ReleaseCallee(
    ModelessHtmlDialogDelegate* remover) {
}

namespace {

//
// Generic functions
//

void STDCALL CPB_SetKeepProcessAlive(CPID id, CPBool keep_alive) {
  // This is a no-op in the main browser process
}

CPError STDCALL CPB_GetCookies(CPID id, CPBrowsingContext bcontext,
                               const char* url, char** cookies) {
  CHECK(ChromePluginLib::IsPluginThread());
  URLRequestContext* context = CPBrowsingContextManager::Instance()->
      ToURLRequestContext(bcontext);
  // TODO(mpcomplete): remove fallback case when Gears support is prevalent.
  if (!context) {
    context = Profile::GetDefaultRequestContext();
    if (!context)
      return CPERR_FAILURE;
  }
  std::string cookies_str = context->cookie_store()->GetCookies(GURL(url));
  *cookies = CPB_StringDup(CPB_Alloc, cookies_str);
  return CPERR_SUCCESS;
}

CPError STDCALL CPB_ShowHtmlDialogModal(
    CPID id, CPBrowsingContext context, const char* url, int width, int height,
    const char* json_arguments, char** json_retval) {
  // Should not be called in browser process.
  return CPERR_FAILURE;
}

CPError STDCALL CPB_ShowHtmlDialog(
    CPID id, CPBrowsingContext context, const char* url, int width, int height,
    const char* json_arguments, void* plugin_context) {
  CHECK(ChromePluginLib::IsPluginThread());
  ChromePluginLib* plugin = ChromePluginLib::FromCPID(id);
  CHECK(plugin);

#if defined(OS_WIN)
  HWND parent_hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(context));
  PluginService* service = PluginService::GetInstance();
  if (!service)
    return CPERR_FAILURE;
  MessageLoop* main_message_loop = service->main_message_loop();
  ModelessHtmlDialogDelegate* delegate =
      new ModelessHtmlDialogDelegate(GURL(url), width, height, json_arguments,
                                     plugin_context, plugin, main_message_loop,
                                     parent_hwnd);
#else
  // TODO(port): port ModelessHtmlDialogDelegate
  NOTIMPLEMENTED();
#endif

  return CPERR_SUCCESS;
}

CPError STDCALL CPB_GetCommandLineArguments(
    CPID id, CPBrowsingContext context, const char* url, char** arguments) {
  CHECK(ChromePluginLib::IsPluginThread());
  std::string arguments_str;
  CPError rv = CPB_GetCommandLineArgumentsCommon(url, &arguments_str);
  if (rv == CPERR_SUCCESS)
    *arguments = CPB_StringDup(CPB_Alloc, arguments_str);
  return rv;
}

CPBrowsingContext STDCALL CPB_GetBrowsingContextFromNPP(struct _NPP* npp) {
  CHECK(ChromePluginLib::IsPluginThread());
  NOTREACHED() << "NPP does not exist in the browser process.";
  return 0;
}

int STDCALL CPB_GetBrowsingContextInfo(
    CPID id, CPBrowsingContext context, CPBrowsingContextInfoType type,
    void* buf, uint32 buf_size) {
  CHECK(ChromePluginLib::IsPluginThread());
  switch (type) {
  case CPBROWSINGCONTEXT_DATA_DIR_PTR: {
    if (buf_size < sizeof(char*))
      return sizeof(char*);

    // TODO(mpcomplete): http://b/1143021 - When we support multiple profiles,
    // fetch the data dir from the context.
    PluginService* service = PluginService::GetInstance();
    if (!service)
      return CPERR_FAILURE;
    std::wstring wretval = service->GetChromePluginDataDir().ToWStringHack();
    file_util::AppendToPath(&wretval, chrome::kChromePluginDataDirname);
    *static_cast<char**>(buf) = CPB_StringDup(CPB_Alloc, WideToUTF8(wretval));
    return CPERR_SUCCESS;
    }
  case CPBROWSINGCONTEXT_UI_LOCALE_PTR: {
    if (buf_size < sizeof(char*))
      return sizeof(char*);

    PluginService* service = PluginService::GetInstance();
    if (!service)
      return CPERR_FAILURE;
    const std::wstring& wretval = service->GetUILocale();
    *static_cast<char**>(buf) = CPB_StringDup(CPB_Alloc, WideToUTF8(wretval));
    return CPERR_SUCCESS;
    }
  }

  return CPERR_FAILURE;
}

CPError STDCALL CPB_AddUICommand(CPID id, int command) {
  CHECK(ChromePluginLib::IsPluginThread());
  ChromePluginLib* plugin = ChromePluginLib::FromCPID(id);
  CHECK(plugin);

  PluginCommandHandler::RegisterPlugin(plugin);
  return CPERR_SUCCESS;
}

static void NotifyGearsShortcutsChanged() {
  DCHECK(MessageLoop::current() ==
         PluginService::GetInstance()->main_message_loop());

  // TODO(michaeln): source should be the original profile, fix this
  // when gears provides the correct browser context, and when we
  // can relate that to an actual profile.
  NotificationService::current()->Notify(
      NotificationType::WEB_APP_INSTALL_CHANGED,
      Source<Profile>(NULL),
      NotificationService::NoDetails());
}

CPError STDCALL CPB_HandleCommand(
    CPID id, CPBrowsingContext context, int command, void *data) {
  CHECK(ChromePluginLib::IsPluginThread());
  ChromePluginLib* plugin = ChromePluginLib::FromCPID(id);
  CHECK(plugin);

  if (command == GEARSBROWSERCOMMAND_CREATE_SHORTCUT_DONE) {
    GearsCreateShortcutResult* result =
        static_cast<GearsCreateShortcutResult*>(data);
    CHECK(result);

    GearsCreateShortcutData* shortcut_data =
        static_cast<GearsCreateShortcutData*>(result->shortcut);
    shortcut_data->command_interface->OnCommandResponse(result->result);
  } else if (command == GEARSBROWSERCOMMAND_NOTIFY_SHORTCUTS_CHANGED) {
    PluginService* service = PluginService::GetInstance();
    if (!service)
      return CPERR_FAILURE;
    service->main_message_loop()->PostTask(FROM_HERE,
        NewRunnableFunction(NotifyGearsShortcutsChanged));
    return CPERR_SUCCESS;
  }
  return CPERR_FAILURE;
}

//
// Functions related to network interception
//

void STDCALL CPB_EnableRequestIntercept(CPID id, const char** schemes,
                                        uint32 num_schemes) {
  CHECK(ChromePluginLib::IsPluginThread());
  ChromePluginLib* plugin = ChromePluginLib::FromCPID(id);
  CHECK(plugin);

  if (schemes && num_schemes > 0) {
    PluginRequestInterceptor* interceptor =
        new PluginRequestInterceptor(plugin);
    for (uint32 i = 0; i < num_schemes; ++i)
      interceptor->RegisterProtocol(schemes[i]);
  } else {
    PluginRequestInterceptor::DestroyAllHelpersForPlugin(plugin);
  }
}

void STDCALL CPRR_ReceivedRedirect(CPRequest* request, const char* new_url) {
}

void STDCALL CPRR_StartCompleted(CPRequest* request, CPError result) {
  CHECK(ChromePluginLib::IsPluginThread());
  URLRequestInterceptJob* job = URLRequestInterceptJob::FromCPRequest(request);
  CHECK(job);
  job->OnStartCompleted(result);
}

void STDCALL CPRR_ReadCompleted(CPRequest* request, int bytes_read) {
  CHECK(ChromePluginLib::IsPluginThread());
  URLRequestInterceptJob* job = URLRequestInterceptJob::FromCPRequest(request);
  CHECK(job);
  job->OnReadCompleted(bytes_read);
}

void STDCALL CPRR_UploadProgress(CPRequest* request, uint64 pos, uint64 size) {
  // Does not apply, plugins do not yet intercept uploads
}

//
// Functions related to serving network requests to the plugin
//

CPError STDCALL CPB_CreateRequest(CPID id, CPBrowsingContext context,
                                  const char* method, const char* url,
                                  CPRequest** request) {
  CHECK(ChromePluginLib::IsPluginThread());
  ChromePluginLib* plugin = ChromePluginLib::FromCPID(id);
  CHECK(plugin);

  ScopableCPRequest* cprequest = new ScopableCPRequest(url, method, context);
  PluginRequestHandler* handler = new PluginRequestHandler(plugin, cprequest);

  *request = cprequest;
  return CPERR_SUCCESS;
}

CPError STDCALL CPR_StartRequest(CPRequest* request) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandler* handler = PluginRequestHandler::FromCPRequest(request);
  CHECK(handler);
  handler->request()->Start();
  return CPERR_IO_PENDING;
}

void STDCALL CPR_EndRequest(CPRequest* request, CPError reason) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandler* handler = PluginRequestHandler::FromCPRequest(request);
  delete handler;
}

void STDCALL CPR_SetExtraRequestHeaders(CPRequest* request,
                                        const char* headers) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandler* handler = PluginRequestHandler::FromCPRequest(request);
  CHECK(handler);
  handler->request()->SetExtraRequestHeaders(headers);
}

void STDCALL CPR_SetRequestLoadFlags(CPRequest* request, uint32 flags) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandler* handler = PluginRequestHandler::FromCPRequest(request);
  CHECK(handler);
  uint32 net_flags = PluginResponseUtils::CPLoadFlagsToNetFlags(flags);
  handler->request()->set_load_flags(net_flags);
}

void STDCALL CPR_AppendDataToUpload(CPRequest* request, const char* bytes,
                                    int bytes_len) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandler* handler = PluginRequestHandler::FromCPRequest(request);
  CHECK(handler);
  handler->request()->AppendBytesToUpload(bytes, bytes_len);
}

CPError STDCALL CPR_AppendFileToUpload(CPRequest* request, const char* filepath,
                                       uint64 offset, uint64 length) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandler* handler = PluginRequestHandler::FromCPRequest(request);
  CHECK(handler);

  if (!length) length = kuint64max;
  std::wstring wfilepath(UTF8ToWide(filepath));
  handler->request()->AppendFileRangeToUpload(wfilepath, offset, length);
  return CPERR_SUCCESS;
}

int STDCALL CPR_GetResponseInfo(CPRequest* request, CPResponseInfoType type,
                                void* buf, uint32 buf_size) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandler* handler = PluginRequestHandler::FromCPRequest(request);
  CHECK(handler);
  return PluginResponseUtils::GetResponseInfo(
      handler->request()->response_headers(), type, buf, buf_size);
}

int STDCALL CPR_Read(CPRequest* request, void* buf, uint32 buf_size) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandler* handler = PluginRequestHandler::FromCPRequest(request);
  CHECK(handler);

  int bytes_read;
  if (handler->Read(static_cast<char*>(buf), buf_size, &bytes_read))
    return bytes_read;  // 0 == CPERR_SUCESS

  if (handler->request()->status().is_io_pending())
    return CPERR_IO_PENDING;

  return CPERR_FAILURE;
}

CPBool STDCALL CPB_IsPluginProcessRunning(CPID id) {
  CHECK(ChromePluginLib::IsPluginThread());
  ChromePluginLib* plugin = ChromePluginLib::FromCPID(id);
  CHECK(plugin);
  PluginService* service = PluginService::GetInstance();
  if (!service)
    return false;
  PluginProcessHost *host = service->FindPluginProcess(plugin->filename());
  return host ? true : false;
}

CPProcessType STDCALL CPB_GetProcessType(CPID id) {
  CHECK(ChromePluginLib::IsPluginThread());
  return CP_PROCESS_BROWSER;
}

CPError STDCALL CPB_SendMessage(CPID id, const void *data, uint32 data_len) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGearsInRenderer)) {
    ChromePluginLib* plugin = ChromePluginLib::FromCPID(id);
    CHECK(plugin);

    const unsigned char* data_ptr = static_cast<const unsigned char*>(data);
    std::vector<uint8> v(data_ptr, data_ptr + data_len);
    for (RenderProcessHost::iterator it = RenderProcessHost::begin();
      it != RenderProcessHost::end(); ++it) {
        it->second->Send(new ViewMsg_PluginMessage(plugin->filename(), v));
    }

    return CPERR_SUCCESS;
  } else {
    CHECK(ChromePluginLib::IsPluginThread());
    ChromePluginLib* plugin = ChromePluginLib::FromCPID(id);
    CHECK(plugin);

    PluginService* service = PluginService::GetInstance();
    if (!service)
    return CPERR_FAILURE;
    PluginProcessHost *host =
    service->FindOrStartPluginProcess(plugin->filename(), std::string());
    if (!host)
    return CPERR_FAILURE;

    const unsigned char* data_ptr = static_cast<const unsigned char*>(data);
    std::vector<uint8> v(data_ptr, data_ptr + data_len);
    if (!host->Send(new PluginProcessMsg_PluginMessage(v)))
      return CPERR_FAILURE;

    return CPERR_SUCCESS;
  }
}

CPError STDCALL CPB_SendSyncMessage(CPID id, const void *data, uint32 data_len,
                                    void **retval, uint32 *retval_len) {
  NOTREACHED() << "Sync messages should not be sent from the browser process.";

  return CPERR_FAILURE;
}

CPError STDCALL CPB_PluginThreadAsyncCall(CPID id,
                                          void (*func)(void *),
                                          void *user_data) {
  MessageLoop *message_loop = ChromeThread::GetMessageLoop(ChromeThread::IO);
  if (!message_loop) {
    return CPERR_FAILURE;
  }
  message_loop->PostTask(FROM_HERE, NewRunnableFunction(func, user_data));

  return CPERR_SUCCESS;
}

CPError STDCALL CPB_OpenFileDialog(CPID id,
                                   CPBrowsingContext context,
                                   bool multiple_files,
                                   const char *title,
                                   const char *filter,
                                   void *user_data) {
  NOTREACHED() <<
    "Open file dialog should only be called from the renderer process.";

  return CPERR_FAILURE;
}

}

CPBrowserFuncs* GetCPBrowserFuncsForBrowser() {
  static CPBrowserFuncs browser_funcs;
  static CPRequestFuncs request_funcs;
  static CPResponseFuncs response_funcs;
  static bool initialized = false;
  if (!initialized) {
    initialized = true;

    browser_funcs.size = sizeof(browser_funcs);
    browser_funcs.version = CP_VERSION;
    browser_funcs.enable_request_intercept = CPB_EnableRequestIntercept;
    browser_funcs.create_request = CPB_CreateRequest;
    browser_funcs.get_cookies = CPB_GetCookies;
    browser_funcs.alloc = CPB_Alloc;
    browser_funcs.free = CPB_Free;
    browser_funcs.set_keep_process_alive = CPB_SetKeepProcessAlive;
    browser_funcs.show_html_dialog_modal = CPB_ShowHtmlDialogModal;
    browser_funcs.show_html_dialog = CPB_ShowHtmlDialog;
    browser_funcs.is_plugin_process_running = CPB_IsPluginProcessRunning;
    browser_funcs.get_process_type = CPB_GetProcessType;
    browser_funcs.send_message = CPB_SendMessage;
    browser_funcs.get_browsing_context_from_npp = CPB_GetBrowsingContextFromNPP;
    browser_funcs.get_browsing_context_info = CPB_GetBrowsingContextInfo;
    browser_funcs.get_command_line_arguments = CPB_GetCommandLineArguments;
    browser_funcs.add_ui_command = CPB_AddUICommand;
    browser_funcs.handle_command = CPB_HandleCommand;

    browser_funcs.request_funcs = &request_funcs;
    browser_funcs.response_funcs = &response_funcs;
    browser_funcs.send_sync_message = CPB_SendSyncMessage;
    browser_funcs.plugin_thread_async_call = CPB_PluginThreadAsyncCall;
    browser_funcs.open_file_dialog = CPB_OpenFileDialog;

    request_funcs.size = sizeof(request_funcs);
    request_funcs.start_request = CPR_StartRequest;
    request_funcs.end_request = CPR_EndRequest;
    request_funcs.set_extra_request_headers = CPR_SetExtraRequestHeaders;
    request_funcs.set_request_load_flags = CPR_SetRequestLoadFlags;
    request_funcs.append_data_to_upload = CPR_AppendDataToUpload;
    request_funcs.get_response_info = CPR_GetResponseInfo;
    request_funcs.read = CPR_Read;
    request_funcs.append_file_to_upload = CPR_AppendFileToUpload;

    response_funcs.size = sizeof(response_funcs);
    response_funcs.received_redirect = CPRR_ReceivedRedirect;
    response_funcs.start_completed = CPRR_StartCompleted;
    response_funcs.read_completed = CPRR_ReadCompleted;
    response_funcs.upload_progress = CPRR_UploadProgress;
  }

  return &browser_funcs;
}

void CPHandleCommand(int command, CPCommandInterface* data,
                     CPBrowsingContext context) {
  // Sadly if we try and pass context through, we seem to break cl's little
  // brain trying to compile the Tuple3 ctor. This cast works.
  int32 context_as_int32 = static_cast<int32>(context);
  // Plugins can only be accessed on the IO thread.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableFunction(PluginCommandHandler::HandleCommand,
                          command, data, context_as_int32));
}
