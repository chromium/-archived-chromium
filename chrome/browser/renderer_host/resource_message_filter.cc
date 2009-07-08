// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_message_filter.h"

#include "base/clipboard.h"
#include "base/command_line.h"
#include "base/gfx/native_widget_types.h"
#include "base/histogram.h"
#include "base/process_util.h"
#include "base/thread.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/chrome_plugin_browsing_context.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/in_process_webkit/dom_storage_dispatcher_host.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/file_system_accessor.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/browser/worker_host/worker_service.h"
#include "chrome/common/app_cache/app_cache_dispatcher_host.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/histogram_synchronizer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "net/base/cookie_monster.h"
#include "net/base/mime_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin.h"

#if defined(OS_WIN)
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#elif defined(OS_MACOSX) || defined(OS_LINUX)
// TODO(port) remove this.
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

#if defined(OS_WIN)
#include "webkit/api/public/win/WebScreenInfoFactory.h"
#elif defined(OS_MACOSX)
#include "webkit/api/public/mac/WebScreenInfoFactory.h"
#endif

using WebKit::WebCache;
using WebKit::WebScreenInfo;
#if !defined(OS_LINUX)
using WebKit::WebScreenInfoFactory;
#endif

namespace {

// Context menus are somewhat complicated. We need to intercept them here on
// the I/O thread to add any spelling suggestions to them. After that's done,
// we need to forward the modified message to the UI thread and the normal
// message forwarding isn't set up for sending modified messages.
//
// Therefore, this class dispatches the IPC message to the RenderProcessHost
// with the given ID (if possible) to emulate the normal dispatch.
class ContextMenuMessageDispatcher : public Task {
 public:
  ContextMenuMessageDispatcher(
      int render_process_id,
      const ViewHostMsg_ContextMenu& context_menu_message)
      : render_process_id_(render_process_id),
        context_menu_message_(context_menu_message) {
  }

  void Run() {
    RenderProcessHost* host =
        RenderProcessHost::FromID(render_process_id_);
    if (host)
      host->OnMessageReceived(context_menu_message_);
  }

 private:
  int render_process_id_;
  const ViewHostMsg_ContextMenu context_menu_message_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuMessageDispatcher);
};

// Completes a clipboard write initiated by the renderer. The write must be
// performed on the UI thread because the clipboard service from the IO thread
// cannot create windows so it cannot be the "owner" of the clipboard's
// contents.
class WriteClipboardTask : public Task {
 public:
  explicit WriteClipboardTask(Clipboard::ObjectMap* objects)
      : objects_(objects) {}
  ~WriteClipboardTask() {}

  void Run() {
    g_browser_process->clipboard()->WriteObjects(*objects_.get());
  }

 private:
  scoped_ptr<Clipboard::ObjectMap> objects_;
};

void RenderParamsFromPrintSettings(const printing::PrintSettings& settings,
                                   ViewMsg_Print_Params* params) {
  DCHECK(params);
#if defined(OS_WIN)
  params->printable_size.SetSize(
      settings.page_setup_pixels().content_area().width(),
      settings.page_setup_pixels().content_area().height());
  params->dpi = settings.dpi();
  // Currently hardcoded at 1.25. See PrintSettings' constructor.
  params->min_shrink = settings.min_shrink;
  // Currently hardcoded at 2.0. See PrintSettings' constructor.
  params->max_shrink = settings.max_shrink;
  // Currently hardcoded at 72dpi. See PrintSettings' constructor.
  params->desired_dpi = settings.desired_dpi;
  // Always use an invalid cookie.
  params->document_cookie = 0;
  params->selection_only = settings.selection_only;
#else
  NOTIMPLEMENTED();
#endif
}

}  // namespace

