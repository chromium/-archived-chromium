// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_H_

#include "build/build_config.h"

#include <string>

#include "base/process.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/common/transport_dib.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/notification_registrar.h"
#include "webkit/api/public/WebCache.h"

class CommandLine;
class GURL;
class RendererMainThread;
class RenderWidgetHelper;
class TabContents;
class VisitedLinkUpdater;

namespace gfx {
class Size;
}

// Implements a concrete RenderProcessHost for the browser process for talking
// to actual renderer processes (as opposed to mocks).
//
// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.
//
// This object is refcounted so that it can release its resources when all
// hosts using it go away.
//
// This object communicates back and forth with the RenderProcess object
// running in the renderer process. Each RenderProcessHost and RenderProcess
// keeps a list of RenderView (renderer) and TabContents (browser) which
// are correlated with IDs. This way, the Views and the corresponding ViewHosts
// communicate through the two process objects.
class BrowserRenderProcessHost : public RenderProcessHost,
                                 public NotificationObserver {
 public:
  explicit BrowserRenderProcessHost(Profile* profile);
  ~BrowserRenderProcessHost();

  // RenderProcessHost implementation (public portion).
  virtual bool Init();
  virtual int GetNextRoutingID();
  virtual void CancelResourceRequests(int render_widget_id);
  virtual void CrossSiteClosePageACK(int new_render_process_host_id,
                                     int new_request_id);
  virtual bool WaitForPaintMsg(int render_widget_id,
                               const base::TimeDelta& max_delay,
                               IPC::Message* msg);
  virtual void ReceivedBadMessage(uint16 msg_type);
  virtual void WidgetRestored();
  virtual void WidgetHidden();
  virtual void AddWord(const std::wstring& word);
  virtual void AddVisitedLinks(const VisitedLinkCommon::Fingerprints& links);
  virtual void ResetVisitedLinks();
  virtual bool FastShutdownIfPossible();
  virtual bool SendWithTimeout(IPC::Message* msg, int timeout_ms);
  virtual TransportDIB* GetTransportDIB(TransportDIB::Id dib_id);

  // IPC::Channel::Sender via RenderProcessHost.
  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener via RenderProcessHost.
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  // If the a process has sent a message that cannot be decoded, it is deemed
  // corrupted and thus needs to be terminated using this call. This function
  // can be safely called from any thread.
  static void BadMessageTerminateProcess(uint16 msg_type,
                                         base::ProcessHandle renderer);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend class VisitRelayingRenderProcessHost;

  // Control message handlers.
  void OnPageContents(const GURL& url, int32 page_id,
                      const std::wstring& contents);
  void OnUpdatedCacheStats(const WebKit::WebCache::UsageStats& stats);
  void SuddenTerminationChanged(bool enabled);
  void OnExtensionAddListener(const std::string& event_name);
  void OnExtensionRemoveListener(const std::string& event_name);
  void OnExtensionCloseChannel(int port_id);

  // Initialize support for visited links. Send the renderer process its initial
  // set of visited links.
  void InitVisitedLinks();

  // Initialize support for user scripts. Send the renderer process its initial
  // set of scripts and listen for updates to scripts.
  void InitUserScripts();

  // Initialize support for extension APIs. Send the list of registered API
  // functions to thre renderer process.
  void InitExtensions();

  // Sends the renderer process a new set of user scripts.
  void SendUserScriptsUpdate(base::SharedMemory* shared_memory);

  // Gets a handle to the renderer process, normalizing the case where we were
  // started with --single-process.
  base::ProcessHandle GetRendererProcessHandle();

  // Callers can reduce the RenderProcess' priority.
  // Returns true if the priority is backgrounded; false otherwise.
  void SetBackgrounded(bool boost);

  NotificationRegistrar registrar_;

  // The count of currently visible widgets.  Since the host can be a container
  // for multiple widgets, it uses this count to determine when it should be
  // backgrounded.
  int32 visible_widgets_;

  // Does this process have backgrounded priority.
  bool backgrounded_;

  // Used to allow a RenderWidgetHost to intercept various messages on the
  // IO thread.
  scoped_refptr<RenderWidgetHelper> widget_helper_;

  // The host of audio renderers in the renderer process.
  scoped_refptr<AudioRendererHost> audio_renderer_host_;

  // A map of transport DIB ids to cached TransportDIBs
  std::map<TransportDIB::Id, TransportDIB*> cached_dibs_;
  enum {
    // This is the maximum size of |cached_dibs_|
    MAX_MAPPED_TRANSPORT_DIBS = 3,
  };

  // Map a transport DIB from its Id and return it. Returns NULL on error.
  TransportDIB* MapTransportDIB(TransportDIB::Id dib_id);

  void ClearTransportDIBCache();
  // This is used to clear our cache five seconds after the last use.
  base::DelayTimer<BrowserRenderProcessHost> cached_dibs_cleaner_;

  // Used in single-process mode.
  scoped_ptr<RendererMainThread> in_process_renderer_;

  // Buffer visited links and send them to to renderer.
  scoped_ptr<VisitedLinkUpdater> visited_link_updater_;

  // True iff the renderer is a child of a zygote process.
  bool zygote_child_;

  DISALLOW_COPY_AND_ASSIGN(BrowserRenderProcessHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_H_
