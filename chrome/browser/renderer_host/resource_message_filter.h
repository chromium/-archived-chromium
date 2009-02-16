// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RESOURCE_MSG_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_RESOURCE_MSG_FILTER_H_

#include "base/clipboard.h"
#include "base/file_path.h"
#include "base/gfx/rect.h"
#include "base/gfx/native_widget_types.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "chrome/browser/net/resolve_proxy_msg_helper.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/common/modal_dialog_event.h"
#include "chrome/common/notification_observer.h"
#include "webkit/glue/cache_manager.h"

#if defined(OS_WIN)
#include <windows.h>
#else
// TODO(port): port ResourceDispatcherHost.
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

class AudioRendererHost;
class ClipboardService;
class Profile;
class RenderWidgetHelper;
class SpellChecker;
struct WebPluginInfo;

namespace printing {
class PrinterQuery;
class PrintJobManager;
}

namespace webkit_glue {
struct ScreenInfo;
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
                        int render_process_host_id,
                        Profile* profile,
                        RenderWidgetHelper* render_widget_helper,
                        SpellChecker* spellchecker);
  virtual ~ResourceMessageFilter();

  // IPC::ChannelProxy::MessageFilter methods:
  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelClosing();
  virtual bool OnMessageReceived(const IPC::Message& message);

  // ResourceDispatcherHost::Receiver methods:
  virtual bool Send(IPC::Message* message);

  // Access to the spell checker.
  SpellChecker* spellchecker() { return spellchecker_.get(); }

  int render_process_host_id() const { return render_process_host_id_;}

  base::ProcessHandle renderer_handle() const { return render_handle_;}

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  void OnMsgCreateWindow(int opener_id, bool user_gesture, int* route_id,
                         ModalDialogEvent* modal_dialog_event);
  void OnMsgCreateWidget(int opener_id, bool activatable, int* route_id);
  void OnRequestResource(const IPC::Message& msg, int request_id,
                         const ViewHostMsg_Resource_Request& request);
  void OnCancelRequest(int request_id);
  void OnClosePageACK(int new_render_process_host_id, int new_request_id);
  void OnDataReceivedACK(int request_id);
  void OnUploadProgressACK(int request_id);
  void OnSyncLoad(int request_id,
                  const ViewHostMsg_Resource_Request& request,
                  IPC::Message* result_message);
  void OnSetCookie(const GURL& url, const GURL& policy_url,
                   const std::string& cookie);
  void OnGetCookies(const GURL& url, const GURL& policy_url,
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

  void OnGetScreenInfo(gfx::NativeViewId window,
                       webkit_glue::ScreenInfo* results);
  void OnGetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins);
  void OnGetPluginPath(const GURL& url,
                       const std::string& mime_type,
                       const std::string& clsid,
                       FilePath* filename,
                       std::string* actual_mime_type);
  void OnOpenChannelToPlugin(const GURL& url,
                             const std::string& mime_type,
                             const std::string& clsid,
                             const std::wstring& locale,
                             IPC::Message* reply_msg);
  void OnDownloadUrl(const IPC::Message& message,
                     const GURL& url,
                     const GURL& referrer);
  void OnSpellCheck(const std::wstring& word,
                    IPC::Message* reply_msg);
  void OnDnsPrefetch(const std::vector<std::string>& hostnames);
  void OnReceiveContextMenuMsg(const IPC::Message& msg);
  // Clipboard messages
  void OnClipboardWriteObjects(const Clipboard::ObjectMap& objects);
  void OnClipboardIsFormatAvailable(unsigned int format, bool* result);
  void OnClipboardReadText(std::wstring* result);
  void OnClipboardReadAsciiText(std::string* result);
  void OnClipboardReadHTML(std::wstring* markup, GURL* src_url);
#if defined(OS_WIN)
  void OnGetWindowRect(gfx::NativeViewId window, gfx::Rect *rect);
  void OnGetRootWindowRect(gfx::NativeViewId window, gfx::Rect *rect);
#endif
  void OnGetMimeTypeFromExtension(const FilePath::StringType& ext,
                                  std::string* mime_type);
  void OnGetMimeTypeFromFile(const FilePath& file_path,
                             std::string* mime_type);
  void OnGetPreferredExtensionForMimeType(const std::string& mime_type,
                                          FilePath::StringType* ext);
  void OnGetCPBrowsingContext(uint32* context);
  void OnDuplicateSection(base::SharedMemoryHandle renderer_handle,
                          base::SharedMemoryHandle* browser_handle);
  void OnResourceTypeStats(const CacheManager::ResourceTypeStats& stats);

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
                       IPC::Message* reply_msg);
  void OnScriptedPrintReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);
#endif

  // We have our own clipboard service because we want to access the clipboard
  // on the IO thread instead of forwarding (possibly synchronous) messages to
  // the UI thread.
  // This instance of the clipboard service should be accessed only on the IO
  // thread.
  static ClipboardService* GetClipboardService();

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
  int render_process_host_id_;

  // Our spellchecker object.
  scoped_refptr<SpellChecker> spellchecker_;

  // Helper class for handling PluginProcessHost_ResolveProxy messages (manages
  // the requests to the proxy service).
  ResolveProxyMsgHelper resolve_proxy_msg_helper_;

  // Process handle of the renderer process.
  base::ProcessHandle render_handle_;

  // Contextual information to be used for requests created here.
  scoped_refptr<URLRequestContext> request_context_;

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

  DISALLOW_COPY_AND_ASSIGN(ResourceMessageFilter);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RESOURCE_MSG_FILTER_H_