ResourceMessageFilter::ResourceMessageFilter(
    ResourceDispatcherHost* resource_dispatcher_host,
    AudioRendererHost* audio_renderer_host,
    PluginService* plugin_service,
    printing::PrintJobManager* print_job_manager,
    Profile* profile,
    RenderWidgetHelper* render_widget_helper,
    SpellChecker* spellchecker)
    : Receiver(RENDER_PROCESS),
      channel_(NULL),
      resource_dispatcher_host_(resource_dispatcher_host),
      plugin_service_(plugin_service),
      print_job_manager_(print_job_manager),
      render_process_id_(-1),
      spellchecker_(spellchecker),
      ALLOW_THIS_IN_INITIALIZER_LIST(resolve_proxy_msg_helper_(this, NULL)),
      request_context_(profile->GetRequestContext()),
      media_request_context_(profile->GetRequestContextForMedia()),
      extensions_request_context_(profile->GetRequestContextForExtensions()),
      profile_(profile),
      render_widget_helper_(render_widget_helper),
      audio_renderer_host_(audio_renderer_host),
      app_cache_dispatcher_host_(new AppCacheDispatcherHost),
      ALLOW_THIS_IN_INITIALIZER_LIST(dom_storage_dispatcher_host_(
          new DOMStorageDispatcherHost(this, profile->GetWebKitContext(),
              resource_dispatcher_host->webkit_thread()))),
      off_the_record_(profile->IsOffTheRecord()) {
  DCHECK(request_context_.get());
  DCHECK(request_context_->cookie_store());
  DCHECK(media_request_context_.get());
  DCHECK(media_request_context_->cookie_store());
  DCHECK(audio_renderer_host_.get());
  DCHECK(app_cache_dispatcher_host_.get());
  DCHECK(dom_storage_dispatcher_host_.get());
}

ResourceMessageFilter::~ResourceMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // Let interested observers know we are being deleted.
  NotificationService::current()->Notify(
      NotificationType::RESOURCE_MESSAGE_FILTER_SHUTDOWN,
      Source<ResourceMessageFilter>(this),
      NotificationService::NoDetails());

  if (handle())
    base::CloseProcessHandle(handle());
}

void ResourceMessageFilter::Init(int render_process_id) {
  render_process_id_ = render_process_id;
  render_widget_helper_->Init(render_process_id, resource_dispatcher_host_);
  app_cache_dispatcher_host_->Initialize(this);
}

// Called on the IPC thread:
void ResourceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;

  // Add the observers to intercept.
  registrar_.Add(this, NotificationType::SPELLCHECKER_REINITIALIZED,
                 Source<Profile>(static_cast<Profile*>(profile_)));
}

// Called on the IPC thread:
void ResourceMessageFilter::OnChannelConnected(int32 peer_pid) {
  DCHECK(!handle()) << " " << handle();
  base::ProcessHandle peer_handle;
  if (!base::OpenProcessHandle(peer_pid, &peer_handle)) {
    NOTREACHED();
  }
  set_handle(peer_handle);

  // Set the process ID if Init hasn't been called yet.  This doesn't work in
  // single-process mode since peer_pid won't be the special fake PID we use
  // for RenderProcessHost in that mode, so we just have to hope that Init
  // is called first in that case.
  if (render_process_id_ == -1)
    render_process_id_ = peer_pid;

  // Hook AudioRendererHost to this object after channel is connected so it can
  // this object for sending messages.
  audio_renderer_host_->IPCChannelConnected(render_process_id_, handle(), this);

  WorkerService::GetInstance()->Initialize(
      resource_dispatcher_host_, ui_loop());
}

void ResourceMessageFilter::OnChannelError() {
  NotificationService::current()->Notify(
      NotificationType::RESOURCE_MESSAGE_FILTER_SHUTDOWN,
      Source<ResourceMessageFilter>(this),
      NotificationService::NoDetails());
}

// Called on the IPC thread:
void ResourceMessageFilter::OnChannelClosing() {
  channel_ = NULL;

  // Unhook us from all pending network requests so they don't get sent to a
  // deleted object.
  resource_dispatcher_host_->CancelRequestsForProcess(render_process_id_);

  // Unhook AudioRendererHost.
  audio_renderer_host_->IPCChannelClosing();
}

// Called on the IPC thread:
bool ResourceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool msg_is_ok = true;
  bool handled = resource_dispatcher_host_->OnMessageReceived(
                                                message, this, &msg_is_ok) ||
                 app_cache_dispatcher_host_->OnMessageReceived(
                                                 message, &msg_is_ok) ||
                 dom_storage_dispatcher_host_->OnMessageReceived(message) ||
                 audio_renderer_host_->OnMessageReceived(message, &msg_is_ok);

  if (!handled) {
    DCHECK(msg_is_ok);  // It should have been marked handled if it wasn't OK.
    handled = true;
    IPC_BEGIN_MESSAGE_MAP_EX(ResourceMessageFilter, message, msg_is_ok)
      // On Linux we need to dispatch these messages to the UI2 thread because
      // we cannot make X calls from the IO thread.  On other platforms, we can
      // handle these calls directly.
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetScreenInfo,
                                      OnGetScreenInfo)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetWindowRect,
                                      OnGetWindowRect)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetRootWindowRect,
                                      OnGetRootWindowRect)

      IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWindow, OnMsgCreateWindow)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnMsgCreateWidget)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SetCookie, OnSetCookie)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetCookies, OnGetCookies)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetDataDir, OnGetDataDir)
      IPC_MESSAGE_HANDLER(ViewHostMsg_PluginMessage, OnPluginMessage)
      IPC_MESSAGE_HANDLER(ViewHostMsg_PluginSyncMessage, OnPluginSyncMessage)
