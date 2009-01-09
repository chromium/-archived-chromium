// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_RESOURCE_MSG_FILTER_H__
#define CHROME_BROWSER_RENDERER_RESOURCE_MSG_FILTER_H__

#include "base/clipboard.h"
#include "base/file_path.h"
#include "base/gfx/rect.h"
#include "base/gfx/native_widget_types.h"
#include "base/ref_counted.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/common/notification_service.h"
#include "webkit/glue/cache_manager.h"

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
                              public NotificationObserver {
 public:
  // Create the filter.
  // Note:  because the lifecycle of the ResourceMessageFilter is not
  //        tied to the lifecycle of the object which created it, the
  //        ResourceMessageFilter is 'given' ownership of the spellchecker
  //        object and must clean it up on exit.
  ResourceMessageFilter(ResourceDispatcherHost* resource_dispatcher_host,
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

  HANDLE renderer_handle() const { return render_handle_;}
  
  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  void OnMsgCreateWindow(int opener_id, bool user_gesture, int* route_id,
                         HANDLE* modal_dialog_event);
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

  // Cache fonts for the renderer. See ResourceMessageFilter::OnLoadFont
  // implementation for more details
  void OnLoadFont(LOGFONT font);
  void OnGetScreenInfo(gfx::NativeView window,
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
  void OnGetWindowRect(HWND window, gfx::Rect *rect);
  void OnGetRootWindowRect(HWND window, gfx::Rect *rect);
  void OnGetRootWindowResizerRect(HWND window, gfx::Rect *rect);
  void OnGetMimeTypeFromExtension(const std::wstring& ext,
                                  std::string* mime_type);
  void OnGetMimeTypeFromFile(const std::wstring& file_path,
                             std::string* mime_type);
  void OnGetPreferredExtensionForMimeType(const std::string& mime_type,
                                          std::wstring* ext);
  void OnGetCPBrowsingContext(uint32* context);
  void OnDuplicateSection(base::SharedMemoryHandle renderer_handle,
                          base::SharedMemoryHandle* browser_handle);
  void OnResourceTypeStats(const CacheManager::ResourceTypeStats& stats);

  // A javascript code requested to print the current page. This is done in two
  // steps and this is the first step. Get the print setting right here
  // synchronously. It will hang the I/O completely.
  void OnGetDefaultPrintSettings(IPC::Message* reply_msg);
  void OnGetDefaultPrintSettingsReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);
  // A javascript code requested to print the current page. The renderer host
  // have to show to the user the print dialog and returns the selected print
  // settings.
  void OnScriptedPrint(HWND host_window,
                       int cookie,
                       int expected_pages_count,
                       IPC::Message* reply_msg);
  void OnScriptedPrintReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);

  // We have our own clipboard service because we want to access the clipboard
  // on the IO thread instead of forwarding (possibly synchronous) messages to
  // the UI thread.
  // This instance of the clipboard service should be accessed only on the IO
  // thread.
  static ClipboardService* GetClipboardService();

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

  HANDLE render_handle_;

  // Contextual information to be used for requests created here.
  scoped_refptr<URLRequestContext> request_context_;

  // Save the profile pointer so that notification observer can be added.
  Profile* profile_;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;
};

#endif // CHROME_BROWSER_RENDERER_RESOURCE_MSG_FILTER_H__
