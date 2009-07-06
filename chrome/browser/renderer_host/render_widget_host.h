// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_H_

#include <queue>
#include <vector>

#include "base/gfx/native_widget_types.h"
#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/property_bag.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "webkit/glue/webtextdirection.h"

namespace gfx {
class Rect;
}

namespace WebKit {
class WebInputEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
}

class BackingStore;
class PaintObserver;
class RenderProcessHost;
class RenderWidgetHostView;
class RenderWidgetHostPaintingObserver;
class TransportDIB;
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
// 1. By a TabContents as the communication conduit for a rendered web page.
//    The TabContents instantiates a derived class: RenderViewHost.
// 2. By a TabContents as the communication conduit for a select widget. The
//    TabContents instantiates the RenderWidgetHost directly.
//
// For every TabContents there are several objects in play that need to be
// properly destroyed or cleaned up when certain events occur.
//
// - TabContents - the TabContents itself, and its associated HWND.
// - RenderViewHost - representing the communication conduit with the child
//   process.
// - RenderWidgetHostView - the view of the web page content, message handler,
//   and plugin root.
//
// Normally, the TabContents contains a child RenderWidgetHostView that renders
// the contents of the loaded page. It has a WS_CLIPCHILDREN style so that it
// does no painting of its own.
//
// The lifetime of the RenderWidgetHostView is tied to the render process. If
// the render process dies, the RenderWidgetHostView goes away and all
// references to it must become NULL. If the TabContents finds itself without a
// RenderWidgetHostView, it paints Sad Tab instead.
//
// RenderViewHost (a RenderWidgetHost subclass) is the conduit used to
// communicate with the RenderView and is owned by the TabContents. If the
// render process crashes, the RenderViewHost remains and restarts the render
// process if needed to continue navigation.
//
// The TabContents is itself owned by the NavigationController in which it
// resides.
//
// Some examples of how shutdown works:
//
// When a tab is closed (either by the user, the web page calling window.close,
// etc) the TabStrip destroys the associated NavigationController, which calls
// Destroy on each TabContents it owns.
//
// For a TabContents, its Destroy method tells the RenderViewHost to
// shut down the render process and die.
//
// When the render process is destroyed it destroys the View: the
// RenderWidgetHostView, which destroys its HWND and deletes that object.
//
// For select popups, the situation is a little different. The RenderWidgetHost
// associated with the select popup owns the view and itself (is responsible
// for destroying itself when the view is closed). The TabContents's only
// responsibility is to select popups is to create them when it is told to. When
// the View is destroyed via an IPC message (for when WebCore destroys the
// popup, e.g. if the user selects one of the options), or because
// WM_CANCELMODE is received by the view, the View schedules the destruction of
// the render process. However in this case since there's no TabContents
// container, when the render process is destroyed, the RenderWidgetHost just
// deletes itself, which is safe because no one else should have any references
// to it (the TabContents does not).
//
// It should be noted that the RenderViewHost, not the RenderWidgetHost,
// handles IPC messages relating to the render process going away, since the
// way a RenderViewHost (TabContents) handles the process dying is different to
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

  // Returns the property bag for this widget, where callers can add extra data
  // they may wish to associate with it. Returns a pointer rather than a
  // reference since the PropertyAccessors expect this.
  const PropertyBag* property_bag() const { return &property_bag_; }
  PropertyBag* property_bag() { return &property_bag_; }

  // The painting observer that will be called for paint events. This
  // pointer's ownership will remain with the caller and must remain valid
  // until this class is destroyed or the observer is replaced.
  RenderWidgetHostPaintingObserver* painting_observer() const {
    return painting_observer_;
  }
  void set_painting_observer(RenderWidgetHostPaintingObserver* observer) {
    painting_observer_ = observer;
  }

  // Called when a renderer object already been created for this host, and we
  // just need to be attached to it. Used for window.open, <select> dropdown
  // menus, and other times when the renderer initiates creating an object.
  void Init();

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

  // Called to notify the RenderWidget that its associated native window got
  // focused.
  virtual void GotFocus();

  // Tells the renderer it got/lost focus.
  void Focus();
  void Blur();
  void LostCapture();

  // Notifies the RenderWidgetHost that the View was destroyed.
  void ViewDestroyed();

  // Indicates if the page has finished loading.
  void SetIsLoading(bool is_loading);

  // Get access to the widget's backing store.  If a resize is in progress,
  // then the current size of the backing store may be less than the size of
  // the widget's view.  If you pass |force_create| as true, then the backing
  // store will be created if it doesn't exist. Otherwise, NULL will be returned
  // if the backing store doesn't already exist. It will also return NULL if the
  // backing store could not be created.
  BackingStore* GetBackingStore(bool force_create);

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
  virtual void ForwardMouseEvent(const WebKit::WebMouseEvent& mouse_event);
  void ForwardWheelEvent(const WebKit::WebMouseWheelEvent& wheel_event);
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& key_event);
  virtual void ForwardEditCommand(const std::string& name,
                                  const std::string& value);

  // Update the text direction of the focused input element and notify it to a
  // renderer process.
  // These functions have two usage scenarios: changing the text direction
  // from a menu (as Safari does), and; changing the text direction when a user
  // presses a set of keys (as IE and Firefox do).
  // 1. Change the text direction from a menu.
  // In this scenario, we receive a menu event only once and we should update
  // the text direction immediately when a user chooses a menu item. So, we
  // should call both functions at once as listed in the following snippet.
  //   void RenderViewHost::SetTextDirection(WebTextDirection direction) {
  //     UpdateTextDirection(direction);
  //     NotifyTextDirection();
  //   }
  // 2. Change the text direction when pressing a set of keys.
  // Becauses of auto-repeat, we may receive the same key-press event many
  // times while we presses the keys and it is nonsense to send the same IPC
  // messsage every time when we receive a key-press event.
  // To suppress the number of IPC messages, we just update the text direction
  // when receiving a key-press event and send an IPC message when we release
  // the keys as listed in the following snippet.
  //   if (key_event.type == WebKeyboardEvent::KEY_DOWN) {
  //     if (key_event.windows_key_code == 'A' &&
  //         key_event.modifiers == WebKeyboardEvent::CTRL_KEY) {
  //       UpdateTextDirection(dir);
  //     } else {
  //       CancelUpdateTextDirection();
  //     }
  //   } else if (key_event.type == WebKeyboardEvent::KEY_UP) {
  //     NotifyTextDirection();
  //   }
  // Once we cancel updating the text direction, we have to ignore all
  // succeeding UpdateTextDirection() requests until calling
  // NotifyTextDirection(). (We may receive keydown events even after we
  // canceled updating the text direction because of auto-repeat.)
  // Note: we cannot undo this change for compatibility with Firefox and IE.
  void UpdateTextDirection(WebTextDirection direction);
  void CancelUpdateTextDirection();
  void NotifyTextDirection();

  // Notifies the renderer whether or not the IME attached to this process is
  // activated.
  // When the IME is activated, a renderer process sends IPC messages to notify
  // the status of its composition node. (This message is mainly used for
  // notifying the position of the input cursor so that the browser can
  // display IME windows under the cursor.)
  void ImeSetInputMode(bool activate);

  // Update the composition node of the renderer (or WebKit).
  // WebKit has a special node (a composition node) for IMEs to change its text
  // without affecting any other DOM nodes. When the IME (attached to the
  // browser) updates its text, the browser sends IPC messages to update the
  // composition node of the renderer.
  // (Read the comments of each function for its detail.)

  // Sets the text of the composition node.
  // This function can also update the cursor position and mark the specified
  // range in the composition node.
  // A browser should call this function:
  // * when it receives a WM_IME_COMPOSITION message with a GCS_COMPSTR flag
  //   (on Windows);
  // * when it receives a "preedit_changed" signal of GtkIMContext (on Linux);
  // * when markedText of NSTextInput is called (on Mac).
  void ImeSetComposition(const std::wstring& ime_string,
                         int cursor_position,
                         int target_start,
                         int target_end);

  // Finishes an ongoing composition with the specified text.
  // A browser should call this function:
  // * when it receives a WM_IME_COMPOSITION message with a GCS_RESULTSTR flag
  //   (on Windows);
  // * when it receives a "commit" signal of GtkIMContext (on Linux);
  // * when insertText of NSTextInput is called (on Mac).
  void ImeConfirmComposition(const std::wstring& ime_string);

  // Cancels an ongoing composition.
  void ImeCancelComposition();

  // This is for derived classes to give us access to the resizer rect.
  // And to also expose it to the RenderWidgetHostView.
  virtual gfx::Rect GetRootWindowResizerRect() const;

 protected:
  // Internal implementation of the public Forward*Event() methods.
  void ForwardInputEvent(
      const WebKit::WebInputEvent& input_event, int event_size);

  // Called when we receive a notification indicating that the renderer
  // process has gone. This will reset our state so that our state will be
  // consistent if a new renderer is created.
  void RendererExited();

  // Retrieves an id the renderer can use to refer to its view.
  // This is used for various IPC messages, including plugins.
  gfx::NativeViewId GetNativeViewId();

  // Called when we an InputEvent was not processed by the renderer. This is
  // overridden by RenderView to send upwards to its delegate.
  virtual void UnhandledKeyboardEvent(const NativeWebKeyboardEvent& event) {}

  // Notification that the user has made some kind of input that could
  // perform an action. The render view host overrides this to forward the
  // information to its delegate (see corresponding function in
  // RenderViewHostDelegate). The gestures that count are 1) any mouse down
  // event and 2) enter or space key presses.
  virtual void OnUserGesture() {}

  // Callbacks for notification when the renderer becomes unresponsive to user
  // input events, and subsequently responsive again. RenderViewHost overrides
  // these to tell its delegate to show the user a warning.
  virtual void NotifyRendererUnresponsive() {}
  virtual void NotifyRendererResponsive() {}

 protected:
  // true if a renderer has once been valid. We use this flag to display a sad
  // tab only when we lose our renderer and not if a paint occurs during
  // initialization.
  bool renderer_initialized_;

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
  void OnMsgShowPopup(const IPC::Message& message);

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

  // Stores random bits of data for others to associate with this object.
  PropertyBag property_bag_;

  // Observer that will be called for paint events. This may be NULL. The
  // pointer is not owned by this class.
  RenderWidgetHostPaintingObserver* painting_observer_;

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

  // True if a mouse move event was sent to the render view and we are waiting
  // for a corresponding ViewHostMsg_HandleInputEvent_ACK message.
  bool mouse_move_pending_;

  // The next mouse move event to send (only non-null while mouse_move_pending_
  // is true).
  scoped_ptr<WebKit::WebMouseEvent> next_mouse_move_;

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

  // Flag to detect recurive calls to GetBackingStore().
  bool in_get_backing_store_;

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

  // Set when we update the text direction of the selected input element.
  bool text_direction_updated_;
  WebTextDirection text_direction_;

  // Set when we cancel updating the text direction.
  // This flag also ignores succeeding update requests until we call
  // NotifyTextDirection().
  bool text_direction_canceled_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_H_
