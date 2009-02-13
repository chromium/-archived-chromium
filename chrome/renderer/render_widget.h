// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_WIDGET_H__
#define CHROME_RENDERER_RENDER_WIDGET_H__

#include <vector>
#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "chrome/common/ipc_channel.h"
#include "skia/ext/platform_canvas.h"

#include "webkit/glue/webwidget_delegate.h"
#include "webkit/glue/webcursor.h"

class RenderThreadBase;
struct WebPluginGeometry;

// RenderWidget provides a communication bridge between a WebWidget and
// a RenderWidgetHost, the latter of which lives in a different process.
class RenderWidget : public IPC::Channel::Listener,
                     public IPC::Message::Sender,
                     virtual public WebWidgetDelegate,
                     public base::RefCounted<RenderWidget> {
 public:
  // Creates a new RenderWidget.  The opener_id is the routing ID of the
  // RenderView that this widget lives inside. The render_thread is any
  // RenderThreadBase implementation, mostly commonly g_render_thread.
  static RenderWidget* Create(int32 opener_id,
                              RenderThreadBase* render_thread,
                              bool activatable);

  // The routing ID assigned by the RenderProcess. Will be MSG_ROUTING_NONE if
  // not yet assigned a view ID, in which case, the process MUST NOT send
  // messages with this ID to the parent.
  int32 routing_id() const {
    return routing_id_;
  }

  // May return NULL when the window is closing.
  WebWidget* webwidget() const {
    return webwidget_;
  }

  // Implementing RefCounting required for WebWidgetDelegate
  virtual void AddRef() {
    base::RefCounted<RenderWidget>::AddRef();
  }
  virtual void Release() {
    base::RefCounted<RenderWidget>::Release();
  }

  // IPC::Channel::Listener
  virtual void OnMessageReceived(const IPC::Message& msg);

  // IPC::Message::Sender
  virtual bool Send(IPC::Message* msg);

  // True if the underlying IPC is currently sending data.
  bool InSend() const;

  // WebWidgetDelegate
  virtual gfx::NativeViewId GetContainingView(WebWidget* webwidget);
  virtual void DidInvalidateRect(WebWidget* webwidget, const gfx::Rect& rect);
  virtual void DidScrollRect(WebWidget* webwidget, int dx, int dy,
                             const gfx::Rect& clip_rect);
  virtual void SetCursor(WebWidget* webwidget, const WebCursor& cursor);
  virtual void Show(WebWidget* webwidget, WindowOpenDisposition disposition);
  virtual void CloseWidgetSoon(WebWidget* webwidget);
  virtual void Focus(WebWidget* webwidget);
  virtual void Blur(WebWidget* webwidget);
  virtual void GetWindowRect(WebWidget* webwidget, gfx::Rect* rect);
  virtual void SetWindowRect(WebWidget* webwidget, const gfx::Rect& rect);
  virtual void GetRootWindowRect(WebWidget* webwidget, gfx::Rect* rect);
  virtual void GetRootWindowResizerRect(WebWidget* webwidget, gfx::Rect* rect);
  virtual void DidMove(WebWidget* webwidget, const WebPluginGeometry& move);
  virtual void RunModal(WebWidget* webwidget) {}
  virtual bool IsHidden() { return is_hidden_; }

  // Close the underlying WebWidget.
  void Close();

  // Get the size of the paint buffer for the given rectangle, rounding up to
  // the allocation granularity of the system.
  static size_t GetPaintBufSize(const gfx::Rect& rect);

 protected:
  // Friend RefCounted so that the dtor can be non-public. Using this class
  // without ref-counting is an error.
  friend class base::RefCounted<RenderWidget>;

  RenderWidget(RenderThreadBase* render_thread, bool activatable);
  virtual ~RenderWidget();

  // Initializes this view with the given opener.  CompleteInit must be called
  // later.
  void Init(int32 opener_id);

  // Finishes creation of a pending view started with Init.
  void CompleteInit(gfx::NativeViewId parent);

  // Paints the given rectangular region of the WebWidget into canvas (a
  // shared memory segment returned by AllocPaintBuf on Windows). The caller
  // must ensure that the given rect fits within the bounds of the WebWidget.
  void PaintRect(const gfx::Rect& rect, skia::PlatformCanvas* canvas);

  void DoDeferredPaint();
  void DoDeferredScroll();

  // This method is called immediately after PaintRect but before the
  // corresponding paint or scroll message is send to the widget host.
  virtual void DidPaint() {}

  // RenderWidget IPC message handlers
  void OnClose();
  void OnCreatingNewAck(gfx::NativeViewId parent);
  void OnResize(const gfx::Size& new_size, const gfx::Rect& resizer_rect);
  void OnWasHidden();
  void OnWasRestored(bool needs_repainting);
  void OnPaintRectAck();
  void OnScrollRectAck();
  void OnHandleInputEvent(const IPC::Message& message);
  void OnMouseCaptureLost();
  void OnSetFocus(bool enable);
  void OnImeSetInputMode(bool is_active);
  void OnImeSetComposition(int string_type, int cursor_position,
                           int target_start, int target_end,
                           const std::wstring& ime_string);
  void OnMsgRepaint(const gfx::Size& size_to_paint);

  // True if a PaintRect_ACK message is pending.
  bool paint_reply_pending() const {
    return paint_reply_pending_;
  }

  // True if a ScrollRect_ACK message is pending.
  bool scroll_reply_pending() const {
    return current_scroll_buf_ != NULL;
  }

  bool next_paint_is_resize_ack() const;
  bool next_paint_is_restore_ack() const;
  void set_next_paint_is_resize_ack();
  void set_next_paint_is_restore_ack();
  void set_next_paint_is_repaint_ack();

  // Called when a renderer process moves an input focus or updates the
  // position of its caret.
  // This function compares them with the previous values, and send them to
  // the browser process only if they are updated.
  // The browser process moves IME windows and context.
  void UpdateIME();

  // Tells the renderer it does not have focus. Used to prevent us from getting
  // the focus on our own when the browser did not focus us.
  void ClearFocus();

  // Routing ID that allows us to communicate to the parent browser process
  // RenderWidgetHost. When MSG_ROUTING_NONE, no messages may be sent.
  int32 routing_id_;

  // We are responsible for destroying this object via its Close method.
  WebWidget* webwidget_;

  // Set to the ID of the view that initiated creating this view, if any. When
  // the view was initiated by the browser (the common case), this will be
  // MSG_ROUTING_NONE. This is used in determining ownership when opening
  // child tabs. See RenderWidget::createWebViewWithRequest.
  //
  // This ID may refer to an invalid view if that view is closed before this
  // view is.
  int32 opener_id_;

  // The thread that does our IPC.
  RenderThreadBase* render_thread_;

  // The position where this view should be initially shown.
  gfx::Rect initial_pos_;

  // The window we are embedded within.  TODO(darin): kill this.
  gfx::NativeViewId host_window_;

  // We store the current cursor object so we can avoid spamming SetCursor
  // messages.
  WebCursor current_cursor_;
  // The size of the RenderWidget.
  gfx::Size size_;

  // Shared memory handles that are currently in use to transfer an image to
  // the browser.
  base::SharedMemory* current_paint_buf_;
  base::SharedMemory* current_scroll_buf_;

  // The smallest bounding rectangle that needs to be re-painted.  This is non-
  // empty if a paint event is pending.
  gfx::Rect paint_rect_;

  // The clip rect for the pending scroll event.  This is non-empty if a
  // scroll event is pending.
  gfx::Rect scroll_rect_;

  // The area that must be reserved for drawing the resize corner.
  gfx::Rect resizer_rect_;

  // The scroll delta for a pending scroll event.
  gfx::Point scroll_delta_;

  // Flags for the next ViewHostMsg_PaintRect message.
  int next_paint_flags_;

  // True if we are expecting a PaintRect_ACK message (i.e., that a PaintRect
  // message has been sent).
  bool paint_reply_pending_;

  // Set to true if we should ignore RenderWidget::Show calls.
  bool did_show_;

  // Indicates that we shouldn't bother generated paint events.
  bool is_hidden_;

  // Indicates that we should be repainted when restored.  This flag is set to
  // true if we receive an invalidation / scroll event from webkit while our
  // is_hidden_ flag is set to true.  This is used to force a repaint once we
  // restore to account for the fact that our host would not know about the
  // invalidation / scroll event(s) from webkit while we are hidden.
  bool needs_repainting_on_restore_;

  // Indicates whether we have been focused/unfocused by the browser.
  bool has_focus_;

  // True if we have requested this widget be closed.  No more messages will
  // be sent, except for a Close.
  bool closing_;

  // Represents whether or not the IME of a browser process is active.
  bool ime_is_active_;

  // Represents the status of the selected edit control sent to a browser
  // process last time.
  // When a renderer process finishes rendering a region, it retrieves:
  //   * The identifier of the selected edit control;
  //   * Whether or not the selected edit control requires IME, and;
  //   * The position of the caret (or cursor).
  // If the above values is updated, a renderer process sends an IPC message
  // to a browser process. A browser process uses these values to
  // activate/deactivate IME and set the position of IME windows.
  bool ime_control_enable_ime_;
  int ime_control_x_;
  int ime_control_y_;
  bool ime_control_new_state_;
  bool ime_control_updated_;
  bool ime_control_busy_;

  // Whether the window for this RenderWidget can be activated.
  bool activatable_;

  // Holds all the needed plugin window moves for a scroll.
  std::vector<WebPluginGeometry> plugin_window_moves_;

  DISALLOW_EVIL_CONSTRUCTORS(RenderWidget);
};

#endif  // CHROME_RENDERER_RENDER_WIDGET_H__
