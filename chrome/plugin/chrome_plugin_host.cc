// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/chrome_plugin_host.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/plugin/plugin_process.h"
#include "chrome/plugin/plugin_thread.h"
#include "chrome/plugin/webplugin_proxy.h"
#include "net/base/data_url.h"
#include "net/base/upload_data.h"
#include "net/http/http_response_headers.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/resource_type.h"
#include "webkit/glue/webkit_glue.h"

namespace {

using webkit_glue::ResourceLoaderBridge;

// This class manages a network request made by the plugin, handling the
// data as it comes in from the ResourceLoaderBridge and is requested by the
// plugin.
// NOTE: All methods must be called on the Plugin thread.
class PluginRequestHandlerProxy
    : public PluginHelper, public ResourceLoaderBridge::Peer {
 public:
  static PluginRequestHandlerProxy* FromCPRequest(CPRequest* request) {
    return ScopableCPRequest::GetData<PluginRequestHandlerProxy*>(request);
  }

  PluginRequestHandlerProxy(ChromePluginLib* plugin,
                            ScopableCPRequest* cprequest)
      : PluginHelper(plugin), cprequest_(cprequest), response_data_offset_(0),
        completed_(false), sync_(false), read_buffer_(NULL) {
    load_flags_ = PluginResponseUtils::CPLoadFlagsToNetFlags(0);
    cprequest_->data = this;  // see FromCPRequest().
  }

  ~PluginRequestHandlerProxy() {
    if (bridge_.get() && !completed_) {
      bridge_->Cancel();
    }
  }

  // ResourceLoaderBridge::Peer
  virtual void OnUploadProgress(uint64 position, uint64 size) {
    CPRR_UploadProgressFunc upload_progress =
        plugin_->functions().response_funcs->upload_progress;
    if (upload_progress)
      upload_progress(cprequest_.get(), position, size);
  }

  virtual void OnReceivedRedirect(const GURL& new_url) {
    plugin_->functions().response_funcs->received_redirect(
        cprequest_.get(), new_url.spec().c_str());
  }

  virtual void OnReceivedResponse(
      const ResourceLoaderBridge::ResponseInfo& info,
      bool content_filtered) {
    response_headers_ = info.headers;
    plugin_->functions().response_funcs->start_completed(
        cprequest_.get(), CPERR_SUCCESS);
  }

  virtual void OnReceivedData(const char* data, int len) {
    response_data_.append(data, len);
    if (read_buffer_) {
      // If we had an asynchronous operation pending, read into that buffer
      // and inform the plugin.
      int rv = Read(read_buffer_, read_buffer_size_);
      DCHECK(rv != CPERR_IO_PENDING);
      read_buffer_ = NULL;
      plugin_->functions().response_funcs->read_completed(
          cprequest_.get(), rv);
    }
  }

  virtual void OnCompletedRequest(const URLRequestStatus& status,
                                  const std::string& security_info) {
    completed_ = true;

    if (!status.is_success()) {
      // TODO(mpcomplete): better error codes
      // Inform the plugin, calling the right function depending on whether
      // we got the start_completed event or not.
      if (response_headers_) {
        plugin_->functions().response_funcs->start_completed(
            cprequest_.get(), CPERR_FAILURE);
      } else {
        plugin_->functions().response_funcs->read_completed(
            cprequest_.get(), CPERR_FAILURE);
      }
    } else if (read_buffer_) {
      // The plugin was waiting for more data.  Inform him we're done.
      read_buffer_ = NULL;
      plugin_->functions().response_funcs->read_completed(
          cprequest_.get(), CPERR_SUCCESS);
    }
  }

  virtual std::string GetURLForDebugging() {
    return cprequest_->url;
  }

  void set_extra_headers(const std::string& headers) {
    extra_headers_ = headers;
  }
  void set_load_flags(uint32 flags) {
    load_flags_ = flags;
  }
  void set_sync(bool sync) {
    sync_ = sync;
  }
  void AppendDataToUpload(const char* bytes, int bytes_len) {
    upload_content_.push_back(net::UploadData::Element());
    upload_content_.back().SetToBytes(bytes, bytes_len);
  }

  void AppendFileToUpload(const std::wstring &filepath) {
    AppendFileRangeToUpload(filepath, 0, kuint64max);
  }

  void AppendFileRangeToUpload(const std::wstring &filepath,
                               uint64 offset, uint64 length) {
    upload_content_.push_back(net::UploadData::Element());
    upload_content_.back().SetToFilePathRange(filepath, offset, length);
  }

  CPError Start() {
    bridge_.reset(
        PluginThread::current()->resource_dispatcher()->CreateBridge(
            cprequest_->method,
            GURL(cprequest_->url),
            GURL(cprequest_->url),  // TODO(jackson): policy url?
            GURL(),  // TODO(mpcomplete): referrer?
            extra_headers_,
            load_flags_,
            GetCurrentProcessId(),
            ResourceType::OBJECT,
            false,  // TODO (jcampan): mixed-content?
            cprequest_->context,
            MSG_ROUTING_CONTROL));
    if (!bridge_.get())
      return CPERR_FAILURE;

    for (size_t i = 0; i < upload_content_.size(); ++i) {
      switch (upload_content_[i].type()) {
        case net::UploadData::TYPE_BYTES: {
          const std::vector<char>& bytes = upload_content_[i].bytes();
          bridge_->AppendDataToUpload(&bytes[0],
                                      static_cast<int>(bytes.size()));
          break;
        }
        case net::UploadData::TYPE_FILE: {
          bridge_->AppendFileRangeToUpload(
              upload_content_[i].file_path(),
              upload_content_[i].file_range_offset(),
              upload_content_[i].file_range_length());
          break;
        }
        default: {
          NOTREACHED() << "Unknown UploadData::Element type";
        }
      }
    }

    if (sync_) {
      ResourceLoaderBridge::SyncLoadResponse response;
      bridge_->SyncLoad(&response);
      response_headers_ = response.headers;
      response_data_ = response.data;
      completed_ = true;
      return response.status.is_success() ? CPERR_SUCCESS : CPERR_FAILURE;
    } else {
      if (!bridge_->Start(this)) {
        bridge_.reset();
        return CPERR_FAILURE;
      }
      return CPERR_IO_PENDING;
    }
  }

  int GetResponseInfo(CPResponseInfoType type, void* buf, uint32 buf_size) {
    return PluginResponseUtils::GetResponseInfo(
        response_headers_, type, buf, buf_size);
  }

  int Read(void* buf, uint32 buf_size) {
    uint32 avail =
        static_cast<uint32>(response_data_.size()) - response_data_offset_;
    uint32 count = buf_size;
    if (count > avail)
      count = avail;

    int rv = CPERR_FAILURE;
    if (count) {
      // Data is ready now.
      memcpy(buf, &response_data_[0] + response_data_offset_, count);
      response_data_offset_ += count;
    } else if (!completed_) {
      read_buffer_ = buf;
      read_buffer_size_ = buf_size;
      DCHECK(!sync_);
      return CPERR_IO_PENDING;
    }

    if (response_data_.size() == response_data_offset_) {
      // Simple optimization for large requests.  Generally the consumer will
      // read the data faster than it comes in, so we can clear our buffer
      // any time it has all been read.
      response_data_.clear();
      response_data_offset_ = 0;
    }

    read_buffer_ = NULL;
    return count;
  }

 private:
  scoped_ptr<ScopableCPRequest> cprequest_;
  scoped_ptr<ResourceLoaderBridge> bridge_;
  std::vector<net::UploadData::Element> upload_content_;
  std::string extra_headers_;
  uint32 load_flags_;
  bool sync_;

  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  std::string response_data_;
  int response_data_offset_;
  bool completed_;
  void* read_buffer_;
  uint32 read_buffer_size_;
};

//
// Generic functions
//

void STDCALL CPB_SetKeepProcessAlive(CPID id, CPBool keep_alive) {
  CHECK(ChromePluginLib::IsPluginThread());
  static bool g_keep_process_alive = false;
  bool desired_value = keep_alive ? true : false;  // smash to bool
  if (desired_value != g_keep_process_alive) {
    g_keep_process_alive = desired_value;
    if (g_keep_process_alive)
      PluginProcess::current()->AddRefProcess();
    else
      PluginProcess::current()->ReleaseProcess();
  }
}

CPError STDCALL CPB_GetCookies(CPID id, CPBrowsingContext context,
                               const char* url, char** cookies) {
  CHECK(ChromePluginLib::IsPluginThread());
  std::string cookies_str;

  WebPluginProxy* webplugin = WebPluginProxy::FromCPBrowsingContext(context);
  // There are two contexts in which we can be asked for cookies:
  // 1. From a script context.  webplugin will be non-NULL.
  // 2. From a global browser context (think: Gears UpdateTask).  webplugin will
  //    be NULL and context will (loosely) represent a browser Profile.
  // In case 1, we *must* route through the renderer process, otherwise we race
  // with renderer script that may have set cookies.  In case 2, we are running
  // out-of-band with script, so we don't need to stay in sync with any
  // particular renderer.
  // See http://b/issue?id=1487502.
  if (webplugin) {
    cookies_str = webplugin->GetCookies(GURL(url), GURL(url));
  } else {
    PluginThread::current()->Send(
        new PluginProcessHostMsg_GetCookies(context, GURL(url), &cookies_str));
  }

  *cookies = CPB_StringDup(CPB_Alloc, cookies_str);
  return CPERR_SUCCESS;
}

CPError STDCALL CPB_ShowHtmlDialogModal(
    CPID id, CPBrowsingContext context, const char* url, int width, int height,
    const char* json_arguments, char** json_retval) {
  CHECK(ChromePluginLib::IsPluginThread());

  WebPluginProxy* webplugin = WebPluginProxy::FromCPBrowsingContext(context);
  if (!webplugin)
    return CPERR_INVALID_PARAMETER;

  std::string retval_str;
  webplugin->ShowModalHTMLDialog(
      GURL(url), width, height, json_arguments, &retval_str);
  *json_retval = CPB_StringDup(CPB_Alloc, retval_str);
  return CPERR_SUCCESS;
}

CPError STDCALL CPB_ShowHtmlDialog(
    CPID id, CPBrowsingContext context, const char* url, int width, int height,
    const char* json_arguments, void* plugin_context) {
  // TODO(mpcomplete): support non-modal dialogs.
  return CPERR_FAILURE;
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

CPBrowsingContext STDCALL CPB_GetBrowsingContextFromNPP(NPP npp) {
  if (!npp)
    return CPERR_INVALID_PARAMETER;

  NPAPI::PluginInstance* instance =
      static_cast<NPAPI::PluginInstance *>(npp->ndata);
  WebPluginProxy* webplugin =
      static_cast<WebPluginProxy*>(instance->webplugin());

  return webplugin->GetCPBrowsingContext();
}

int STDCALL CPB_GetBrowsingContextInfo(
    CPID id, CPBrowsingContext context, CPBrowsingContextInfoType type,
    void* buf, uint32 buf_size) {
  CHECK(ChromePluginLib::IsPluginThread());

  switch (type) {
  case CPBROWSINGCONTEXT_DATA_DIR_PTR: {
    if (buf_size < sizeof(char*))
      return sizeof(char*);

    std::wstring wretval = CommandLine::ForCurrentProcess()->
        GetSwitchValue(switches::kPluginDataDir);
    DCHECK(!wretval.empty());
    file_util::AppendToPath(&wretval, chrome::kChromePluginDataDirname);
    *static_cast<char**>(buf) = CPB_StringDup(CPB_Alloc, WideToUTF8(wretval));
    return CPERR_SUCCESS;
    }
  case CPBROWSINGCONTEXT_UI_LOCALE_PTR: {
    if (buf_size < sizeof(char*))
      return sizeof(char*);

    std::wstring wretval = webkit_glue::GetWebKitLocale();
    *static_cast<char**>(buf) = CPB_StringDup(CPB_Alloc, WideToUTF8(wretval));
    return CPERR_SUCCESS;
    }
  }

  return CPERR_FAILURE;
}

CPError STDCALL CPB_AddUICommand(CPID id, int command) {
  // Not implemented in the plugin process
  return CPERR_FAILURE;
}

CPError STDCALL CPB_HandleCommand(
    CPID id, CPBrowsingContext context, int command, void *data) {
  // Not implemented in the plugin process
  return CPERR_FAILURE;
}

//
// Functions related to network interception
//

void STDCALL CPB_EnableRequestIntercept(
    CPID id, const char** schemes, uint32 num_schemes) {
  // We ignore requests by the plugin to intercept from this process.  That's
  // handled in the browser process.
}

void STDCALL CPRR_ReceivedRedirect(CPRequest* request, const char* new_url) {
  NOTREACHED() << "Network interception should not happen in plugin process.";
}

void STDCALL CPRR_StartCompleted(CPRequest* request, CPError result) {
  NOTREACHED() << "Network interception should not happen in plugin process.";
}

void STDCALL CPRR_ReadCompleted(CPRequest* request, int bytes_read) {
  NOTREACHED() << "Network interception should not happen in plugin process.";
}

void STDCALL CPRR_UploadProgress(CPRequest* request, uint64 pos, uint64 size) {
  NOTREACHED() << "Network interception should not happen in plugin process.";
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
  PluginRequestHandlerProxy* handler =
      new PluginRequestHandlerProxy(plugin, cprequest);

  *request = cprequest;
  return CPERR_SUCCESS;
}

CPError STDCALL CPR_StartRequest(CPRequest* request) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandlerProxy* handler =
      PluginRequestHandlerProxy::FromCPRequest(request);
  CHECK(handler);
  return handler->Start();
}

void STDCALL CPR_EndRequest(CPRequest* request, CPError reason) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandlerProxy* handler =
      PluginRequestHandlerProxy::FromCPRequest(request);
  delete handler;
}