#if defined(OS_WIN)  // This hack is Windows-specific.
      IPC_MESSAGE_HANDLER(ViewHostMsg_LoadFont, OnLoadFont)
#endif
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetPlugins, OnGetPlugins)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetPluginPath, OnGetPluginPath)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DownloadUrl, OnDownloadUrl)
      IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_ContextMenu,
          OnReceiveContextMenuMsg(message))
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenChannelToPlugin,
                                      OnOpenChannelToPlugin)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CreateDedicatedWorker,
                          OnCreateDedicatedWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CancelCreateDedicatedWorker,
                          OnCancelCreateDedicatedWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardToWorker,
                          OnForwardToWorker)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_SpellCheck, OnSpellCheck)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetAutoCorrectWord,
                                      OnGetAutoCorrectWord)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DnsPrefetch, OnDnsPrefetch)
      IPC_MESSAGE_HANDLER(ViewHostMsg_RendererHistograms,
                          OnRendererHistograms)
      IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_PaintRect,
          render_widget_helper_->DidReceivePaintMsg(message))
      IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardWriteObjectsAsync,
                          OnClipboardWriteObjects)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardWriteObjectsSync,
                          OnClipboardWriteObjects)

      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardIsFormatAvailable,
                                      OnClipboardIsFormatAvailable)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardReadText,
                                      OnClipboardReadText)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardReadAsciiText,
                                      OnClipboardReadAsciiText)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardReadHTML,
                                      OnClipboardReadHTML)

      IPC_MESSAGE_HANDLER(ViewHostMsg_GetMimeTypeFromExtension,
                          OnGetMimeTypeFromExtension)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetMimeTypeFromFile,
                          OnGetMimeTypeFromFile)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetPreferredExtensionForMimeType,
                          OnGetPreferredExtensionForMimeType)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetCPBrowsingContext,
                          OnGetCPBrowsingContext)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DuplicateSection, OnDuplicateSection)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ResourceTypeStats, OnResourceTypeStats)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ResolveProxy, OnResolveProxy)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetDefaultPrintSettings,
                                      OnGetDefaultPrintSettings)
#if defined(OS_WIN)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ScriptedPrint,
                                      OnScriptedPrint)
#endif
#if defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER(ViewHostMsg_AllocTransportDIB,
                          OnAllocTransportDIB)
      IPC_MESSAGE_HANDLER(ViewHostMsg_FreeTransportDIB,
                          OnFreeTransportDIB)
#endif
      IPC_MESSAGE_HANDLER(ViewHostMsg_OpenChannelToExtension,
                          OnOpenChannelToExtension)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CloseIdleConnections,
                          OnCloseIdleConnections)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SetCacheMode,
                          OnSetCacheMode)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetFileSize,
                                      OnGetFileSize)

      IPC_MESSAGE_UNHANDLED(
          handled = false)
    IPC_END_MESSAGE_MAP_EX()
  }

  if (!msg_is_ok) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(message.type(),
                                                         handle());
  }

  return handled;
}

void ResourceMessageFilter::OnReceiveContextMenuMsg(const IPC::Message& msg) {
  void* iter = NULL;
  ContextMenuParams params;
  if (!IPC::ParamTraits<ContextMenuParams>::Read(&msg, &iter, &params))
    return;

  // Fill in the dictionary suggestions if required.
  if (!params.misspelled_word.empty() &&
      spellchecker_ != NULL && params.spellcheck_enabled) {
    int misspell_location, misspell_length;
    bool is_misspelled = !spellchecker_->SpellCheckWord(
        params.misspelled_word.c_str(),
        static_cast<int>(params.misspelled_word.length()),
        &misspell_location, &misspell_length,
        &params.dictionary_suggestions);

    // If not misspelled, make the misspelled_word param empty.
    if (!is_misspelled)
      params.misspelled_word.clear();
  }

  // Create a new ViewHostMsg_ContextMenu message.
  const ViewHostMsg_ContextMenu context_menu_message(msg.routing_id(), params);
  render_widget_helper_->ui_loop()->PostTask(FROM_HERE,
      new ContextMenuMessageDispatcher(render_process_id_,
                                       context_menu_message));
}

