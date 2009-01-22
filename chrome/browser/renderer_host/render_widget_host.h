// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_H_

#include <windows.h>

#include <vector>

#include "base/gfx/size.h"
#include "base/timer.h"
#include "chrome/common/ipc_channel.h"

namespace gfx {
class Rect;
}

class BackingStore;
class PaintObserver;
class RenderProcessHost;
class RenderWidgetHostView;
class WebInputEvent;
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebCursor;
enum ViewHostMsg_ImeControl;
struct ViewHostMsg_PaintRect_Params;
struct ViewHostMsg_ScrollRect_Params;
struct WebPluginGeometry;

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

  // Manual RTTI FTW. We are not hosting a web page.
  virtual bool IsRenderView() { return false; }

  // Called when a renderer object already been created for this host, and we
  // just need to be attached to it. Used for window.open, <select> dropdown
  // menus, and other times when the renderer initiates creating an object.
  virtual void Init();

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

  // Tells the renderer to die and then calls Destroy().
  virtual void Shutdown();

  void Focus();
  void Blur();
  void LostCapture();

  // Notifies the RenderWidgetHost that the View was destroyed.
  void ViewDestroyed();

  // Indicates if the page has finished loading.
  virtual void SetIsLoading(bool is_loading);

  // Represents a device-dependent drawing surface used to hold the rendering
  // of a RenderWidgetHost.
  class BackingStore;

  // Manages a set of backing stores.
  class BackingStoreManager;

  // Get access to the widget's backing store.  If a resize is in progress,
  // then the current size of the backing store may be less than the size of
  // the widget's view.  This method returns NULL if the backing store could
  // not be created.
  BackingStore* GetBackingStore();

  // An interface that gets called when paints happen.
  // Used in performance tests.
  class PaintObserver;
  // Set the PaintObserver on this object.  Takes ownership.
  void SetPaintObserver(PaintObserver* paint_observer) {
    paint_observer_.reset(paint_observer);
  }

  // Checks to see if we can give  up focus to this  widget through a
  // javascript call.
  virtual bool CanBlur() const { return true; }

  // Restart the active hang monitor timeout. Clears all existing timeouts and
  // starts with a new one.  This can be because the renderer has become
  // active, the tab is being hidden, or the user has chosen to wait some more
  // to give the tab a chance to become active and we don't want to display a
  // warning too soon.
  void RestartHangMonitorTimeout();
  
  // Stops all existing hang monitor timeouts and assumes the renderer is
  // responsive.
  void StopHangMonitorTimeout();

  // Starts a hang monitor timeout. If there's already a hang monitor timeout
  // the new one will only fire if it has a shorter delay than the time
  // left on the existing timeouts.
  void StartHangMonitorTimeout(base::TimeDelta delay);

  // Called when we receive a notification indicating that the renderer
  // process has gone.
  void RendererExited();

  // Called when the system theme changes. At this time all existing native
  // theme handles are invalid and the renderer must obtain new ones and
  // repaint.
  void SystemThemeChanged();

 protected:
  // Called when we an InputEvent was not processed by the renderer.
  virtual void UnhandledInputEvent(const WebInputEvent& event) { }

  // IPC message handlers
  void OnMsgRendererReady();
  void OnMsgRendererGone();
  void OnMsgClose();
  void OnMsgRequestMove(const gfx::Rect& pos);
  void OnMsgResizeAck();
  void OnMsgPaintRect(const ViewHostMsg_PaintRect_Params& params);
  void OnMsgScrollRect(const ViewHostMsg_ScrollRect_Params& params);
  void OnMsgInputEventAck(const IPC::Message& message);
  void OnMsgFocus();
  void OnMsgBlur();
  void OnMsgSetCursor(const WebCursor& cursor);
  void OnMsgImeUpdateStatus(ViewHostMsg_ImeControl control,
                            const gfx::Rect& caret_rect);

  void MovePluginWindows(
      const std::vector<WebPluginGeometry>& plugin_window_moves);

  // TODO(beng): (Cleanup) remove this friendship once we expose a clean API to
  // RenderWidgetHost Views. This exists only to give RenderWidgetHostView
  // access to Forward*Event.
  friend class RenderWidgetHostViewWin;

  void ForwardMouseEvent(const WebMouseEvent& mouse_event);
  virtual void ForwardKeyboardEvent(const WebKeyboardEvent& key_event);
  void ForwardWheelEvent(const WebMouseWheelEvent& wheel_event);
  void ForwardInputEvent(const WebInputEvent& input_event, int event_size);

  // Called to paint a region of the backing store
  void PaintRect(HANDLE bitmap, const gfx::Rect& bitmap_rect,
                 const gfx::Size& view_size);

  // Called to scroll a region of the backing store
  void ScrollRect(HANDLE bitmap, const gfx::Rect& bitmap_rect, int dx, int dy,
                  const gfx::Rect& clip_rect, const gfx::Size& view_size);

  // Tell this object to destroy itself.
  void Destroy();

  // Callbacks for notification when the renderer becomes unresponsive to user
  // input events, and subsequently responsive again. The delegate can use
  // these notifications to show a warning.
  void CheckRendererIsUnresponsive();
  virtual void NotifyRendererUnresponsive() {}
  void RendererIsResponsive();
  virtual void NotifyRendererResponsive() {}

  // Created during construction but initialized during Init*(). Therefore, it
  // is guaranteed never to be NULL, but its channel may be NULL if the
  // renderer crashed, so you must always check that.
  RenderProcessHost* process_;

  // The ID of the corresponding object in the Renderer Instance.
  int routing_id_;

  bool resize_ack_pending_;  // True when waiting for RESIZE_ACK.
  gfx::Size current_size_;   // The current size of the RenderWidget.

  // True if a mouse move event was sent to the render view and we are waiting
  // for a corresponding ViewHostMsg_HandleInputEvent_ACK message.
  bool mouse_move_pending_;

  // The next mouse move event to send (only non-null while mouse_move_pending_
  // is true).
  scoped_ptr<WebMouseEvent> next_mouse_move_;

  // The View associated with the RenderViewHost. The lifetime of this object
  // is associated with the lifetime of the Render process. If the Renderer
  // crashes, its View is destroyed and this pointer becomes NULL, even though
  // render_view_host_ lives on to load another URL (creating a new View while
  // doing so).
  RenderWidgetHostView* view_;

  // The time when an input event was sent to the RenderWidget.
  base::TimeTicks input_event_start_time_;

  // Indicates whether a page is loading or not.
  bool is_loading_;
  // Indicates whether a page is hidden or not.
  bool is_hidden_;
  // If true, then we should not ask our view to repaint when our backingstore
  // is updated.
  bool suppress_view_updating_;

  // If true, then we should repaint when restoring even if we have a
  // backingstore.  This flag is set to true if we receive a paint message
  // while is_hidden_ to true.  Even though we tell the render widget to hide
  // itself, a paint message could already be in flight at that point.
  bool needs_repainting_on_restore_;

  // The following value indicates a time in the future when we would consider
  // the renderer hung if it does not generate an appropriate response message.
  base::Time time_when_considered_hung_;

  // This timer runs to check if time_when_considered_hung_ has past.
  base::OneShotTimer<RenderWidgetHost> hung_renderer_timer_;

  // This is true if the renderer is currently unresponsive.
  bool is_unresponsive_;

  // Optional observer that listens for notifications of painting.
  scoped_ptr<PaintObserver> paint_observer_;

  // Set when we call DidPaintRect/DidScrollRect on the view.
  bool view_being_painted_;

  // Set if we are waiting for a repaint ack for the view.
  bool repaint_ack_pending_;

  // Used for UMA histogram logging to measure the time for a repaint view
  // operation to finish.
  base::TimeTicks repaint_start_time_;

  DISALLOW_EVIL_CONSTRUCTORS(RenderWidgetHost);
};

class RenderWidgetHost::BackingStore {
 public:
  BackingStore(const gfx::Size& size);
  ~BackingStore();

  HDC dc() { return hdc_; }
  const gfx::Size& size() { return size_; }

  // Paints the bitmap from the renderer onto the backing store.
  bool Refresh(HANDLE process, HANDLE bitmap_section,
               const gfx::Rect& bitmap_rect);

 private:
  // Creates a dib conforming to the height/width/section parameters passed
  // in. The use_os_color_depth parameter controls whether we use the color
  // depth to create an appropriate dib or not.
  HANDLE CreateDIB(HDC dc, int width, int height, bool use_os_color_depth, 
                   HANDLE section);

  // The backing store dc.
  HDC hdc_;
  // The size of the backing store.
  gfx::Size size_;
  // Handle to the backing store dib.
  HANDLE backing_store_dib_;
  // Handle to the original bitmap in the dc.
  HANDLE original_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(BackingStore);
};

class RenderWidgetHost::PaintObserver {
 public:
  virtual ~PaintObserver() {}

  // Called each time the RenderWidgetHost paints.
  virtual void RenderWidgetHostDidPaint(RenderWidgetHost* rwh) = 0;
};

#endif  // #ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_H_
