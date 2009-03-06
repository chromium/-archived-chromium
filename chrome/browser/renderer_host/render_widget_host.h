// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_H_

#include <queue>
#include <vector>

#include "base/gfx/size.h"
#include "base/timer.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "webkit/glue/webinputevent.h"

namespace gfx {
class Rect;
}

class BackingStore;
class PaintObserver;
class RenderProcessHost;
class RenderWidgetHostView;
class TransportDIB;
class WebInputEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebCursor;
struct ViewHostMsg_PaintRect_Params;
struct ViewHostMsg_ScrollRect_Params;

// This class manages the browser side of a browser<->renderer HWND connection.
// The HWND lives in the browser process, and windows events are sent over
// IPC to the corresponding object in the renderer.  The renderer paints into
// shared memory, which we transfer to a backing store and blit to the screen
// when Windows sends us a WM_PAINT message.
//
// How Shutdown Works
//
// There are two situations in which this object, a RenderWidgetHost, can be
// instantiated:
//
// 1. By a WebContents as the communication conduit for a rendered web page.
//    The WebContents instantiates a derived class: RenderViewHost.
// 2. By a WebContents as the communication conduit for a select widget. The
//    WebContents instantiates the RenderWidgetHost directly.
//
// For every WebContents there are several objects in play that need to be
// properly destroyed or cleaned up when certain events occur.
//
// - WebContents - the TabContents itself, and its associated HWND.
// - RenderViewHost - representing the communication conduit with the child
//   process.
// - RenderWidgetHostView - the view of the web page content, message handler,
//   and plugin root.
//
// Normally, the WebContents contains a child RenderWidgetHostView that renders
// the contents of the loaded page. It has a WS_CLIPCHILDREN style so that it
// does no painting of its own.
//
// The lifetime of the RenderWidgetHostView is tied to the render process. If
// the render process dies, the RenderWidgetHostView goes away and all
// references to it must become NULL. If the WebContents finds itself without a
// RenderWidgetHostView, it paints Sad Tab instead.
//
// RenderViewHost (a RenderWidgetHost subclass) is the conduit used to
// communicate with the RenderView and is owned by the WebContents. If the
// render process crashes, the RenderViewHost remains and restarts the render
// process if needed to continue navigation.
//
// The WebContents is itself owned by the NavigationController in which it
// resides.
//
// Some examples of how shutdown works:
//
// When a tab is closed (either by the user, the web page calling window.close,
// etc) the TabStrip destroys the associated NavigationController, which calls
// Destroy on each TabContents it owns.
//
// For a WebContents, its Destroy method tells the RenderViewHost to
// shut down the render process and die.
//
// When the render process is destroyed it destroys the View: the
// RenderWidgetHostView, which destroys its HWND and deletes that object.
//
// For select popups, the situation is a little different. The RenderWidgetHost
// associated with the select popup owns the view and itself (is responsible
// for destroying itself when the view is closed). The WebContents's only
// responsibility is to select popups is to create them when it is told to. When
// the View is destroyed via an IPC message (for when WebCore destroys the
// popup, e.g. if the user selects one of the options), or because
// WM_CANCELMODE is received by the view, the View schedules the destruction of
// the render process. However in this case since there's no WebContents
// container, when the render process is destroyed, the RenderWidgetHost just
// deletes itself, which is safe because no one else should have any references
// to it (the WebContents does not).
//
// It should be noted that the RenderViewHost, not the RenderWidgetHost,
// handles IPC messages relating to the render process going away, since the
// way a RenderViewHost (WebContents) handles the process dying is different to
// the way a select popup does. As such the RenderWidgetHostView handles these
// messages for select popups. This placement is more out of convenience than
// anything else. When the view is live, these messages are forwarded to it by
// the RenderWidgetHost's IPC message map.
//
class RenderWidgetHost : public IPC::Channel::Listener {
 public:
  // An interface that gets called whenever a paint occurs.
  // Used in performance tests.
  class PaintObserver {
   public:
    virtual ~PaintObserver() {}
    virtual void RenderWidgetHostDidPaint(RenderWidgetHost* rwh) = 0;
  };

  // routing_id can be MSG_ROUTING_NONE, in which case the next available
  // routing id is taken from the RenderProcessHost.
  RenderWidgetHost(RenderProcessHost* process, int routing_id);
  virtual ~RenderWidgetHost();

  // Gets/Sets the View of this RenderWidgetHost. Can be NULL, e.g. if the
  // RenderWidget is being destroyed or the render process crashed. You should
  // never cache this pointer since it can become NULL if the renderer crashes,
  // instead you should always ask for it using the accessor.
  void set_view(RenderWidgetHostView* view) { view_ = view; }
  RenderWidgetHostView* view() const { return view_; }