// Called on the IPC thread:
bool ResourceMessageFilter::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

URLRequestContext* ResourceMessageFilter::GetRequestContext(
    uint32 request_id,
    const ViewHostMsg_Resource_Request& request_data) {
  URLRequestContext* request_context = request_context_;
  // If the request has resource type of ResourceType::MEDIA, we use a request
  // context specific to media for handling it because these resources have
  // specific needs for caching.
  if (request_data.resource_type == ResourceType::MEDIA)
    request_context = media_request_context_;
  return request_context;
}

void ResourceMessageFilter::OnMsgCreateWindow(
    int opener_id, bool user_gesture, int* route_id,
    ModalDialogEvent* modal_dialog_event) {
  render_widget_helper_->CreateNewWindow(opener_id,
                                         user_gesture,
                                         handle(),
                                         route_id,
                                         modal_dialog_event);
}

void ResourceMessageFilter::OnMsgCreateWidget(int opener_id,
                                              bool activatable,
                                              int* route_id) {
  render_widget_helper_->CreateNewWidget(opener_id, activatable, route_id);
}

void ResourceMessageFilter::OnSetCookie(const GURL& url,
                                        const GURL& first_party_for_cookies,
                                        const std::string& cookie) {
  URLRequestContext* context = url.SchemeIs(chrome::kExtensionScheme) ?
      extensions_request_context_.get() : request_context_.get();
  if (context->cookie_policy()->CanSetCookie(url, first_party_for_cookies))
    context->cookie_store()->SetCookie(url, cookie);
}

void ResourceMessageFilter::OnGetCookies(const GURL& url,
                                         const GURL& first_party_for_cookies,
                                         std::string* cookies) {
  URLRequestContext* context = url.SchemeIs(chrome::kExtensionScheme) ?
      extensions_request_context_.get() : request_context_.get();
  if (context->cookie_policy()->CanGetCookies(url, first_party_for_cookies))
    *cookies = context->cookie_store()->GetCookies(url);
}

void ResourceMessageFilter::OnGetDataDir(std::wstring* data_dir) {
  *data_dir = plugin_service_->GetChromePluginDataDir().ToWStringHack();
}

void ResourceMessageFilter::OnPluginMessage(const FilePath& plugin_path,
                                            const std::vector<uint8>& data) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  ChromePluginLib *chrome_plugin = ChromePluginLib::Find(plugin_path);
  if (chrome_plugin) {
    void *data_ptr = const_cast<void*>(reinterpret_cast<const void*>(&data[0]));
    uint32 data_len = static_cast<uint32>(data.size());
    chrome_plugin->functions().on_message(data_ptr, data_len);
  }
}

void ResourceMessageFilter::OnPluginSyncMessage(const FilePath& plugin_path,
                                                const std::vector<uint8>& data,
                                                std::vector<uint8> *retval) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  ChromePluginLib *chrome_plugin = ChromePluginLib::Find(plugin_path);
  if (chrome_plugin) {
    void *data_ptr = const_cast<void*>(reinterpret_cast<const void*>(&data[0]));
    uint32 data_len = static_cast<uint32>(data.size());
    void *retval_buffer = 0;
    uint32 retval_size = 0;
    chrome_plugin->functions().on_sync_message(data_ptr, data_len,
                                               &retval_buffer, &retval_size);
    if (retval_buffer) {
      retval->resize(retval_size);
      memcpy(&(retval->at(0)), retval_buffer, retval_size);
      CPB_Free(retval_buffer);
    }
  }
}

