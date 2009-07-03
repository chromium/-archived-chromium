// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RESOURCE_MSG_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_RESOURCE_MSG_FILTER_H_

#include <string>
#include <vector>

#include "base/clipboard.h"
#include "base/file_path.h"
#include "base/gfx/rect.h"
#include "base/gfx/native_widget_types.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "build/build_config.h"
#include "chrome/browser/net/resolve_proxy_msg_helper.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/common/modal_dialog_event.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/transport_dib.h"
#include "webkit/api/public/WebCache.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

class AppCacheDispatcherHost;
class AudioRendererHost;
class Clipboard;
class DOMStorageDispatcherHost;
class Profile;
class RenderWidgetHelper;
class SpellChecker;
struct ViewHostMsg_Audio_CreateStream;
struct WebPluginInfo;

namespace printing {
class PrinterQuery;
class PrintJobManager;
}

namespace WebKit {
struct WebScreenInfo;
}

// This class filters out incoming IPC messages for network requests and
// processes them on the IPC thread.  As a result, network requests are not
// delayed by costly UI processing that may be occuring on the main thread of
// the browser.  It also means that any hangs in starting a network request
// will not interfere with browser UI.

class ResourceMessageFilter : public IPC::ChannelProxy::MessageFilter,
                              public ResourceDispatcherHost::Receiver,
                              public NotificationObserver,
                              public ResolveProxyMsgHelper::Delegate {
 public:
  // Create the filter.
  // Note:  because the lifecycle of the ResourceMessageFilter is not
  //        tied to the lifecycle of the object which created it, the
  //        ResourceMessageFilter is 'given' ownership of the spellchecker
  //        object and must clean it up on exit.
  ResourceMessageFilter(ResourceDispatcherHost* resource_dispatcher_host,
                        AudioRendererHost* audio_renderer_host,
                        PluginService* plugin_service,
                        printing::PrintJobManager* print_job_manager,
                        Profile* profile,
                        RenderWidgetHelper* render_widget_helper,
                        SpellChecker* spellchecker);
  virtual ~ResourceMessageFilter();

  void Init(int render_process_id);

  // IPC::ChannelProxy::MessageFilter methods:
  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();
  virtual void OnChannelClosing();
  virtual bool OnMessageReceived(const IPC::Message& message);

  // ResourceDispatcherHost::Receiver methods:
  virtual bool Send(IPC::Message* message);
  virtual URLRequestContext* GetRequestContext(
      uint32 request_id,
      const ViewHostMsg_Resource_Request& request_data);
  virtual int GetProcessId() const { return render_process_id_; }

  SpellChecker* spellchecker() { return spellchecker_.get(); }
  ResourceDispatcherHost* resource_dispatcher_host() {
    return resource_dispatcher_host_;
  }
  MessageLoop* ui_loop() { return render_widget_helper_->ui_loop(); }
  bool off_the_record() { return off_the_record_; }

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  void OnMsgCreateWindow(int opener_id, bool user_gesture, int* route_id,
                         ModalDialogEvent* modal_dialog_event);
  void OnMsgCreateWidget(int opener_id, bool activatable, int* route_id);
  void OnSetCookie(const GURL& url,
                   const GURL& first_party_for_cookies,
                   const std::string& cookie);
  void OnGetCookies(const GURL& url,
                    const GURL& first_party_for_cookies,
                    std::string* cookies);
  void OnGetDataDir(std::wstring* data_dir);
  void OnPluginMessage(const FilePath& plugin_path,
                       const std::vector<uint8>& message);
  void OnPluginSyncMessage(const FilePath& plugin_path,
                           const std::vector<uint8>& message,
                           std::vector<uint8> *retval);
  void OnPluginFileDialog(const IPC::Message& msg,
                          bool multiple_files,
                          const std::wstring& title,
                          const std::wstring& filter,
                          uint32 user_data);

#if defined(OS_WIN)  // This hack is Windows-specific.
  // Cache fonts for the renderer. See ResourceMessageFilter::OnLoadFont
  // implementation for more details
  void OnLoadFont(LOGFONT font);
#endif

  void OnGetScreenInfo(gfx::NativeViewId window, IPC::Message* reply);
  void OnGetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins);
  void OnGetPluginPath(const GURL& url,
                       const GURL& policy_url,
                       const std::string& mime_type,
                       const std::string& clsid,
                       FilePath* filename,
                       std::string* actual_mime_type);
  void OnOpenChannelToPlugin(const GURL& url,
                             const std::string& mime_type,
                             const std::string& clsid,
                             const std::wstring& locale,
                             IPC::Message* reply_msg);
  void OnCreateDedicatedWorker(const GURL& url,
                               int render_view_route_id,
                               int* route_id);
  void OnCancelCreateDedicatedWorker(int route_id);
  void OnForwardToWorker(const IPC::Message& msg);
  void OnDownloadUrl(const IPC::Message& message,
                     const GURL& url,
                     const GURL& referrer);
  void OnSpellCheck(const std::wstring& word,
                    IPC::Message* reply_msg);
  void OnGetAutoCorrectWord(const std::wstring& word,
                            IPC::Message* reply_msg);
  void OnDnsPrefetch(const std::vector<std::string>& hostnames);
  void OnRendererHistograms(int sequence_number,
                            const std::vector<std::string>& histogram_info);
  void OnReceiveContextMenuMsg(const IPC::Message& msg);
  // Clipboard messages
  void OnClipboardWriteObjects(const Clipboard::ObjectMap& objects);

  void OnClipboardIsFormatAvailable(Clipboard::FormatType format,
                                    IPC::Message* reply);
  void OnClipboardReadText(IPC::Message* reply);
  void OnClipboardReadAsciiText(IPC::Message* reply);
  void OnClipboardReadHTML(IPC::Message* reply);

  void OnGetWindowRect(gfx::NativeViewId window, IPC::Message* reply);
  void OnGetRootWindowRect(gfx::NativeViewId window, IPC::Message* reply);
  void OnGetMimeTypeFromExtension(const FilePath::StringType& ext,
                                  std::string* mime_type);
  void OnGetMimeTypeFromFile(const FilePath& file_path,
                             std::string* mime_type);
  void OnGetPreferredExtensionForMimeType(const std::string& mime_type,
                                          FilePath::StringType* ext);
  void OnGetCPBrowsingContext(uint32* context);
  void OnDuplicateSection(base::SharedMemoryHandle renderer_handle,
                          base::SharedMemoryHandle* browser_handle);
  void OnResourceTypeStats(const WebKit::WebCache::ResourceTypeStats& stats);

  void OnResolveProxy(const GURL& url, IPC::Message* reply_msg);

  // ResolveProxyMsgHelper::Delegate implementation:
  virtual void OnResolveProxyCompleted(IPC::Message* reply_msg,
                                       int result,
                                       const std::string& proxy_list);

  // A javascript code requested to print the current page. This is done in two
  // steps and this is the first step. Get the print setting right here
  // synchronously. It will hang the I/O completely.
  void OnGetDefaultPrintSettings(IPC::Message* reply_msg);
  void OnGetDefaultPrintSettingsReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);