  RenderProcessHost* process() const { return process_; }
  int routing_id() const { return routing_id_; }

  // Set the PaintObserver on this object. Takes ownership.
  void set_paint_observer(PaintObserver* paint_observer) {
    paint_observer_.reset(paint_observer);
  }

  // Called when a renderer object already been created for this host, and we
  // just need to be attached to it. Used for window.open, <select> dropdown
  // menus, and other times when the renderer initiates creating an object.
  virtual void Init();

  // Tells the renderer to die and then calls Destroy().
  virtual void Shutdown();

  // Manual RTTI FTW. We are not hosting a web page.
  virtual bool IsRenderView() { return false; }

  // IPC::Channel::Listener
  virtual void OnMessageReceived(const IPC::Message& msg);

  // Sends a message to the corresponding object in the renderer.
  bool Send(IPC::Message* msg);

  // Called to notify the RenderWidget that it has been hidden or restored from
  // having been hidden.
  void WasHidden();
  void WasRestored();

  // Called to notify the RenderWidget that it has been resized.
  void WasResized();

  void Focus();
  void Blur();
  void LostCapture();

  // Notifies the RenderWidgetHost that the View was destroyed.
  void ViewDestroyed();

  // Indicates if the page has finished loading.
  void SetIsLoading(bool is_loading);

  // Get access to the widget's backing store.  If a resize is in progress,
  // then the current size of the backing store may be less than the size of
  // the widget's view.  This method returns NULL if the backing store could
  // not be created.
  BackingStore* GetBackingStore();

  // Allocate a new backing store of the given size. Returns NULL on failure
  // (for example, if we don't currently have a RenderWidgetHostView.)
  BackingStore* AllocBackingStore(const gfx::Size& size);

  // Checks to see if we can give up focus to this widget through a JS call.
  virtual bool CanBlur() const { return true; }

  // Starts a hang monitor timeout. If there's already a hang monitor timeout
  // the new one will only fire if it has a shorter delay than the time
  // left on the existing timeouts.
  void StartHangMonitorTimeout(base::TimeDelta delay);

  // Restart the active hang monitor timeout. Clears all existing timeouts and
  // starts with a new one.  This can be because the renderer has become
  // active, the tab is being hidden, or the user has chosen to wait some more
  // to give the tab a chance to become active and we don't want to display a
  // warning too soon.
  void RestartHangMonitorTimeout();

  // Stops all existing hang monitor timeouts and assumes the renderer is
  // responsive.
  void StopHangMonitorTimeout();

  // Called when the system theme changes. At this time all existing native
  // theme handles are invalid and the renderer must obtain new ones and
  // repaint.
  void SystemThemeChanged();