#if defined(OS_WIN)  // This hack is Windows-specific.
void ResourceMessageFilter::OnLoadFont(LOGFONT font) {
  // If renderer is running in a sandbox, GetTextMetrics
  // can sometimes fail. If a font has not been loaded
  // previously, GetTextMetrics will try to load the font
  // from the font file. However, the sandboxed renderer does
  // not have permissions to access any font files and
  // the call fails. So we make the browser pre-load the
  // font for us by using a dummy call to GetTextMetrics of
  // the same font.

  // Maintain a circular queue for the fonts and DCs to be cached.
  // font_index maintains next available location in the queue.
  static const int kFontCacheSize = 32;
  static HFONT fonts[kFontCacheSize] = {0};
  static HDC hdcs[kFontCacheSize] = {0};
  static size_t font_index = 0;

  UMA_HISTOGRAM_COUNTS_100("Memory.CachedFontAndDC",
      fonts[kFontCacheSize-1] ? kFontCacheSize : static_cast<int>(font_index));

  HDC hdc = GetDC(NULL);
  HFONT font_handle = CreateFontIndirect(&font);
  DCHECK(NULL != font_handle);

  HGDIOBJ old_font = SelectObject(hdc, font_handle);
  DCHECK(NULL != old_font);

  TEXTMETRIC tm;
  BOOL ret = GetTextMetrics(hdc, &tm);
  DCHECK(ret);

  if (fonts[font_index] || hdcs[font_index]) {
    // We already have too many fonts, we will delete one and take it's place.
    DeleteObject(fonts[font_index]);
    ReleaseDC(NULL, hdcs[font_index]);
  }

  fonts[font_index] = font_handle;
  hdcs[font_index] = hdc;
  font_index = (font_index + 1) % kFontCacheSize;
}
#endif

#if !defined(OS_LINUX)
void ResourceMessageFilter::OnGetScreenInfo(gfx::NativeViewId view,
                                            IPC::Message* reply_msg) {
  // TODO(darin): Change this into a routed message so that we can eliminate
  // the NativeViewId parameter.
  WebScreenInfo results =
      WebScreenInfoFactory::screenInfo(gfx::NativeViewFromId(view));
  ViewHostMsg_GetScreenInfo::WriteReplyParams(reply_msg, results);
  Send(reply_msg);
}
#endif

void ResourceMessageFilter::OnGetPlugins(bool refresh,
                                         std::vector<WebPluginInfo>* plugins) {
  plugin_service_->GetPlugins(refresh, plugins);
}

void ResourceMessageFilter::OnGetPluginPath(const GURL& url,
                                            const GURL& policy_url,
                                            const std::string& mime_type,
                                            const std::string& clsid,
                                            FilePath* filename,
                                            std::string* url_mime_type) {
  *filename = plugin_service_->GetPluginPath(url, policy_url, mime_type, clsid,
                                             url_mime_type);
}

void ResourceMessageFilter::OnOpenChannelToPlugin(const GURL& url,
                                                  const std::string& mime_type,
                                                  const std::string& clsid,
                                                  const std::wstring& locale,
                                                  IPC::Message* reply_msg) {
  plugin_service_->OpenChannelToPlugin(this, url, mime_type, clsid,
                                       locale, reply_msg);
}

void ResourceMessageFilter::OnCreateDedicatedWorker(const GURL& url,
                                                    int render_view_route_id,
                                                    int* route_id) {
  *route_id = render_widget_helper_->GetNextRoutingID();
  WorkerService::GetInstance()->CreateDedicatedWorker(
      url, render_process_id_, render_view_route_id, this, render_process_id_,
      *route_id);
}

void ResourceMessageFilter::OnCancelCreateDedicatedWorker(int route_id) {
  WorkerService::GetInstance()->CancelCreateDedicatedWorker(
      render_process_id_, route_id);
}

void ResourceMessageFilter::OnForwardToWorker(const IPC::Message& message) {
  WorkerService::GetInstance()->ForwardMessage(message, render_process_id_);
}

void ResourceMessageFilter::OnDownloadUrl(const IPC::Message& message,
                                          const GURL& url,
                                          const GURL& referrer) {
  resource_dispatcher_host_->BeginDownload(url,
                                           referrer,
                                           render_process_id_,
                                           message.routing_id(),
                                           request_context_);
}

