// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_H_

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "chrome/common/ipc_sync_channel.h"

class Profile;

// Virtual interface that represents the browser side of the browser <->
// renderer communication channel. There will generally be one
// RenderProcessHost per renderer process.
//
// The concrete implementation of this class for normal use is the
// BrowserRenderProcessHost. It may also be implemented by a testing interface
// for mocking purposes.
class RenderProcessHost : public IPC::Channel::Sender,
                          public IPC::Channel::Listener {
 public:
  typedef IDMap<RenderProcessHost>::const_iterator iterator;

  RenderProcessHost(Profile* profile);
  virtual ~RenderProcessHost();

  // Returns the user profile associated with this renderer process.
  Profile* profile() const { return profile_; }

  // Returns the unique identifier for this host. This can be used later in
  // a call to FromID() to get back to this object (this is used to avoid
  // sending non-threadsafe pointers to other threads).
  int host_id() const { return host_id_; }

  // Returns the process object associated with the child process. In certain
  // tests or single-process mode, this will actually represent the current
  // process.
  const base::Process& process() const { return process_; }

  // May return NULL if there is no connection.
  IPC::SyncChannel* channel() { return channel_.get(); }

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

  // Allows iteration over this RenderProcessHost's RenderViewHost listeners.
  // Use from UI thread only.
  typedef IDMap<IPC::Channel::Listener>::const_iterator listeners_iterator;
  listeners_iterator listeners_begin() {
    return listeners_.begin();
  }
  listeners_iterator listeners_end() {
    return listeners_.end();
  }

  IPC::Channel::Listener* GetListenerByID(int routing_id) {
    return listeners_.Lookup(routing_id);
  }

  // Called to inform the render process host of a new "max page id" for a
  // render view host.  The render process host computes the largest page id
  // across all render view hosts and uses the value when it needs to
  // initialize a new renderer in place of the current one.
  void UpdateMaxPageID(int32 page_id);

  // Virtual interface ---------------------------------------------------------

  // Initialize the new renderer process, returning true on success. This must
  // be called once before the object can be used, but can be called after
  // that with no effect. Therefore, if the caller isn't sure about whether
  // the process has been created, it should just call Init().
  virtual bool Init() = 0;

  // Gets the next available routing id.
  virtual int GetNextRoutingID() = 0;

  // Called on the UI thread to cancel any outstanding resource requests for
  // the specified render widget.
  virtual void CancelResourceRequests(int render_widget_id) = 0;

  // Called on the UI thread to simulate a ClosePage_ACK message to the
  // ResourceDispatcherHost.  Necessary for a cross-site request, in the case
  // that the original RenderViewHost is not live and thus cannot run an
  // onunload handler.
  virtual void CrossSiteClosePageACK(int new_render_process_host_id,
                                     int new_request_id) = 0;

  // Called on the UI thread to wait for the next PaintRect message for the
  // specified render widget.  Returns true if successful, and the msg out-
  // param will contain a copy of the received PaintRect message.
  virtual bool WaitForPaintMsg(int render_widget_id,
                               const base::TimeDelta& max_delay,
                               IPC::Message* msg) = 0;

  // Called when a received message cannot be decoded.
  virtual void ReceivedBadMessage(uint16 msg_type) = 0;

  // Track the count of visible widgets. Called by listeners to register and
  // unregister visibility.
  virtual void WidgetRestored() = 0;
  virtual void WidgetHidden() = 0;

  // Add a word in the spellchecker.
  virtual void AddWord(const std::wstring& word) = 0;

  // Try to shutdown the associated renderer process as fast as possible.
  // If this renderer has any RenderViews with unload handlers, then this
  // function does nothing.  The current implementation uses TerminateProcess.
  // Returns True if it was able to do fast shutdown.
  virtual bool FastShutdownIfPossible() = 0;

  // Static management functions -----------------------------------------------

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

  // Allows iteration over all the RenderProcessHosts in the browser. Note
  // that each host may not be active, and therefore may have NULL channels.
  // This is just a standard STL iterator, so it is not valid if the list
  // of RenderProcessHosts changes between iterations.
  static iterator begin();
  static iterator end();
  static size_t size();  // TODO(brettw) rename this, it's very unclear.

  // Returns the RenderProcessHost given its ID.  Returns NULL if the ID does
  // not correspond to a live RenderProcessHost.
  static RenderProcessHost* FromID(int render_process_id);

  // Returns true if the caller should attempt to use an existing
  // RenderProcessHost rather than creating a new one.
  static bool ShouldTryToUseExistingProcessHost();

  // Get an existing RenderProcessHost associated with the given profile, if
  // possible.  The renderer process is chosen randomly from the
  // processes associated with the given profile.
  // Returns NULL if no suitable renderer process is available.
  static RenderProcessHost* GetExistingProcessHost(Profile* profile);

 protected:
  // Unregister this object from all globals that reference it.
  // This would naturally be part of the destructor, but we destruct
  // asynchronously.
  //
  // This can be overridden by derived classes to add their own unregister code,
  // but you should be sure to always call this one AT THE END of your own.
  virtual void Unregister();

  base::Process process_;

  // A proxy for our IPC::Channel that lives on the IO thread (see
  // browser_process.h)
  scoped_ptr<IPC::SyncChannel> channel_;

  // the registered listeners. When this list is empty or all NULL, we should
  // delete ourselves
  IDMap<IPC::Channel::Listener> listeners_;

  // The maximum page ID we've ever seen from the renderer process.
  int32 max_page_id_;

  // Indicates whether we have notified that the process has terminated. We
  // only want to send this out once.
  bool notified_termination_;

 private:
  int host_id_;

  Profile* profile_;

  // set of listeners that expect the renderer process to close
  std::set<int> listeners_expecting_close_;

  // See getter above.
  static bool run_renderer_in_process_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHost);
};

// Factory object for RenderProcessHosts. Using this factory allows tests to
// swap out a different one to use a TestRenderProcessHost.
class RenderProcessHostFactory {
 public:
  virtual ~RenderProcessHostFactory() {}
  virtual RenderProcessHost* CreateRenderProcessHost(
      Profile* profile) const = 0;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_PROCESS_HOST_H_