void STDCALL CPR_SetExtraRequestHeaders(CPRequest* request,
                                        const char* headers) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandlerProxy* handler =
      PluginRequestHandlerProxy::FromCPRequest(request);
  CHECK(handler);
  handler->set_extra_headers(headers);
}

void STDCALL CPR_SetRequestLoadFlags(CPRequest* request, uint32 flags) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandlerProxy* handler =
      PluginRequestHandlerProxy::FromCPRequest(request);
  CHECK(handler);

  if (flags & CPREQUESTLOAD_SYNCHRONOUS) {
    handler->set_sync(true);
  }

  uint32 net_flags = PluginResponseUtils::CPLoadFlagsToNetFlags(flags);
  handler->set_load_flags(net_flags);
}

void STDCALL CPR_AppendDataToUpload(CPRequest* request, const char* bytes,
                                    int bytes_len) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandlerProxy* handler =
      PluginRequestHandlerProxy::FromCPRequest(request);
  CHECK(handler);
  handler->AppendDataToUpload(bytes, bytes_len);
}

CPError STDCALL CPR_AppendFileToUpload(CPRequest* request, const char* filepath,
                                       uint64 offset, uint64 length) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandlerProxy* handler =
      PluginRequestHandlerProxy::FromCPRequest(request);
  CHECK(handler);

  if (!length) length = kuint64max;
  std::wstring wfilepath(UTF8ToWide(filepath));
  handler->AppendFileRangeToUpload(wfilepath, offset, length);
  return CPERR_SUCCESS;
}