void ResourceMessageFilter::OnClipboardWriteObjects(
    const Clipboard::ObjectMap& objects) {
  // We cannot write directly from the IO thread, and cannot service the IPC
  // on the UI thread. We'll copy the relevant data and get a handle to any
  // shared memory so it doesn't go away when we resume the renderer, and post
  // a task to perform the write on the UI thread.
  Clipboard::ObjectMap* long_living_objects = new Clipboard::ObjectMap(objects);

#if defined(OS_WIN)
  // We pass the renderer handle to assist the clipboard with using shared
  // memory objects. handle() is a handle to the process that would
  // own any shared memory that might be in the object list. We only do this
  // on Windows and it only applies to bitmaps. (On Linux, bitmaps
  // are copied pixel by pixel rather than using shared memory.)
  Clipboard::DuplicateRemoteHandles(handle(), long_living_objects);
#endif

  render_widget_helper_->ui_loop()->PostTask(FROM_HERE,
      new WriteClipboardTask(long_living_objects));
}

#if !defined(OS_LINUX)
// On non-Linux platforms, clipboard actions can be performed on the IO thread.
// On Linux, since the clipboard is linked with GTK, we either have to do this
// with GTK on the UI thread, or with Xlib on the BACKGROUND_X11 thread. In an
// ideal world, we would do the latter. However, for now we're going to
// terminate these calls on the UI thread. This risks deadlock in the case of
// plugins, but it's better than crashing which is what doing on the IO thread
// gives us.
//
// See resource_message_filter_gtk.cc for the Linux implementation of these
// functions.

void ResourceMessageFilter::OnClipboardIsFormatAvailable(
    Clipboard::FormatType format, IPC::Message* reply) {
  const bool result = GetClipboard()->IsFormatAvailable(format);
  ViewHostMsg_ClipboardIsFormatAvailable::WriteReplyParams(reply, result);
  Send(reply);
}

void ResourceMessageFilter::OnClipboardReadText(IPC::Message* reply) {
  string16 result;
  GetClipboard()->ReadText(&result);
  ViewHostMsg_ClipboardReadText::WriteReplyParams(reply, result);
  Send(reply);
}

void ResourceMessageFilter::OnClipboardReadAsciiText(IPC::Message* reply) {
  std::string result;
  GetClipboard()->ReadAsciiText(&result);
  ViewHostMsg_ClipboardReadAsciiText::WriteReplyParams(reply, result);
  Send(reply);
}

void ResourceMessageFilter::OnClipboardReadHTML(IPC::Message* reply) {
  std::string src_url_str;
  string16 markup;
  GetClipboard()->ReadHTML(&markup, &src_url_str);
  const GURL src_url = GURL(src_url_str);

  ViewHostMsg_ClipboardReadHTML::WriteReplyParams(reply, markup, src_url);
  Send(reply);
}

#endif

void ResourceMessageFilter::OnGetMimeTypeFromExtension(
    const FilePath::StringType& ext, std::string* mime_type) {
  net::GetMimeTypeFromExtension(ext, mime_type);
}

void ResourceMessageFilter::OnGetMimeTypeFromFile(
    const FilePath& file_path, std::string* mime_type) {
  net::GetMimeTypeFromFile(file_path, mime_type);
}

void ResourceMessageFilter::OnGetPreferredExtensionForMimeType(
    const std::string& mime_type, FilePath::StringType* ext) {
  net::GetPreferredExtensionForMimeType(mime_type, ext);
}

void ResourceMessageFilter::OnGetCPBrowsingContext(uint32* context) {
  // Always allocate a new context when a plugin requests one, since it needs to
  // be unique for that plugin instance.
  *context =
      CPBrowsingContextManager::Instance()->Allocate(request_context_.get());
}

void ResourceMessageFilter::OnDuplicateSection(
    base::SharedMemoryHandle renderer_handle,
    base::SharedMemoryHandle* browser_handle) {
  // Duplicate the handle in this process right now so the memory is kept alive
  // (even if it is not mapped)
  base::SharedMemory shared_buf(renderer_handle, true, handle());
  shared_buf.GiveToProcess(base::GetCurrentProcessHandle(), browser_handle);
}

