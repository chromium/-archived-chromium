// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_PROCESS_HOST_H_
#define CHROME_BROWSER_PLUGIN_PROCESS_HOST_H_

#include <vector>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/object_watcher.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/resource_message_filter.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/browser/resource_message_filter.h"

class PluginService;
class PluginProcessHost;
class ResourceDispatcherHost;
class URLRequestContext;
struct ViewHostMsg_Resource_Request;
class GURL;

// Represents the browser side of the browser <--> plugin communication
// channel.  Different plugins run in their own process, but multiple instances
// of the same plugin run in the same process.  There will be one
// PluginProcessHost per plugin process, matched with a corresponding
// PluginProcess running in the plugin process.  The browser is responsible for
// starting the plugin process when a plugin is created that doesn't already
// have a process.  After that, most of the communication is directly between
// the renderer and plugin processes.
class PluginProcessHost : public IPC::Channel::Listener,
                          public IPC::Message::Sender,
                          public base::ObjectWatcher::Delegate {
 public:
  PluginProcessHost(PluginService* plugin_service);
  ~PluginProcessHost();

  // Initialize the new plugin process, returning true on success. This must
  // be called before the object can be used. If plugin_path is the
  // ActiveX-shim, then activex_clsid is the class id of ActiveX control,
  // otherwise activex_clsid is ignored.
  bool Init(const FilePath& plugin_path,
            const std::string& activex_clsid,
            const std::wstring& locale);

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  // ObjectWatcher::Delegate implementation:
  virtual void OnObjectSignaled(HANDLE object);

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  // Getter to the process, may return NULL if there is no connection.
  HANDLE process() { return process_.handle(); }

  // Tells the plugin process to create a new channel for communication with a
  // renderer.  When the plugin process responds with the channel name,
  // reply_msg is used to send the name to the renderer.
  void OpenChannelToPlugin(ResourceMessageFilter* renderer_message_filter,
                           const std::string& mime_type,
                           IPC::Message* reply_msg);

  const FilePath& plugin_path() const { return plugin_path_; }

  // Sends the reply to an open channel request to the renderer with the given
  // channel name.
  static void ReplyToRenderer(ResourceMessageFilter* renderer_message_filter,
                              const std::wstring& channel,
                              const FilePath& plugin_path,
                              IPC::Message* reply_msg);

  // This function is called on the IO thread once we receive a reply from the
  // modal HTML dialog (in the form of a JSON string). This function forwards
  // that reply back to the plugin that requested the dialog.
  void OnModalDialogResponse(const std::string& json_retval,
                             IPC::Message* sync_result);

  // Shuts down the current plugin process instance.
  void Shutdown();

 private:
  friend class PluginResolveProxyHelper;

  // Sends a message to the plugin process to request creation of a new channel
  // for the given mime type.
  void RequestPluginChannel(ResourceMessageFilter* renderer_message_filter,
                            const std::string& mime_type,
                            IPC::Message* reply_msg);
  // Message handlers.
  void OnChannelCreated(int process_id, const std::wstring& channel_name);
  void OnDownloadUrl(const std::string& url, int source_pid,
                     HWND caller_window);
  void OnGetPluginFinderUrl(std::string* plugin_finder_url);
  void OnRequestResource(const IPC::Message& message,
                         int request_id,
                         const ViewHostMsg_Resource_Request& request);
  void OnCancelRequest(int request_id);
  void OnDataReceivedACK(int request_id);
  void OnUploadProgressACK(int request_id);
  void OnSyncLoad(int request_id,
                  const ViewHostMsg_Resource_Request& request,
                  IPC::Message* sync_result);
  void OnGetCookies(uint32 request_context, const GURL& url,
                    std::string* cookies);
  void OnResolveProxy(const GURL& url, IPC::Message* reply_msg);
  void OnPluginShutdownRequest();
  void OnPluginMessage(const std::vector<uint8>& data);
  void OnGetPluginDataDir(std::wstring* retval);
  void OnCreateWindow(HWND parent, IPC::Message* reply_msg);
  void OnDestroyWindow(HWND window);

  struct ChannelRequest {
    ChannelRequest(ResourceMessageFilter* renderer_message_filter,
                    const std::string& m, IPC::Message* r) :
        renderer_message_filter_(renderer_message_filter), mime_type(m),
        reply_msg(r) { }
    std::string mime_type;
    IPC::Message* reply_msg;
    scoped_refptr<ResourceMessageFilter> renderer_message_filter_;
  };

  // These are channel requests that we are waiting to send to the
  // plugin process once the channel is opened.
  std::vector<ChannelRequest> pending_requests_;

  // These are the channel requests that we have already sent to
  // the plugin process, but haven't heard back about yet.
  std::vector<ChannelRequest> sent_requests_;

  // The handle to our plugin process.
  base::Process process_;

  // Used to watch the plugin process handle.
  base::ObjectWatcher watcher_;

  // true while we're waiting the channel to be opened.  In the meantime,
  // plugin instance requests will be buffered.
  bool opening_channel_;

  // The IPC::Channel.
  scoped_ptr<IPC::Channel> channel_;

  // IPC Channel's id.
  std::wstring channel_id_;

  // Path to the file of that plugin.
  FilePath plugin_path_;

  PluginService* plugin_service_;

  ResourceDispatcherHost* resource_dispatcher_host_;

  // This RevocableStore prevents ResolveProxy completion callbacks from
  // accessing a deleted PluginProcessHost (since we do not cancel the
  // in-progress resolve requests during destruction).
  RevocableStore revocable_store_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginProcessHost);
};

#endif  // CHROME_BROWSER_PLUGIN_PROCESS_HOST_H_

