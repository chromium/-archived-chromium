// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_RENDER_WIDGET_HELPER_H__
#define CHROME_BROWSER_RENDER_WIDGET_HELPER_H__

#include <hash_map>

#include "base/ref_counted.h"
#include "base/lock.h"

namespace IPC {
class Message;
}

class MessageLoop;
class ResourceDispatcherHost;
class TimeDelta;

// Instantiated per RenderProcessHost to provide various optimizations on
// behalf of a RenderWidgetHost.  This class bridges between the IO thread
// where the RenderProcessHost's MessageFilter lives and the UI thread where
// the RenderWidgetHost lives.
//
//
// OPTIMIZED RESIZE
//
//   RenderWidgetHelper is used to implement optimized resize.  When the
//   RenderWidgetHost is resized, it sends a Resize message to its RenderWidget
//   counterpart in the renderer process.  The RenderWidget generates a
//   PaintRect message in response to the Resize message, and it sets the
//   IS_RESIZE_ACK flag in the PaintRect message to true.
//
//   Back in the browser process, when the RenderProcessHost's MessageFilter
//   sees a PaintRect message, it directs it to the RenderWidgetHelper by
//   calling the DidReceivePaintMsg method.  That method stores the data for
//   the PaintRect message in a map, where it can be directly accessed by the
//   RenderWidgetHost on the UI thread during a call to RenderWidgetHost's
//   GetBackingStore method.
//
//   When the RenderWidgetHost's GetBackingStore method is called, it first
//   checks to see if it is waiting for a resize ack.  If it is, then it calls
//   the RenderWidgetHelper's WaitForPaintMsg to check if there is already a
//   resulting PaintRect message (or to wait a short amount of time for one to
//   arrive).  The main goal of this mechanism is to short-cut the usual way in
//   which IPC messages are proxied over to the UI thread via InvokeLater.
//   This approach is necessary since window resize is followed up immediately
//   by a request to repaint the window.
//
//
// OPTIMIZED TAB SWITCHING
//
//   When a RenderWidgetHost is in a background tab, it is flagged as hidden.
//   This causes the corresponding RenderWidget to stop sending PaintRect
//   messages. The RenderWidgetHost also discards its backingstore when it is
//   hidden, which helps free up memory.  As a result, when a RenderWidgetHost
//   is restored, it can be momentarily without a backingstore.  (Restoring a
//   RenderWidgetHost results in a WasRestored message being sent to the
//   RenderWidget, which triggers a full PaintRect message.)  This can lead to
//   an observed rendering glitch as the WebContents will just have to fill
//   white overtop the RenderWidgetHost until the RenderWidgetHost receives a
//   PaintRect message to refresh its backingstore.
//
//   To avoid this 'white flash', the RenderWidgetHost again makes use of the
//   RenderWidgetHelper's WaitForPaintMsg method.  When the RenderWidgetHost's
//   GetBackingStore method is called, it will call WaitForPaintMsg if it has
//   no backingstore.
//
class RenderWidgetHelper :
    public base::RefCountedThreadSafe<RenderWidgetHelper> {
 public:
  RenderWidgetHelper(int render_process_id);
  ~RenderWidgetHelper();

  // Gets the next available routing id.  This is thread safe.
  int GetNextRoutingID();

  // Sets whether popup blocking is enabled or not.
  void set_block_popups(bool block) { block_popups_ = block; }


  // UI THREAD ONLY -----------------------------------------------------------

  // Called on the UI thread to cancel any outstanding resource requests for
  // the specified render widget.
  void CancelResourceRequests(int render_widget_id);

  // Called on the UI thread to simulate a ClosePage_ACK message to the
  // ResourceDispatcherHost.  Necessary for a cross-site request, in the case
  // that the original RenderViewHost is not live and thus cannot run an
  // onunload handler.
  void CrossSiteClosePageACK(int new_render_process_host_id,
                             int new_request_id);

  // Called on the UI thread to wait for the next PaintRect message for the
  // specified render widget.  Returns true if successful, and the msg out-
  // param will contain a copy of the received PaintRect message.
  bool WaitForPaintMsg(int render_widget_id, const TimeDelta& max_delay,
                       IPC::Message* msg);


  // IO THREAD ONLY -----------------------------------------------------------

  // Called on the IO thread when a PaintRect message is received.
  void DidReceivePaintMsg(const IPC::Message& msg);

  MessageLoop* ui_loop() { return ui_loop_; }

  void CreateView(int opener_id, bool user_gesture, int* route_id,
                  HANDLE* modal_dialog_event, HANDLE render_process);
  void CreateWidget(int opener_id, int* route_id);

 private:
  // A class used to proxy a paint message.  PaintMsgProxy objects are created
  // on the IO thread and destroyed on the UI thread.
  class PaintMsgProxy;
  friend PaintMsgProxy;

  // Map from render_widget_id to live PaintMsgProxy instance.
  typedef stdext::hash_map<int, PaintMsgProxy*> PaintMsgProxyMap;

  // Called on the UI thread to dispatch a paint message if necessary.
  void OnDispatchPaintMsg(PaintMsgProxy* proxy);

  // Called on the UI thread to send a message to the RenderProcessHost.
  void OnSimulateReceivedMessage(const IPC::Message& message);

  // Called on the IO thread to cancel resource requests for the render widget.
  void OnCancelResourceRequests(ResourceDispatcherHost* dispatcher,
                                int render_widget_id);

  // Called on the IO thread to resume a cross-site response.
  void OnCrossSiteClosePageACK(ResourceDispatcherHost* dispatcher,
                               int new_render_process_host_id,
                               int new_request_id);

  // A map of live paint messages.  Must hold pending_paints_lock_ to access.
  // The PaintMsgProxy objects are not owned by this map.  (See PaintMsgProxy
  // for details about how the lifetime of instances are managed.)
  PaintMsgProxyMap pending_paints_;
  Lock pending_paints_lock_;

  int render_process_id_;
  MessageLoop* ui_loop_;

  // Event used to implement WaitForPaintMsg.
  HANDLE event_;

  // The next routing id to use.
  LONG next_routing_id_;

  // Whether popup blocking is enabled or not.
  bool block_popups_;

  DISALLOW_EVIL_CONSTRUCTORS(RenderWidgetHelper);
};

#endif  // CHROME_BROWSER_RENDER_WIDGET_HELPER_H__