int STDCALL CPR_GetResponseInfo(CPRequest* request, CPResponseInfoType type,
                                void* buf, uint32 buf_size) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandlerProxy* handler =
      PluginRequestHandlerProxy::FromCPRequest(request);
  CHECK(handler);
  return handler->GetResponseInfo(type, buf, buf_size);
}

int STDCALL CPR_Read(CPRequest* request, void* buf, uint32 buf_size) {
  CHECK(ChromePluginLib::IsPluginThread());
  PluginRequestHandlerProxy* handler =
      PluginRequestHandlerProxy::FromCPRequest(request);
  CHECK(handler);
  return handler->Read(buf, buf_size);
}


CPBool STDCALL CPB_IsPluginProcessRunning(CPID id) {
  CHECK(ChromePluginLib::IsPluginThread());
  return true;
}

CPProcessType STDCALL CPB_GetProcessType(CPID id) {
  CHECK(ChromePluginLib::IsPluginThread());
  return CP_PROCESS_PLUGIN;
}

CPError STDCALL CPB_SendMessage(CPID id, const void *data, uint32 data_len) {
  CHECK(ChromePluginLib::IsPluginThread());
  const uint8* data_ptr = static_cast<const uint8*>(data);
  std::vector<uint8> v(data_ptr, data_ptr + data_len);
  if (!PluginThread::current()->Send(new PluginProcessHostMsg_PluginMessage(v)))
    return CPERR_FAILURE;

  return CPERR_SUCCESS;
}

