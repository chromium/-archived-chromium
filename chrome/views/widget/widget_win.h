// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_WIDGET_WIDGET_WIN_H_
#define CHROME_VIEWS_WIDGET_WIDGET_WIN_H_

#include <atlbase.h>
#include <atlcrack.h>

#include "base/message_loop.h"
#include "base/system_monitor.h"
#include "chrome/views/focus_manager.h"
#include "chrome/views/layout_manager.h"
#include "chrome/views/widget/widget.h"

class ChromeCanvas;

namespace gfx {
class Rect;
}

namespace views {

class RootView;
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

// These two messages aren't defined in winuser.h, but they are sent to windows
// with captions. They appear to paint the window caption and frame.
// Unfortunately if you override the standard non-client rendering as we do
// with CustomFrameWindow, sometimes Windows (not deterministically
// reproducibly but definitely frequently) will send these messages to the
// window and paint the standard caption/title over the top of the custom one.
// So we need to handle these messages in CustomFrameWindow to prevent this
// from happening.
static const int WM_NCUAHDRAWCAPTION = 0xAE;
static const int WM_NCUAHDRAWFRAME = 0xAF;

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
  virtual gfx::Size GetPreferredSize(View* host);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(FillLayout);
};

///////////////////////////////////////////////////////////////////////////////
//
// WidgetWin
//  A Widget for a views hierarchy used to represent anything that can be
//  contained within an HWND, e.g. a control, a window, etc. Specializations
//  suitable for specific tasks, e.g. top level window, are derived from this.
//
//  This Widget contains a RootView which owns the hierarchy of views within it.
//  As long as views are part of this tree, they will be deleted automatically
//  when the RootView is destroyed. If you remove a view from the tree, you are
//  then responsible for cleaning up after it.
//
///////////////////////////////////////////////////////////////////////////////
class WidgetWin : public Widget,
                  public MessageLoopForUI::Observer,
                  public FocusTraversable,
                  public AcceleratorTarget {
 public:
  WidgetWin();
  virtual ~WidgetWin();

  // Initialize the Widget with a parent and an initial desired size.
  // |contents_view| is the view that will be the single child of RootView
  // within this Widget. As contents_view is inserted into RootView's tree,
  // RootView assumes ownership of this view and cleaning it up. If you remove
  // this view, you are responsible for its destruction. If this value is NULL,
  // the caller is responsible for populating the RootView, and sizing its
  // contents as the window is sized.
  // If |has_own_focus_manager| is true, the focus traversal stay confined to
  // the window.
  void Init(HWND parent,
            const gfx::Rect& bounds,
            bool has_own_focus_manager);

  // Sets the specified view as the contents of this Widget. There can only
  // be one contnets view child of this Widget's RootView. This view is sized to
  // fit the entire size of the RootView. The RootView takes ownership of this
  // View, unless it is set as not being parent-owned.
  virtual void SetContentsView(View* view);

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

  // See description of use_layered_buffer_ for details.
  void SetUseLayeredBuffer(bool use_layered_buffer);

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

  // Shows the window without changing size/position/activation state.
  virtual void Show();

  // Closes the window synchronously.  Note that this should not be called from
  // an ATL message callback as it deletes the WidgetWin and ATL will
  // dereference it after the callback is processed.
  void CloseNow();

  // All classes registered by WidgetWin start with this name.
  static const wchar_t* const kBaseClassName;

  BEGIN_MSG_MAP_EX(0)
    // Range handlers must go first!
    MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    MESSAGE_RANGE_HANDLER_EX(WM_NCMOUSEMOVE, WM_NCMOUSEMOVE, OnMouseRange)

    // Reflected message handler
    MESSAGE_HANDLER_EX(kReflectedMessage, OnReflectedMessage)

    // CustomFrameWindow hacks
    MESSAGE_HANDLER_EX(WM_NCUAHDRAWCAPTION, OnNCUAHDrawCaption)
    MESSAGE_HANDLER_EX(WM_NCUAHDRAWFRAME, OnNCUAHDrawFrame)

    // Vista and newer
    MESSAGE_HANDLER_EX(WM_DWMCOMPOSITIONCHANGED, OnDwmCompositionChanged)

    // Non-atlcrack.h handlers
    MESSAGE_HANDLER_EX(WM_GETOBJECT, OnGetObject)
    MESSAGE_HANDLER_EX(WM_NCMOUSELEAVE, OnNCMouseLeave)
    MESSAGE_HANDLER_EX(WM_MOUSELEAVE, OnMouseLeave)

    // This list is in _ALPHABETICAL_ order! OR I WILL HURT YOU.
    MSG_WM_ACTIVATE(OnActivate)
    MSG_WM_ACTIVATEAPP(OnActivateApp)
    MSG_WM_APPCOMMAND(OnAppCommand)
    MSG_WM_CANCELMODE(OnCancelMode)
    MSG_WM_CAPTURECHANGED(OnCaptureChanged)
    MSG_WM_CLOSE(OnClose)
    MSG_WM_COMMAND(OnCommand)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_DESTROY(OnDestroy)
    MSG_WM_ERASEBKGND(OnEraseBkgnd)
    MSG_WM_ENDSESSION(OnEndSession)
    MSG_WM_ENTERSIZEMOVE(OnEnterSizeMove)
    MSG_WM_EXITMENULOOP(OnExitMenuLoop)
    MSG_WM_HSCROLL(OnHScroll)
    MSG_WM_INITMENU(OnInitMenu)
    MSG_WM_INITMENUPOPUP(OnInitMenuPopup)
    MSG_WM_KEYDOWN(OnKeyDown)
    MSG_WM_KEYUP(OnKeyUp)
    MSG_WM_SYSKEYDOWN(OnKeyDown)
    MSG_WM_SYSKEYUP(OnKeyUp)
    MSG_WM_LBUTTONDBLCLK(OnLButtonDblClk)
    MSG_WM_LBUTTONDOWN(OnLButtonDown)
    MSG_WM_LBUTTONUP(OnLButtonUp)
    MSG_WM_MBUTTONDOWN(OnMButtonDown)
    MSG_WM_MBUTTONUP(OnMButtonUp)
    MSG_WM_MBUTTONDBLCLK(OnMButtonDblClk)
    MSG_WM_MOUSEACTIVATE(OnMouseActivate)
    MSG_WM_MOUSEMOVE(OnMouseMove)
    MSG_WM_MOUSEWHEEL(OnMouseWheel)
    MSG_WM_MOVE(OnMove)
    MSG_WM_MOVING(OnMoving)
    MSG_WM_NCACTIVATE(OnNCActivate)
    MSG_WM_NCCALCSIZE(OnNCCalcSize)
    MSG_WM_NCHITTEST(OnNCHitTest)
    MSG_WM_NCMOUSEMOVE(OnNCMouseMove)
    MSG_WM_NCLBUTTONDBLCLK(OnNCLButtonDblClk)
    MSG_WM_NCLBUTTONDOWN(OnNCLButtonDown)
    MSG_WM_NCLBUTTONUP(OnNCLButtonUp)
    MSG_WM_NCMBUTTONDBLCLK(OnNCMButtonDblClk)
    MSG_WM_NCMBUTTONDOWN(OnNCMButtonDown)
    MSG_WM_NCMBUTTONUP(OnNCMButtonUp)
    MSG_WM_NCPAINT(OnNCPaint)
    MSG_WM_NCRBUTTONDBLCLK(OnNCRButtonDblClk)
    MSG_WM_NCRBUTTONDOWN(OnNCRButtonDown)
    MSG_WM_NCRBUTTONUP(OnNCRButtonUp)
    MSG_WM_NOTIFY(OnNotify)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_POWERBROADCAST(OnPowerBroadcast)
    MSG_WM_RBUTTONDBLCLK(OnRButtonDblClk)
    MSG_WM_RBUTTONDOWN(OnRButtonDown)
    MSG_WM_RBUTTONUP(OnRButtonUp)
    MSG_WM_SETCURSOR(OnSetCursor)
    MSG_WM_SETFOCUS(OnSetFocus)
    MSG_WM_SETICON(OnSetIcon)
    MSG_WM_SETTEXT(OnSetText)
    MSG_WM_SETTINGCHANGE(OnSettingChange)
    MSG_WM_SIZE(OnSize)
    MSG_WM_SYSCOMMAND(OnSysCommand)
    MSG_WM_THEMECHANGED(OnThemeChanged)
    MSG_WM_VSCROLL(OnVScroll)
    MSG_WM_WINDOWPOSCHANGING(OnWindowPosChanging)
    MSG_WM_WINDOWPOSCHANGED(OnWindowPosChanged)
  END_MSG_MAP()

  // Overridden from Widget:
  virtual void GetBounds(gfx::Rect* out, bool including_frame) const;
  virtual void MoveToFront(bool should_activate);
  virtual gfx::NativeView GetNativeView() const;
  virtual void PaintNow(const gfx::Rect& update_rect);
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

  // Overridden from AcceleratorTarget:
  virtual bool AcceleratorPressed(const Accelerator& accelerator) {
    return false;
  }

  void SetFocusTraversableParent(FocusTraversable* parent);
  void SetFocusTraversableParentView(View* parent_view);

  virtual bool GetAccelerator(int cmd_id, Accelerator* accelerator) {
    return false;
  }

  BOOL IsWindow() const {
    return ::IsWindow(GetNativeView());
  }

  BOOL ShowWindow(int command) {
    DCHECK(::IsWindow(GetNativeView()));
    return ::ShowWindow(GetNativeView(), command);
  }

  HWND SetCapture() {
    DCHECK(::IsWindow(GetNativeView()));
    return ::SetCapture(GetNativeView());
  }

  HWND GetParent() const {
    return ::GetParent(GetNativeView());
  }

  BOOL GetWindowRect(RECT* rect) const {
    return ::GetWindowRect(GetNativeView(), rect);
  }

  BOOL SetWindowPos(HWND hwnd_after, int x, int y, int cx, int cy, UINT flags) {
    DCHECK(::IsWindow(GetNativeView()));
    return ::SetWindowPos(GetNativeView(), hwnd_after, x, y, cx, cy, flags);
  }

  BOOL IsZoomed() const {
    DCHECK(::IsWindow(GetNativeView()));
    return ::IsZoomed(GetNativeView());
  }

  BOOL MoveWindow(int x, int y, int width, int height) {
    return MoveWindow(x, y, width, height, TRUE);
  }

  BOOL MoveWindow(int x, int y, int width, int height, BOOL repaint) {
    DCHECK(::IsWindow(GetNativeView()));
    return ::MoveWindow(GetNativeView(), x, y, width, height, repaint);
  }

  int SetWindowRgn(HRGN region, BOOL redraw) {
    DCHECK(::IsWindow(GetNativeView()));
    return ::SetWindowRgn(GetNativeView(), region, redraw);
  }

  BOOL GetClientRect(RECT* rect) const {
    DCHECK(::IsWindow(GetNativeView()));
    return ::GetClientRect(GetNativeView(), rect);
  }

 protected:

  // Call close instead of this to Destroy the window.
  BOOL DestroyWindow() {
    DCHECK(::IsWindow(GetNativeView()));
    return ::DestroyWindow(GetNativeView());
  }

  // Message Handlers
  // These are all virtual so that specialized Widgets can modify or augment
  // processing.
  // This list is in _ALPHABETICAL_ order!
  // Note: in the base class these functions must do nothing but convert point
  //       coordinates to client coordinates (if necessary) and forward the
  //       handling to the appropriate Process* function. This is so that
  //       subclasses can easily override these methods to do different things
  //       and have a convenient function to call to get the default behavior.
  virtual void OnActivate(UINT action, BOOL minimized, HWND window) {
    SetMsgHandled(FALSE);
  }
  virtual void OnActivateApp(BOOL active, DWORD thread_id) {
    SetMsgHandled(FALSE);
  }
  virtual LRESULT OnAppCommand(HWND window, short app_command, WORD device,
                               int keystate) {
    SetMsgHandled(FALSE);
    return 0;
  }
  virtual void OnCancelMode() {}
  virtual void OnCaptureChanged(HWND hwnd);
  virtual void OnClose();
  virtual void OnCommand(UINT notification_code, int command_id, HWND window) {
    SetMsgHandled(FALSE);
  }
  virtual LRESULT OnCreate(LPCREATESTRUCT create_struct) { return 0; }
  // WARNING: If you override this be sure and invoke super, otherwise we'll
  // leak a few things.
  virtual void OnDestroy();
  virtual LRESULT OnDwmCompositionChanged(UINT msg,
                                          WPARAM w_param,
                                          LPARAM l_param) {
    SetMsgHandled(FALSE);
    return 0;
  }
  virtual void OnEndSession(BOOL ending, UINT logoff) { SetMsgHandled(FALSE); }
  virtual void OnEnterSizeMove() { SetMsgHandled(FALSE); }
  virtual void OnExitMenuLoop(BOOL is_track_popup_menu) {
    SetMsgHandled(FALSE);
  }
  virtual LRESULT OnEraseBkgnd(HDC dc);
  virtual LRESULT OnGetObject(UINT uMsg, WPARAM w_param, LPARAM l_param);
  virtual void OnHScroll(int scroll_type, short position, HWND scrollbar) {
    SetMsgHandled(FALSE);
  }
  virtual void OnInitMenu(HMENU menu) { SetMsgHandled(FALSE); }
  virtual void OnInitMenuPopup(HMENU menu, UINT position, BOOL is_system_menu) {
    SetMsgHandled(FALSE);
  }
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
  virtual LRESULT OnMouseLeave(UINT message, WPARAM w_param, LPARAM l_param);
  virtual void OnMove(const CPoint& point) { SetMsgHandled(FALSE); }
  virtual void OnMoving(UINT param, const LPRECT new_bounds) { }
  virtual LRESULT OnMouseWheel(UINT flags, short distance, const CPoint& point);
  virtual LRESULT OnMouseRange(UINT msg, WPARAM w_param, LPARAM l_param);
  virtual LRESULT OnNCActivate(BOOL active) { SetMsgHandled(FALSE); return 0; }
  virtual LRESULT OnNCCalcSize(BOOL w_param, LPARAM l_param) {
    SetMsgHandled(FALSE);
    return 0;
  }
  virtual LRESULT OnNCHitTest(const CPoint& pt) {
    SetMsgHandled(FALSE);
    return 0;
  }
  virtual void OnNCLButtonDblClk(UINT flags, const CPoint& point);
  virtual void OnNCLButtonDown(UINT flags, const CPoint& point);
  virtual void OnNCLButtonUp(UINT flags, const CPoint& point);
  virtual void OnNCMButtonDblClk(UINT flags, const CPoint& point);
  virtual void OnNCMButtonDown(UINT flags, const CPoint& point);
  virtual void OnNCMButtonUp(UINT flags, const CPoint& point);
  virtual LRESULT OnNCMouseLeave(UINT uMsg, WPARAM w_param, LPARAM l_param);
  virtual LRESULT OnNCMouseMove(UINT flags, const CPoint& point);
  virtual void OnNCPaint(HRGN rgn) { SetMsgHandled(FALSE); }
  virtual void OnNCRButtonDblClk(UINT flags, const CPoint& point);
  virtual void OnNCRButtonDown(UINT flags, const CPoint& point);
  virtual void OnNCRButtonUp(UINT flags, const CPoint& point);
  virtual LRESULT OnNCUAHDrawCaption(UINT msg,
                                     WPARAM w_param,
                                     LPARAM l_param) {
    SetMsgHandled(FALSE);
    return 0;
  }
  virtual LRESULT OnNCUAHDrawFrame(UINT msg, WPARAM w_param, LPARAM l_param) {
    SetMsgHandled(FALSE);
    return 0;
  }
  virtual LRESULT OnNotify(int w_param, NMHDR* l_param);
  virtual void OnPaint(HDC dc);
  virtual LRESULT OnPowerBroadcast(DWORD power_event, DWORD data) {
    base::SystemMonitor* monitor = base::SystemMonitor::Get();
    if (monitor)
      monitor->ProcessWmPowerBroadcastMessage(power_event);
    SetMsgHandled(FALSE);
    return 0;
  }
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
  virtual LRESULT OnSetIcon(UINT size_type, HICON new_icon) {
    SetMsgHandled(FALSE);
    return 0;
  }
  virtual LRESULT OnSetText(const wchar_t* text) {
    SetMsgHandled(FALSE);
    return 0;
  }
  virtual void OnSettingChange(UINT flags, const wchar_t* section);
  virtual void OnSize(UINT param, const CSize& size);
  virtual void OnSysCommand(UINT notification_code, CPoint click) { }
  virtual void OnThemeChanged();
  virtual void OnVScroll(int scroll_type, short position, HWND scrollbar) {
    SetMsgHandled(FALSE);
  }
  virtual void OnWindowPosChanging(WINDOWPOS* window_pos) {
    SetMsgHandled(FALSE);
  }
  virtual void OnWindowPosChanged(WINDOWPOS* window_pos) {
    SetMsgHandled(FALSE);
  }

  // deletes this window as it is destroyed, override to provide different
  // behavior.
  virtual void OnFinalMessage(HWND window);

  // Start tracking all mouse events so that this window gets sent mouse leave
  // messages too. |is_nonclient| is true when we should track WM_NCMOUSELEAVE
  // messages instead of WM_MOUSELEAVE ones.
  void TrackMouseEvents(DWORD mouse_tracking_flags);

  // Actually handle mouse events. These functions are called by subclasses who
  // override the message handlers above to do the actual real work of handling
  // the event in the View system.
  bool ProcessMousePressed(const CPoint& point, UINT flags, bool dbl_click);
  void ProcessMouseDragged(const CPoint& point, UINT flags);
  void ProcessMouseReleased(const CPoint& point, UINT flags);
  void ProcessMouseMoved(const CPoint& point, UINT flags, bool is_nonclient);
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

  // Returns true if this WidgetWin is opaque.
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

  // The windows procedure used by all WidgetWins.
  static LRESULT CALLBACK WndProc(HWND window,
                                  UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param);

  // Gets the window class name to use when creating the corresponding HWND.
  // If necessary, this registers the window class.
  std::wstring GetWindowClassName();

  // The following factory is used for calls to close the WidgetWin
  // instance.
  ScopedRunnableMethodFactory<WidgetWin> close_widget_factory_;

  // The flags currently being used with TrackMouseEvent to track mouse
  // messages. 0 if there is no active tracking. The value of this member is
  // used when tracking is canceled.
  DWORD active_mouse_tracking_flags_;

  // Whether or not this is a top level window.
  bool toplevel_;

  bool opaque_;

  // Window Styles used when creating the window.
  DWORD window_style_;

  // Window Extended Styles used when creating the window.
  DWORD window_ex_style_;

  // Style of the class to use.
  UINT class_style_;

  // Should we keep an offscreen buffer? This is initially true and if the
  // window has WS_EX_LAYERED then it remains true. You can set this to false
  // at any time to ditch the buffer, and similarly set back to true to force
  // creation of the buffer.
  //
  // NOTE: this is intended to be used with a layered window (a window with an
  // extended window style of WS_EX_LAYERED). If you are using a layered window
  // and NOT changing the layered alpha or anything else, then leave this value
  // alone. OTOH if you are invoking SetLayeredWindowAttributes then you'll
  // must likely want to set this to false, or after changing the alpha toggle
  // the extended style bit to false than back to true. See MSDN for more
  // details.
  bool use_layered_buffer_;

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

  // Coordinates of the last mouse move event, in screen coordinates.
  int last_mouse_move_x_;
  int last_mouse_move_y_;

  // Instance of accessibility information and handling for MSAA root
  CComPtr<IAccessible> accessibility_root_;

  // Our hwnd.
  HWND hwnd_;
};

}  // namespace views

#endif  // #ifndef CHROME_VIEWS_WIDGET_WIDGET_WIN_H_