void ResourceMessageFilter::OnResourceTypeStats(
    const WebCache::ResourceTypeStats& stats) {
  HISTOGRAM_COUNTS("WebCoreCache.ImagesSizeKB",
                   static_cast<int>(stats.images.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.CSSStylesheetsSizeKB",
                   static_cast<int>(stats.cssStyleSheets.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.ScriptsSizeKB",
                   static_cast<int>(stats.scripts.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.XSLStylesheetsSizeKB",
                   static_cast<int>(stats.xslStyleSheets.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.FontsSizeKB",
                   static_cast<int>(stats.fonts.size / 1024));
}

void ResourceMessageFilter::OnResolveProxy(const GURL& url,
                                           IPC::Message* reply_msg) {
  resolve_proxy_msg_helper_.Start(url, reply_msg);
}

void ResourceMessageFilter::OnResolveProxyCompleted(
    IPC::Message* reply_msg,
    int result,
    const std::string& proxy_list) {
  ViewHostMsg_ResolveProxy::WriteReplyParams(reply_msg, result, proxy_list);
  Send(reply_msg);
}

void ResourceMessageFilter::OnGetDefaultPrintSettings(IPC::Message* reply_msg) {
  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager_->PopPrinterQuery(0, &printer_query);
  if (!printer_query.get()) {
    printer_query = new printing::PrinterQuery;
  }

  CancelableTask* task = NewRunnableMethod(
      this,
      &ResourceMessageFilter::OnGetDefaultPrintSettingsReply,
      printer_query,
      reply_msg);
  // Loads default settings. This is asynchronous, only the IPC message sender
  // will hang until the settings are retrieved.
  printer_query->GetSettings(printing::PrinterQuery::DEFAULTS,
                             NULL,
                             0,
                             false,
                             task);
}

void ResourceMessageFilter::OnGetDefaultPrintSettingsReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  ViewMsg_Print_Params params;
  if (printer_query->last_status() != printing::PrintingContext::OK) {
    memset(&params, 0, sizeof(params));
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params);
    params.document_cookie = printer_query->cookie();
  }
  ViewHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  // If user hasn't cancelled.
  if (printer_query->cookie() && printer_query->settings().dpi()) {
    print_job_manager_->QueuePrinterQuery(printer_query.get());
  } else {
    printer_query->StopWorker();
  }
}

#if defined(OS_WIN)

void ResourceMessageFilter::OnScriptedPrint(gfx::NativeViewId host_window_id,
                                            int cookie,
                                            int expected_pages_count,
                                            bool has_selection,
                                            IPC::Message* reply_msg) {
  HWND host_window = gfx::NativeViewFromId(host_window_id);

  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager_->PopPrinterQuery(cookie, &printer_query);
  if (!printer_query.get()) {
    printer_query = new printing::PrinterQuery;
  }

  CancelableTask* task = NewRunnableMethod(
      this,
      &ResourceMessageFilter::OnScriptedPrintReply,
      printer_query,
      reply_msg);
  // Shows the Print... dialog box. This is asynchronous, only the IPC message
  // sender will hang until the Print dialog is dismissed.
  if (!host_window || !IsWindow(host_window)) {
    // TODO(maruel):  bug 1214347 Get the right browser window instead.
    host_window = GetDesktopWindow();
  } else {
    host_window = GetAncestor(host_window, GA_ROOTOWNER);
  }
  DCHECK(host_window);
  printer_query->GetSettings(printing::PrinterQuery::ASK_USER,
                             host_window,
                             expected_pages_count,
                             has_selection,
                             task);
}

void ResourceMessageFilter::OnScriptedPrintReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  ViewMsg_PrintPages_Params params;
  if (printer_query->last_status() != printing::PrintingContext::OK ||
      !printer_query->settings().dpi()) {
    memset(&params, 0, sizeof(params));
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params.params);
    params.params.document_cookie = printer_query->cookie();
    params.pages =
        printing::PageRange::GetPages(printer_query->settings().ranges);
  }
  ViewHostMsg_ScriptedPrint::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  if (params.params.dpi && params.params.document_cookie) {
    print_job_manager_->QueuePrinterQuery(printer_query.get());
  } else {
    printer_query->StopWorker();
  }
}

#endif  // OS_WIN

// static
Clipboard* ResourceMessageFilter::GetClipboard() {
  // We have a static instance of the clipboard service for use by all message
  // filters.  This instance lives for the life of the browser processes.
  static Clipboard* clipboard = new Clipboard;

  return clipboard;
}