#if defined(OS_WIN)
  // A javascript code requested to print the current page. The renderer host
  // have to show to the user the print dialog and returns the selected print
  // settings.
  void OnScriptedPrint(gfx::NativeViewId host_window,
                       int cookie,
                       int expected_pages_count,
                       bool has_selection,
                       IPC::Message* reply_msg);
  void OnScriptedPrintReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);
#endif
  // Browser side transport DIB allocation
  void OnAllocTransportDIB(size_t size,
                           TransportDIB::Handle* result);
  void OnFreeTransportDIB(TransportDIB::Id dib_id);

  void OnOpenChannelToExtension(int routing_id,
                                const std::string& extension_id, int* port_id);

  void OnCloseIdleConnections();
  void OnSetCacheMode(bool enabled);

  void OnGetFileSize(const FilePath& path, IPC::Message* reply_msg);
  void ReplyGetFileSize(int64 result, void* param);

#if defined(OS_LINUX)
  void SendDelayedReply(IPC::Message* reply_msg);
  void DoOnGetScreenInfo(gfx::NativeViewId view, IPC::Message* reply_msg);
  void DoOnGetWindowRect(gfx::NativeViewId view, IPC::Message* reply_msg);
  void DoOnGetRootWindowRect(gfx::NativeViewId view, IPC::Message* reply_msg);
  void DoOnClipboardIsFormatAvailable(Clipboard::FormatType format,
                                      IPC::Message* reply_msg);
  void DoOnClipboardReadText(IPC::Message* reply_msg);
  void DoOnClipboardReadAsciiText(IPC::Message* reply_msg);
  void DoOnClipboardReadHTML(IPC::Message* reply_msg);
#endif

  bool CheckBenchmarkingEnabled();

  // We have our own clipboard because we want to access the clipboard on the
  // IO thread instead of forwarding (possibly synchronous) messages to the UI
  // thread. This instance of the clipboard should be accessed only on the IO
  // thread.
  static Clipboard* GetClipboard();

  NotificationRegistrar registrar_;

  // The channel associated with the renderer connection. This pointer is not
  // owned by this class.
  IPC::Channel* channel_;

  // Cached resource request dispatcher host and plugin service, guaranteed to
  // be non-null if Init succeeds. We do not own the objects, they are managed
  // by the BrowserProcess, which has a wider scope than we do.
  ResourceDispatcherHost* resource_dispatcher_host_;
  PluginService* plugin_service_;
  printing::PrintJobManager* print_job_manager_;

  // ID for the RenderProcessHost that corresponds to this channel.  This is
  // used by the ResourceDispatcherHost to look up the TabContents that
  // originated URLRequest.  Since the RenderProcessHost can be destroyed
  // before this object, we only hold an ID for lookup.
  int render_process_id_;

  // Our spellchecker object.
  scoped_refptr<SpellChecker> spellchecker_;

  // Helper class for handling PluginProcessHost_ResolveProxy messages (manages
  // the requests to the proxy service).
  ResolveProxyMsgHelper resolve_proxy_msg_helper_;

  // Contextual information to be used for requests created here.
  scoped_refptr<URLRequestContext> request_context_;

  // A request context specific for media resources.
  scoped_refptr<URLRequestContext> media_request_context_;

  scoped_refptr<URLRequestContext> extensions_request_context_;

  // A pointer to the profile associated with this filter.
  //
  // DANGER! Do not dereference this pointer! This class lives on the I/O thread
  // and the profile may only be used on the UI thread. It is used only for
  // determining which notifications to watch for.
  //
  // This is void* to prevent people from accidentally dereferencing it.
  // When registering for observers, cast to Profile*.
  void* profile_;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;

  // Object that should take care of audio related resource requests.
  scoped_refptr<AudioRendererHost> audio_renderer_host_;

  // Handles AppCache related messages.
  scoped_ptr<AppCacheDispatcherHost> app_cache_dispatcher_host_;

  // Handles DOM Storage related messages.
  scoped_refptr<DOMStorageDispatcherHost> dom_storage_dispatcher_host_;

  // Whether this process is used for off the record tabs.
  bool off_the_record_;

  DISALLOW_COPY_AND_ASSIGN(ResourceMessageFilter);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RESOURCE_MSG_FILTER_H_
