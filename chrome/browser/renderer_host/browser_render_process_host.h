// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_H_

#include <limits>
#include <set>
#include <vector>
#include <windows.h>

#include "base/id_map.h"
#include "base/object_watcher.h"
#include "base/rand_util.h"
#include "base/ref_counted.h"
#include "base/scoped_handle.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"

class PrefService;
class RenderWidgetHelper;
class WebContents;

namespace base {
class Thread;
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
// keeps a list of RenderView (renderer) and WebContents (browser) which
// are correlated with IDs. This way, the Views and the corresponding ViewHosts
// communicate through the two process objects.
class BrowserRenderProcessHost : public RenderProcessHost,
                                 public base::ObjectWatcher::Delegate,
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
  virtual bool FastShutdownIfPossible();

  // IPC::Channel::Sender via RenderProcessHost.
  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener via RenderProcessHost.
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);

  static void RegisterPrefs(PrefService* prefs);

  // If the a process has sent a message that cannot be decoded, it is deemed
  // corrupted and thus needs to be terminated using this call. This function
  // can be safely called from any thread.
  static void BadMessageTerminateProcess(uint16 msg_type, HANDLE renderer);

  // ObjectWatcher::Delegate
  virtual void OnObjectSignaled(HANDLE object);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // RenderProcessHost implementation (protected portion).
  virtual void Unregister();

  // Control message handlers.
  void OnPageContents(const GURL& url, int32 page_id,
                      const std::wstring& contents);
  // Clipboard messages
  void OnClipboardWriteHTML(const std::wstring& markup, const GURL& src_url);
  void OnClipboardWriteBookmark(const std::wstring& title, const GURL& url);
  void OnClipboardWriteBitmap(base::SharedMemoryHandle bitmap, gfx::Size size);
  void OnClipboardIsFormatAvailable(unsigned int format, bool* result);
  void OnClipboardReadText(std::wstring* result);
  void OnClipboardReadAsciiText(std::string* result);
  void OnClipboardReadHTML(std::wstring* markup, GURL* src_url);
  void OnUpdatedCacheStats(const CacheManager::UsageStats& stats);

  // Initialize support for visited links. Send the renderer process its initial
  // set of visited links.
  void InitVisitedLinks();

  // Initialize support for user scripts. Send the renderer process its initial
  // set of scripts and listen for updates to scripts.
  void InitUserScripts();

  // Sends the renderer process a new set of user scripts.
  void SendUserScriptsUpdate(base::SharedMemory* shared_memory);

  // Gets a handle to the renderer process, normalizing the case where we were
  // started with --single-process.
  base::ProcessHandle GetRendererProcessHandle();

  // Callers can reduce the RenderProcess' priority.
  // Returns true if the priority is backgrounded; false otherwise.
  void SetBackgrounded(bool boost);

  // Used to watch the renderer process handle.
  base::ObjectWatcher watcher_;

  // The count of currently visible widgets.  Since the host can be a container
  // for multiple widgets, it uses this count to determine when it should be
  // backgrounded.
  int32 visible_widgets_;

  // Does this process have backgrounded priority.
  bool backgrounded_;

  // Used to allow a RenderWidgetHost to intercept various messages on the
  // IO thread.
  scoped_refptr<RenderWidgetHelper> widget_helper_;

  DISALLOW_COPY_AND_ASSIGN(BrowserRenderProcessHost);
};

// Generates a unique channel name for a child renderer/plugin process.
// The "instance" pointer value is baked into the channel id.
inline std::wstring GenerateRandomChannelID(void* instance) {
  // Note: the string must start with the current process id, this is how
  // child processes determine the pid of the parent.
  // Build the channel ID.  This is composed of a unique identifier for the
  // parent browser process, an identifier for the renderer/plugin instance,
  // and a random component. We use a random component so that a hacked child
  // process can't cause denial of service by causing future named pipe creation
  // to fail.
  return StringPrintf(L"%d.%x.%d",
                      GetCurrentProcessId(), instance,
                      base::RandInt(0, std::numeric_limits<int>::max()));
}


#endif  // CHROME_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_H_