// Notes about SpellCheck.
//
// Spellchecking generally uses a fair amount of RAM.  For this reason, we load
// the spellcheck dictionaries into the browser process, and all renderers ask
// the browsers to do SpellChecking.
//
// This filter should not try to initialize the spellchecker. It is up to the
// profile to initialize it when required, and send it here. If |spellchecker_|
// is made NULL, it corresponds to spellchecker turned off - i.e., all
// spellings are correct.
//
// Note: This is called in the IO thread.
void ResourceMessageFilter::OnSpellCheck(const std::wstring& word,
                                         IPC::Message* reply_msg) {
  int misspell_location = 0;
  int misspell_length = 0;

  if (spellchecker_ != NULL) {
    spellchecker_->SpellCheckWord(word.c_str(),
                                  static_cast<int>(word.length()),
                                  &misspell_location, &misspell_length, NULL);
  }

  ViewHostMsg_SpellCheck::WriteReplyParams(reply_msg, misspell_location,
                                           misspell_length);
  Send(reply_msg);
  return;
}


void ResourceMessageFilter::OnGetAutoCorrectWord(const std::wstring& word,
                                                 IPC::Message* reply_msg) {
  std::wstring autocorrect_word;
  if (spellchecker_ != NULL) {
    spellchecker_->GetAutoCorrectionWord(word, &autocorrect_word);
  }

  ViewHostMsg_GetAutoCorrectWord::WriteReplyParams(reply_msg,
                                                   autocorrect_word);
  Send(reply_msg);
  return;
}

void ResourceMessageFilter::Observe(NotificationType type,
                                    const NotificationSource &source,
                                    const NotificationDetails &details) {
  if (type == NotificationType::SPELLCHECKER_REINITIALIZED) {
    spellchecker_ = Details<SpellcheckerReinitializedDetails>
        (details).ptr()->spellchecker;
  }
}

void ResourceMessageFilter::OnDnsPrefetch(
    const std::vector<std::string>& hostnames) {
  chrome_browser_net::DnsPrefetchList(hostnames);
}

void ResourceMessageFilter::OnRendererHistograms(
    int sequence_number,
    const std::vector<std::string>& histograms) {
  HistogramSynchronizer::DeserializeHistogramList(sequence_number, histograms);
}

#if defined(OS_MACOSX)
void ResourceMessageFilter::OnAllocTransportDIB(
    size_t size, TransportDIB::Handle* handle) {
  render_widget_helper_->AllocTransportDIB(size, handle);
}

void ResourceMessageFilter::OnFreeTransportDIB(
    TransportDIB::Id dib_id) {
  render_widget_helper_->FreeTransportDIB(dib_id);
}
#endif

void ResourceMessageFilter::OnOpenChannelToExtension(
    int routing_id, const std::string& extension_id, int* port_id) {
  *port_id = ExtensionMessageService::GetInstance(request_context_.get())->
      OpenChannelToExtension(routing_id, extension_id, this);
}

bool ResourceMessageFilter::CheckBenchmarkingEnabled() {
  static bool checked = false;
  static bool result = false;
  if (!checked) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    result = command_line.HasSwitch(switches::kEnableBenchmarking);
    checked = true;
  }
  return result;
}

void ResourceMessageFilter::OnCloseIdleConnections() {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled())
    return;
  request_context_->
      http_transaction_factory()->GetCache()->CloseIdleConnections();
}

void ResourceMessageFilter::OnSetCacheMode(bool enabled) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled())
    return;

  net::HttpCache::Mode mode = enabled ?
      net::HttpCache::NORMAL : net::HttpCache::DISABLE;
  request_context_->http_transaction_factory()->GetCache()->set_mode(mode);
}

void ResourceMessageFilter::OnGetFileSize(const FilePath& path,
                                          IPC::Message* reply_msg) {
  // Increase the ref count to ensure ResourceMessageFilter won't be destroyed
  // before GetFileSize callback is done.
  AddRef();

  // Get file size only when the child process has been granted permission to
  // upload the file.
  if (ChildProcessSecurityPolicy::GetInstance()->CanUploadFile(
      render_process_id_, path)) {
    FileSystemAccessor::RequestFileSize(
        path,
        reply_msg,
        NewCallback(this, &ResourceMessageFilter::ReplyGetFileSize));
  } else {
    ReplyGetFileSize(-1, reply_msg);
  }
}

void ResourceMessageFilter::ReplyGetFileSize(int64 result, void* param) {
  IPC::Message* reply_msg = static_cast<IPC::Message*>(param);
  ViewHostMsg_GetFileSize::WriteReplyParams(reply_msg, result);
  Send(reply_msg);

  // Getting file size callback done, decrease the ref count.
  Release();
}
