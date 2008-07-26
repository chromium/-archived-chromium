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

#ifndef CHROME_VIEWS_HWND_VIEW_CONTAINER_H__
#define CHROME_VIEWS_HWND_VIEW_CONTAINER_H__

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>

#include "base/message_loop.h"
#include "chrome/views/focus_manager.h"
#include "chrome/views/layout_manager.h"
#include "chrome/views/root_view.h"
#include "chrome/views/view_container.h"

namespace ChromeViews {

class TooltipManager;

bool SetRootViewForHWND(HWND hwnd, RootView* root_view);
RootView* GetRootViewForHWND(HWND hwnd);

// A Windows message reflected from other windows. This message is sent
// with the following arguments:
// hWnd - Target window
// uMsg - kReflectedMessage
// wParam - Should be 0
// lParam - Pointer to MSG struct containing the original message.
static const int kReflectedMessage = WM_APP + 3;

///////////////////////////////////////////////////////////////////////////////
//
// FillLayout
//  A simple LayoutManager that causes the associated view's one child to be
//  sized to match the bounds of its parent.
//
///////////////////////////////////////////////////////////////////////////////
class FillLayout : public LayoutManager {
 public:
  FillLayout();
  virtual ~FillLayout();

  // Overridden from LayoutManager:
  virtual void Layout(View* host);
  virtual void GetPreferredSize(View* host, CSize* out);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(FillLayout);
};

///////////////////////////////////////////////////////////////////////////////
//
// HWNDViewContainer
//  A container for a ChromeViews hierarchy used to represent anything that can
//  be contained within an HWND, e.g. a control, a window, etc. Specializations
//  suitable for specific tasks, e.g. top level window, are derived from this.
//
//  This ViewContainer contains a RootView which owns the hierarchy of
//  ChromeViews within it. As long as ChromeViews are part of this tree, they
//  will be deleted automatically when the RootView is destroyed. If you remove
//  a view from the tree, you are then responsible for cleaning up after it.
//
//  Note: We try and keep this API platform-neutral, since to some extent we
//        consider this the boundary between the platform and potentially cross
//        platform ChromeViews code. In some cases this isn't true, primarily
//        because of other code that has not yet been refactored to maintain
//        this <controller>-<view>-<platform-specific-view> separation.
//
///////////////////////////////////////////////////////////////////////////////
class HWNDViewContainer : public ViewContainer,
                          public MessageLoop::Observer,
                          public FocusTraversable {
 public:
  HWNDViewContainer();
  virtual ~HWNDViewContainer();

  // Initialize the container with a parent and an initial desired size.
  // |contents_view| is the view that will be the single child of RootView
  // within this ViewContainer. As contents_view is inserted into RootView's
  // tree, RootView assumes ownership of this view and cleaning it up. If you
  // remove this view, you are responsible for its destruction. If this value
  // is NULL, the caller is responsible for populating the RootView, and sizing
  // its contents as the window is sized.
  // If |has_own_focus_manager| is true, the focus traversal stay confined to
  // the window.
  void Init(HWND parent,
            const gfx::Rect& bounds,
            View* contents_view,
            bool has_own_focus_manager);

  // Sets the window styles. This is ONLY used when the window is created.
  // In other words, if you invoke this after invoking Init, nothing happens.
  void set_window_style(DWORD style) { window_style_ = style; }
  DWORD window_style() const { return window_style_; }

  // Sets the extended window styles. See comment about |set_window_style|.
  void set_window_ex_style(DWORD style) { window_ex_style_ = style; }
  DWORD window_ex_style() const { return window_ex_style_; };

  // Sets the class style to use. The default is CS_DBLCLKS.
  void set_initial_class_style(UINT class_style) {
    // We dynamically generate the class name, so don't register it globally!
    DCHECK((class_style & CS_GLOBALCLASS) == 0);
    class_style_ = class_style;
  }
  UINT initial_class_style() { return class_style_; }

  void set_delete_on_destroy(bool delete_on_destroy) {
    delete_on_destroy_ = delete_on_destroy;
  }

  // Sets the initial opacity of a layered window, or updates the window's
  // opacity if it is on the screen.
  void SetLayeredAlpha(BYTE layered_alpha);

  // Disable Layered Window updates by setting to false.
  void set_can_update_layered_window(bool can_update_layered_window) {
    can_update_layered_window_ = can_update_layered_window;
  }

  // Returns the RootView associated with the specified HWND (if any).
  static RootView* FindRootView(HWND hwnd);

  // Closes the window asynchronously by scheduling a task for it.  The window
  // is destroyed as a result.
  // This invokes Hide to hide the window, and schedules a task that
  // invokes CloseNow.
  virtual void Close();

  // Hides the window. This does NOT delete the window, it just hides it.
  virtual void Hide();

  // Closes the window synchronously.  Note that this should not be called from
  // an ATL message callback as it deletes the HWNDDViewContainer and ATL will
  // dereference it after the callback is processed.
  void CloseNow();

  // All classes registered by HWNDViewContainer start with this name.
  static const wchar_t* const kBaseClassName;

  BEGIN_MSG_MAP_EX(0)
    // Range handlers must go first!
    MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    MESSAGE_RANGE_HANDLER_EX(WM_NCMOUSEMOVE, WM_NCMOUSEMOVE, OnMouseRange)
    MESSAGE_RANGE_HANDLER_EX(WM_SETTINGCHANGE, WM_SETTINGCHANGE, OnSettingChange)

    // Reflected message handler
    MESSAGE_HANDLER_EX(kReflectedMessage, OnReflectedMessage)

    // This list is in _ALPHABETICAL_ order! OR I WILL HURT YOU.
    MSG_WM_ACTIVATE(OnActivate)
    MSG_WM_CANCELMODE(OnCancelMode)
    MSG_WM_CAPTURECHANGED(OnCaptureChanged)
    MSG_WM_CLOSE(OnClose)
    MSG_WM_COMMAND(OnCommand)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_DESTROY(OnDestroy)
    MSG_WM_ERASEBKGND(OnEraseBkgnd)
    MSG_WM_GETMINMAXINFO(OnGetMinMaxInfo)
    MSG_WM_HSCROLL(OnHScroll)
    MSG_WM_INITMENU(OnInitMenu)
    MSG_WM_KEYDOWN(OnKeyDown)
    MSG_WM_KEYUP(OnKeyUp)
    MSG_WM_LBUTTONDBLCLK(OnLButtonDblClk)
    MSG_WM_LBUTTONDOWN(OnLButtonDown)
    MSG_WM_LBUTTONUP(OnLButtonUp)
    MSG_WM_MBUTTONDOWN(OnMButtonDown)
    MSG_WM_MBUTTONUP(OnMButtonUp)
    MSG_WM_MBUTTONDBLCLK(OnMButtonDblClk)
    MSG_WM_MOUSEACTIVATE(OnMouseActivate)
    MSG_WM_MOUSELEAVE(OnMouseLeave)
    MSG_WM_MOUSEMOVE(OnMouseMove)
    MSG_WM_MOUSEWHEEL(OnMouseWheel)
    MSG_WM_MOVING(OnMoving)
    MSG_WM_NCACTIVATE(OnNCActivate)
    MSG_WM_NCCALCSIZE(OnNCCalcSize)
    MSG_WM_NCHITTEST(OnNCHitTest)
    MSG_WM_NCMOUSEMOVE(OnNCMouseMove)
    MSG_WM_NCLBUTTONDBLCLK(OnNCLButtonDblClk)
    MSG_WM_NCLBUTTONDOWN(OnNCLButtonDown)
    MSG_WM_NCLBUTTONUP(OnNCLButtonUp)
    MSG_WM_NCPAINT(OnNCPaint)
    MSG_WM_NCRBUTTONDBLCLK(OnNCRButtonDblClk)
    MSG_WM_NCRBUTTONDOWN(OnNCRButtonDown)
    MSG_WM_NCRBUTTONUP(OnNCRButtonUp)
    MSG_WM_NOTIFY(OnNotify)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_RBUTTONDBLCLK(OnRButtonDblClk)
    MSG_WM_RBUTTONDOWN(OnRButtonDown)
    MSG_WM_RBUTTONUP(OnRButtonUp)
    MSG_WM_SETCURSOR(OnSetCursor)
    MSG_WM_SETFOCUS(OnSetFocus)
    MSG_WM_SIZE(OnSize)
    MSG_WM_SYSCOMMAND(OnSysCommand)
    MSG_WM_VSCROLL(OnVScroll)
    MSG_WM_WINDOWPOSCHANGED(OnWindowPosChanged)
  END_MSG_MAP()

  // Overridden from ViewContainer:
  virtual void GetBounds(CRect *out, bool including_frame) const;
  virtual void MoveToFront(bool should_activate);
  virtual HWND GetHWND() const;
  virtual void PaintNow(const CRect& update_rect);
  virtual RootView* GetRootView();
  virtual bool IsVisible();
  virtual bool IsActive();
  virtual TooltipManager* GetTooltipManager();

  // Overridden from MessageLoop::Observer:
  void WillProcessMessage(const MSG& msg);
  virtual void DidProcessMessage(const MSG& msg);

  // Overridden from FocusTraversable:
  virtual View* FindNextFocusableView(View* starting_view,
                                      bool reverse,
                                      Direction direction,
                                      bool dont_loop,
                                      FocusTraversable** focus_traversable,
                                      View** focus_traversable_view);
  virtual FocusTraversable* GetFocusTraversableParent();
  virtual View* GetFocusTraversableParentView();

  void SetFocusTraversableParent(FocusTraversable* parent);
  void SetFocusTraversableParentView(View* parent_view);

  virtual bool GetAccelerator(int cmd_id,
                              ChromeViews::Accelerator* accelerator) {
    return false;
  }

  BOOL IsWindow() const {
    return ::IsWindow(GetHWND());
  }

  BOOL ShowWindow(int command) {
    DCHECK(::IsWindow(GetHWND()));
    return ::ShowWindow(GetHWND(), command);
  }

  HWND SetCapture() {
    DCHECK(::IsWindow(GetHWND()));
    return ::SetCapture(GetHWND());
  }

  HWND GetParent() const {
    return ::GetParent(GetHWND());
  }

  BOOL GetWindowRect(RECT* rect) const {
    return ::GetWindowRect(GetHWND(), rect);
  }

  BOOL SetWindowPos(HWND hwnd_after, int x, int y, int cx, int cy, UINT flags) {
    DCHECK(::IsWindow(GetHWND()));
    return ::SetWindowPos(GetHWND(), hwnd_after, x, y, cx, cy, flags);
  }

  BOOL IsZoomed() const {
    DCHECK(::IsWindow(GetHWND()));
    return ::IsZoomed(GetHWND());
  }

  BOOL MoveWindow(int x, int y, int width, int height) {
    return MoveWindow(x, y, width, height, TRUE);
  }

  BOOL MoveWindow(int x, int y, int width, int height, BOOL repaint) {
    DCHECK(::IsWindow(GetHWND()));
    return ::MoveWindow(GetHWND(), x, y, width, height, repaint);
  }

  int SetWindowRgn(HRGN region, BOOL redraw) {
    DCHECK(::IsWindow(GetHWND()));
    return ::SetWindowRgn(GetHWND(), region, redraw);
  }

  BOOL GetClientRect(RECT* rect) const {
    DCHECK(::IsWindow(GetHWND()));
    return ::GetClientRect(GetHWND(), rect);
  }

 protected:

  // Call close instead of this to Destroy the window.
  BOOL DestroyWindow() {
    DCHECK(::IsWindow(GetHWND()));
    return ::DestroyWindow(GetHWND());
  }

  // Message Handlers
  // These are all virtual so that specialized view containers can modify or
  // augment processing.
  // This list is in _ALPHABETICAL_ order!
  // Note: in the base class these functions must do nothing but convert point
  //       coordinates to client coordinates (if necessary) and forward the
  //       handling to the appropriate Process* function. This is so that
  //       subclasses can easily override these methods to do different things
  //       and have a convenient function to call to get the default behavior.
  virtual void OnActivate(UINT action, BOOL minimized, HWND window) { }
  virtual void OnCancelMode() {}
  virtual void OnCaptureChanged(HWND hwnd);
  virtual void OnClose();
  virtual void OnCommand(
    UINT notification_code, int command_id, HWND window) { }
  virtual LRESULT OnCreate(LPCREATESTRUCT create_struct) { return 0; }
  // WARNING: If you override this be sure and invoke super, otherwise we'll
  // leak a few things.
  virtual void OnDestroy();
  virtual LRESULT OnEraseBkgnd(HDC dc);
  virtual void OnGetMinMaxInfo(LPMINMAXINFO mm_info) { }
  virtual void OnHScroll(int scroll_type, short position, HWND scrollbar) {
    SetMsgHandled(FALSE);
  }
  virtual void OnInitMenu(HMENU menu) { SetMsgHandled(FALSE); }
  virtual void OnKeyDown(TCHAR c, UINT rep_cnt, UINT flags);
  virtual void OnKeyUp(TCHAR c, UINT rep_cnt, UINT flags);
  virtual void OnLButtonDblClk(UINT flags, const CPoint& point);
  virtual void OnLButtonDown(UINT flags, const CPoint& point);
  virtual void OnLButtonUp(UINT flags, const CPoint& point);
  virtual void OnMButtonDblClk(UINT flags, const CPoint& point);
  virtual void OnMButtonDown(UINT flags, const CPoint& point);
  virtual void OnMButtonUp(UINT flags, const CPoint& point);
  virtual LRESULT OnMouseActivate(HWND window, UINT hittest_code, UINT message);
  virtual void OnMouseMove(UINT flags, const CPoint& point);
  virtual void OnMouseLeave();
  virtual void OnMoving(UINT param, const LPRECT new_bounds) { }
  virtual LRESULT OnMouseWheel(UINT flags, short distance, const CPoint& point);
  virtual LRESULT OnMouseRange(UINT msg, WPARAM w_param, LPARAM l_param);
  virtual LRESULT OnNCActivate(BOOL active) { SetMsgHandled(FALSE); return 0; }
  virtual LRESULT OnNCCalcSize(BOOL w_param, LPARAM l_param) { SetMsgHandled(FALSE); return 0; }
  virtual LRESULT OnNCHitTest(const CPoint& pt) { SetMsgHandled(FALSE); return 0; }
  virtual void OnNCLButtonDblClk(UINT flags, const CPoint& point);
  virtual void OnNCLButtonDown(UINT flags, const CPoint& point);
  virtual void OnNCLButtonUp(UINT flags, const CPoint& point);
  virtual LRESULT OnNCMouseMove(UINT flags, const CPoint& point);
  virtual void OnNCPaint(HRGN rgn) { SetMsgHandled(FALSE); }
  virtual void OnNCRButtonDblClk(UINT flags, const CPoint& point);
  virtual void OnNCRButtonDown(UINT flags, const CPoint& point);
  virtual void OnNCRButtonUp(UINT flags, const CPoint& point);
  virtual LRESULT OnNotify(int w_param, NMHDR* l_param);
  virtual void OnPaint(HDC dc);
  virtual void OnRButtonDblClk(UINT flags, const CPoint& point);
  virtual void OnRButtonDown(UINT flags, const CPoint& point);
  virtual void OnRButtonUp(UINT flags, const CPoint& point);
  virtual LRESULT OnReflectedMessage(UINT msg, WPARAM w_param, LPARAM l_param) {
    SetMsgHandled(FALSE);
    return 0;
  }
  virtual LRESULT OnSetCursor(HWND window, UINT hittest_code, UINT message) {
    SetMsgHandled(FALSE);
    return 0;
  }
  virtual void OnSetFocus(HWND focused_window) {
    SetMsgHandled(FALSE);
  }
  virtual LRESULT OnSettingChange(UINT msg, WPARAM w_param, LPARAM l_param);
  virtual void OnSize(UINT param, const CSize& size);
  virtual void OnSysCommand(UINT notification_code, CPoint click) { }
  virtual void OnVScroll(int scroll_type, short position, HWND scrollbar) {
    SetMsgHandled(FALSE);
  }
  virtual void OnWindowPosChanged(WINDOWPOS* window_pos) {
    SetMsgHandled(FALSE);
  }

  // deletes this window as it is destroyed, override to provide different
  // behavior.
  virtual void OnFinalMessage(HWND window);

  // Start tracking all mouse events so that this window gets sent mouse leave
  // messages too.
  void TrackMouseEvents();

  // Actually handle mouse events. These functions are called by subclasses who
  // override the message handlers above to do the actual real work of handling
  // the event in the View system.
  bool ProcessMousePressed(const CPoint& point, UINT flags, bool dbl_click);
  void ProcessMouseDragged(const CPoint& point, UINT flags);
  void ProcessMouseReleased(const CPoint& point, UINT flags);
  void ProcessMouseMoved(const CPoint& point, UINT flags);
  void ProcessMouseExited();

  // Makes sure the window still fits on screen after a settings change message
  // from the OS, e.g. a screen resolution change.
  virtual void AdjustWindowToFitScreenSize();

  // Handles re-laying out content in response to a window size change.
  virtual void ChangeSize(UINT size_param, const CSize& size);

  // Returns whether capture should be released on mouse release. The default
  // is true.
  virtual bool ReleaseCaptureOnMouseReleased() { return true; }

  enum FrameAction {FA_NONE = 0, FA_RESIZING, FA_MOVING, FA_FORWARDING};

  virtual RootView* CreateRootView();

  // Returns true if this HWNDViewContainer is opaque.
  bool opaque() const { return opaque_; }

  // The root of the View hierarchy attached to this window.
  scoped_ptr<RootView> root_view_;

  // Current frame ui action
  FrameAction current_action_;

  // Whether or not we have capture the mouse.
  bool has_capture_;

  // If true, the mouse is currently down.
  bool is_mouse_down_;

  scoped_ptr<TooltipManager> tooltip_manager_;

 private:
  // Resize the bitmap used to contain the contents of the layered window. This
  // recreates the entire bitmap.
  void SizeContents(const CRect& window_rect);

  // Paint into a DIB and then update the layered window with its contents.
  void PaintLayeredWindow();

  // In layered mode, update the layered window. |dib_dc| represents a handle
  // to a device context that contains the contents of the window.
  void UpdateWindowFromContents(HDC dib_dc);

  // Invoked from WM_DESTROY. Does appropriate cleanup and invokes OnDestroy
  // so that subclasses can do any cleanup they need to.
  void OnDestroyImpl();

  // The windows procedure used by all HWNDViewContainers.
  static LRESULT CALLBACK WndProc(HWND window,
                                  UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param);

  // Gets the window class name to use when creating the corresponding HWND.
  // If necessary, this registers the window class.
  std::wstring GetWindowClassName();

  // The following factory is used for calls to close the HWNDViewContainer
  // instance.
  ScopedRunnableMethodFactory<HWNDViewContainer> close_container_factory_;

  // Whether or not we are currently tracking mouse events for this HWND
  // using |::TrackMouseEvent|
  bool tracking_mouse_events_;

  // Whether or not this is a top level window.
  bool toplevel_;

  bool opaque_;

  // Window Styles used when creating the window.
  DWORD window_style_;

  // Window Extended Styles used when creating the window.
  DWORD window_ex_style_;

  // Style of the class to use.
  UINT class_style_;

  // Whether or not this is a layered window.
  bool layered_;

  // The default alpha to be applied to the layered window.
  BYTE layered_alpha_;

  // A canvas that contains the window contents in the case of a layered
  // window.
  scoped_ptr<ChromeCanvas> contents_;

  // Whether or not the window should delete itself when it is destroyed.
  // Set this to false via its setter for stack allocated instances.
  bool delete_on_destroy_;

  // True if we are allowed to update the layered window from the DIB backing
  // store if necessary.
  bool can_update_layered_window_;

  // The following are used to detect duplicate mouse move events and not
  // deliver them. Displaying a window may result in the system generating
  // duplicate move events even though the mouse hasn't moved.

  // If true, the last event was a mouse move event.
  bool last_mouse_event_was_move_;

  // Coordinates of the last mouse move event, in terms of the screen.
  int last_mouse_move_x_;
  int last_mouse_move_y_;

  // Our hwnd.
  HWND hwnd_;
};

}

#endif  // #ifndef CHROME_VIEWS_HWND_VIEW_CONTAINER_H__
