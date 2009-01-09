// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDER_PROCESS_HOST_H_
#define CHROME_BROWSER_RENDER_PROCESS_HOST_H_

#include <limits>
#include <set>
#include <vector>
#include <windows.h>

#include "base/id_map.h"
#include "base/object_watcher.h"
#include "base/process.h"
#include "base/rand_util.h"
#include "base/ref_counted.h"
#include "base/scoped_handle.h"
#include "base/scoped_ptr.h"
#include "chrome/common/ipc_sync_channel.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"

class PrefService;
class Profile;
class RenderWidgetHelper;
class WebContents;

namespace base {
class Thread;
}

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
class RenderProcessHost : public IPC::Channel::Listener,
                          public IPC::Channel::Sender,
                          public base::ObjectWatcher::Delegate,
                          public NotificationObserver {
 public:
  // Returns the RenderProcessHost given its ID.  Returns NULL if the ID does
  // not correspond to a live RenderProcessHost.
  static RenderProcessHost* FromID(int render_process_id);

  explicit RenderProcessHost(Profile* profile);
  ~RenderProcessHost();

  // Flag to run the renderer in process.  This is primarily
  // for debugging purposes.  When running "in process", the
  // browser maintains a single RenderProcessHost which communicates
  // to a RenderProcess which is instantiated in the same process
  // with the Browser.  All IPC between the Browser and the
  // Renderer is the same, it's just not crossing a process boundary.
  static bool run_renderer_in_process() {
    return run_renderer_in_process_;
  }
  static void set_run_renderer_in_process(bool value) {
    run_renderer_in_process_ = value;
  }

  static void RegisterPrefs(PrefService* prefs);

  // If the a process has sent a message that cannot be decoded, it is deemed
  // corrupted and thus needs to be terminated using this call. This function
  // can be safely called from any thread.
  static void BadMessageTerminateProcess(uint16 msg_type, HANDLE renderer);

  // Called when a received message cannot be decoded.
  void ReceivedBadMessage(uint16 msg_type) {
    BadMessageTerminateProcess(msg_type, process_.handle());
  }

  // Initialize the new renderer process, returning true on success. This must
  // be called once before the object can be used, but can be called after
  // that with no effect. Therefore, if the caller isn't sure about whether
  // the process has been created, it should just call Init().
  bool Init();

  // Used for refcounting, each holder of this object must Attach and Release
  // just like it would for a COM object. This object should be allocated on
  // the heap; when no listeners own it any more, it will delete itself.
  void Attach(IPC::Channel::Listener* listener, int routing_id);

  // See Attach()
  void Release(int listener_id);

  // Listeners should call this when they've sent a "Close" message and
  // they're waiting for a "Close_ACK", so that if the renderer process
  // goes away we'll know that it was intentional rather than a crash.
  void ReportExpectingClose(int32 listener_id);

  // May return NULL if there is no connection.
  IPC::SyncChannel* channel() {
    return channel_.get();
  }

  const base::Process& process() const {
    return process_;
  }

  // Try to shutdown the associated renderer process as fast as possible.
  // If this renderer has any RenderViews with unload handlers, then this
  // function does nothing.  The current implementation uses TerminateProcess.
  // Returns True if it was able to do fast shutdown.
  bool FastShutdownIfPossible();

  IPC::Channel::Listener* GetListenerByID(int routing_id) {
    return listeners_.Lookup(routing_id);
  }

  // Called to inform the render process host of a new "max page id" for a
  // render view host.  The render process host computes the largest page id
  // across all render view hosts and uses the value when it needs to
  // initialize a new renderer in place of the current one.
  void UpdateMaxPageID(int32 page_id);

  // Called to simulate a ClosePage_ACK message to the ResourceDispatcherHost.
  // Necessary for a cross-site request, in the case that the original
  // RenderViewHost is not live and thus cannot run an onunload handler.
  void CrossSiteClosePageACK(int new_render_process_host_id,
                             int new_request_id);

  // IPC channel listener
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);

  // ObjectWatcher::Delegate
  virtual void OnObjectSignaled(HANDLE object);

  // IPC::Channel::Sender callback
  virtual bool Send(IPC::Message* msg);

  // Allows iteration over all the RenderProcessHosts in the browser. Note
  // that each host may not be active, and therefore may have NULL channels.
  // This is just a standard STL iterator, so it is not valid if the list
  // of RenderProcessHosts changes between iterations.
  typedef IDMap<RenderProcessHost>::const_iterator iterator;
  static iterator begin();
  static iterator end();
  static size_t size();

  // Allows iteration over this RenderProcessHost's RenderViewHost listeners.
  // Use from UI thread only.
  typedef IDMap<IPC::Channel::Listener>::const_iterator listeners_iterator;
  listeners_iterator listeners_begin() {
    return listeners_.begin();
  }
  listeners_iterator listeners_end() {
    return listeners_.end();
  }

  // Returns true if the caller should attempt to use an existing
  // RenderProcessHost rather than creating a new one.
  static bool ShouldTryToUseExistingProcessHost();

  // Get an existing RenderProcessHost associated with the given profile, if
  // possible.  The renderer process is chosen randomly from the
  // processes associated with the given profile.
  // Returns NULL if no suitable renderer process is available.
  static RenderProcessHost* GetExistingProcessHost(Profile* profile);

  int host_id() const { return host_id_; }

  // Returns the user profile associated with this renderer process.
  Profile* profile() const { return profile_; }

  RenderWidgetHelper* widget_helper() const { return widget_helper_; }

  // Track the count of visible widgets.  Called by listeners
  // to register/unregister visibility.
  void WidgetRestored();
  void WidgetHidden();

  // Add a word in the spellchecker.
  void AddWord(const std::wstring& word);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // control message handlers
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

  // Unregister this object from all globals that reference it.
  // This would naturally be part of the destructor, but we destruct
  // asynchronously.
  void Unregister();

  // the registered listeners. When this list is empty or all NULL, we should
  // delete ourselves
  IDMap<IPC::Channel::Listener> listeners_;

  // set of listeners that expect the renderer process to close
  std::set<int> listeners_expecting_close_;

  // A proxy for our IPC::Channel that lives on the IO thread (see
  // browser_process.h)
  scoped_ptr<IPC::SyncChannel> channel_;

  // Our renderer process.
  base::Process process_;

  // Used to watch the renderer process handle.
  base::ObjectWatcher watcher_;

  // The profile associated with this renderer process.
  Profile* profile_;

  // Our ID into the IDMap.
  int host_id_;

  // The maximum page ID we've ever seen from the renderer process.
  int32 max_page_id_;

  // The count of currently visible widgets.  Since the host can be a container
  // for multiple widgets, it uses this count to determine when it should be
  // backgrounded.
  int32 visible_widgets_;

  // Does this process have backgrounded priority.
  bool backgrounded_;

  // Used to allow a RenderWidgetHost to intercept various messages on the
  // IO thread.
  scoped_refptr<RenderWidgetHelper> widget_helper_;

  // Whether we have notified that the process has terminated.
  bool notified_termination_;

  static bool run_renderer_in_process_;

  DISALLOW_EVIL_CONSTRUCTORS(RenderProcessHost);
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


#endif  // CHROME_BROWSER_RENDER_PROCESS_HOST_H_