CPError STDCALL CPB_SendSyncMessage(CPID id, const void *data, uint32 data_len,
                                    void **retval, uint32 *retval_len) {
  CHECK(ChromePluginLib::IsPluginThread());
  const uint8* data_ptr = static_cast<const uint8*>(data);
  std::vector<uint8> v(data_ptr, data_ptr + data_len);
  std::vector<uint8> r;
  if (!PluginThread::current()->Send(
          new PluginProcessHostMsg_PluginSyncMessage(v, &r))) {
    return CPERR_FAILURE;
  }

  if (r.size()) {
    *retval_len = static_cast<uint32>(r.size());
    *retval = CPB_Alloc(*retval_len);
    memcpy(*retval, &(r.at(0)), r.size());
  } else {
    *retval = NULL;
    *retval_len = 0;
  }

  return CPERR_SUCCESS;
}

CPError STDCALL CPB_PluginThreadAsyncCall(CPID id,
                                          void (*func)(void *),
                                          void *user_data) {
  MessageLoop *message_loop = PluginThread::current()->message_loop();
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

}  // namespace

CPBrowserFuncs* GetCPBrowserFuncsForPlugin() {
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
    browser_funcs.show_html_dialog = CPB_ShowHtmlDialog;
    browser_funcs.show_html_dialog_modal = CPB_ShowHtmlDialogModal;
    browser_funcs.is_plugin_process_running = CPB_IsPluginProcessRunning;
    browser_funcs.get_process_type = CPB_GetProcessType;
    browser_funcs.send_message = CPB_SendMessage;
    browser_funcs.get_browsing_context_from_npp = CPB_GetBrowsingContextFromNPP;
    browser_funcs.get_browsing_context_info = CPB_GetBrowsingContextInfo;
    browser_funcs.get_command_line_arguments = CPB_GetCommandLineArguments;
    browser_funcs.add_ui_command = CPB_AddUICommand;
    browser_funcs.handle_command = CPB_HandleCommand;
    browser_funcs.send_sync_message = CPB_SendSyncMessage;
    browser_funcs.plugin_thread_async_call = CPB_PluginThreadAsyncCall;
    browser_funcs.open_file_dialog = CPB_OpenFileDialog;

    browser_funcs.request_funcs = &request_funcs;
    browser_funcs.response_funcs = &response_funcs;

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