  // Forwards the given message to the renderer. These are called by the view
  // when it has received a message.
  void ForwardMouseEvent(const WebMouseEvent& mouse_event);
  void ForwardWheelEvent(const WebMouseWheelEvent& wheel_event);
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& key_event);

  // This is for derived classes to give us access to the resizer rect.
  // And to also expose it to the RenderWidgetHostView.
  virtual gfx::Rect GetRootWindowResizerRect() const;

 protected:
  // Internal implementation of the public Forward*Event() methods.
  void ForwardInputEvent(const WebInputEvent& input_event, int event_size);

  // Called when we receive a notification indicating that the renderer
  // process has gone. This will reset our state so that our state will be
  // consistent if a new renderer is created.
  void RendererExited();

  // Called when we an InputEvent was not processed by the renderer. This is
  // overridden by RenderView to send upwards to its delegate.
  virtual void UnhandledKeyboardEvent(const NativeWebKeyboardEvent& event) {}

  // Notification that the user pressed the enter key or the spacebar. The
  // render view host overrides this to forward the information to its delegate
  // (see corresponding function in RenderViewHostDelegate).
  virtual void OnEnterOrSpace() {}

  // Callbacks for notification when the renderer becomes unresponsive to user
  // input events, and subsequently responsive again. RenderViewHost overrides
  // these to tell its delegate to show the user a warning.
  virtual void NotifyRendererUnresponsive() {}
  virtual void NotifyRendererResponsive() {}

 private:
  FRIEND_TEST(RenderWidgetHostTest, Resize);
  FRIEND_TEST(RenderWidgetHostTest, HiddenPaint);

  // Tell this object to destroy itself.
  void Destroy();

  // Checks whether the renderer is hung and calls NotifyRendererUnresponsive
  // if it is.
  void CheckRendererIsUnresponsive();

  // Called if we know the renderer is responsive. When we currently thing the
  // renderer is unresponsive, this will clear that state and call
  // NotifyRendererResponsive.
  void RendererIsResponsive();

  // IPC message handlers
  void OnMsgRenderViewReady();
  void OnMsgRenderViewGone();
  void OnMsgClose();
  void OnMsgRequestMove(const gfx::Rect& pos);
  void OnMsgPaintRect(const ViewHostMsg_PaintRect_Params& params);
  void OnMsgScrollRect(const ViewHostMsg_ScrollRect_Params& params);
  void OnMsgInputEventAck(const IPC::Message& message);
  void OnMsgFocus();
  void OnMsgBlur();
  void OnMsgSetCursor(const WebCursor& cursor);
  // Using int instead of ViewHostMsg_ImeControl for control's type to avoid
  // having to bring in render_messages.h in a header file.
  void OnMsgImeUpdateStatus(int control, const gfx::Rect& caret_rect);

  // Paints the given bitmap to the current backing store at the given location.
  void PaintBackingStoreRect(TransportDIB* dib,
                             const gfx::Rect& bitmap_rect,
                             const gfx::Size& view_size);

  // Scrolls the given |clip_rect| in the backing by the given dx/dy amount. The
  // |dib| and its corresponding location |bitmap_rect| in the backing store
  // is the newly painted pixels by the renderer.
  void ScrollBackingStoreRect(TransportDIB* dib,
                              const gfx::Rect& bitmap_rect,
                              int dx, int dy,
                              const gfx::Rect& clip_rect,
                              const gfx::Size& view_size);

  // The View associated with the RenderViewHost. The lifetime of this object
  // is associated with the lifetime of the Render process. If the Renderer
  // crashes, its View is destroyed and this pointer becomes NULL, even though
  // render_view_host_ lives on to load another URL (creating a new View while
  // doing so).
  RenderWidgetHostView* view_;

  // Created during construction but initialized during Init*(). Therefore, it
  // is guaranteed never to be NULL, but its channel may be NULL if the
  // renderer crashed, so you must always check that.
  RenderProcessHost* process_;

  // The ID of the corresponding object in the Renderer Instance.
  int routing_id_;

  // Indicates whether a page is loading or not.
  bool is_loading_;

  // Indicates whether a page is hidden or not.
  bool is_hidden_;

  // Set if we are waiting for a repaint ack for the view.
  bool repaint_ack_pending_;

  // True when waiting for RESIZE_ACK.
  bool resize_ack_pending_;

  // The current size of the RenderWidget.
  gfx::Size current_size_;

  // If true, then we should not ask our view to repaint when our backingstore
  // is updated.
  bool suppress_view_updating_;

  // True if a mouse move event was sent to the render view and we are waiting
  // for a corresponding ViewHostMsg_HandleInputEvent_ACK message.
  bool mouse_move_pending_;

  // The next mouse move event to send (only non-null while mouse_move_pending_
  // is true).
  scoped_ptr<WebMouseEvent> next_mouse_move_;

  // The time when an input event was sent to the RenderWidget.
  base::TimeTicks input_event_start_time_;

  // If true, then we should repaint when restoring even if we have a
  // backingstore.  This flag is set to true if we receive a paint message
  // while is_hidden_ to true.  Even though we tell the render widget to hide
  // itself, a paint message could already be in flight at that point.
  bool needs_repainting_on_restore_;

  // This is true if the renderer is currently unresponsive.
  bool is_unresponsive_;

  // The following value indicates a time in the future when we would consider
  // the renderer hung if it does not generate an appropriate response message.
  base::Time time_when_considered_hung_;

  // This timer runs to check if time_when_considered_hung_ has past.
  base::OneShotTimer<RenderWidgetHost> hung_renderer_timer_;

  // Optional observer that listens for notifications of painting.
  scoped_ptr<PaintObserver> paint_observer_;

  // Set when we call DidPaintRect/DidScrollRect on the view.
  bool view_being_painted_;

  // Used for UMA histogram logging to measure the time for a repaint view
  // operation to finish.
  base::TimeTicks repaint_start_time_;

  // Queue of keyboard events that we need to track.
  typedef std::queue<NativeWebKeyboardEvent> KeyQueue;

  // A queue of keyboard events. We can't trust data from the renderer so we
  // stuff key events into a queue and pop them out on ACK, feeding our copy
  // back to whatever unhandled handler instead of the returned version.
  KeyQueue key_queue_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_H_
