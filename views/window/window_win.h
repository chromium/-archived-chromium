// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_WINDOW_WIN_H_
#define VIEWS_WINDOW_WINDOW_WIN_H_

#include "views/widget/widget_win.h"
#include "views/window/window.h"

namespace gfx {
class Point;
class Size;
};

namespace views {

class Client;
class WindowDelegate;

///////////////////////////////////////////////////////////////////////////////
//
// WindowWin
//
//  A WindowWin is a WidgetWin that has a caption and a border. The frame is
//  rendered by the operating system.
//
///////////////////////////////////////////////////////////////////////////////
class WindowWin : public WidgetWin,
                  public Window {
 public:
  virtual ~WindowWin();

  // Show the window with the specified show command.
  void Show(int show_state);

  // Retrieve the show state of the window. This is one of the SW_SHOW* flags
  // passed into Windows' ShowWindow method. For normal windows this defaults
  // to SW_SHOWNORMAL, however windows (e.g. the main window) can override this
  // method to provide different values (e.g. retrieve the user's specified
  // show state from the shortcut starutp info).
  virtual int GetShowState() const;

  // Executes the specified SC_command.
  void ExecuteSystemMenuCommand(int command);

  // Called when the frame type could possibly be changing (theme change or
  // DWM composition change).
  void FrameTypeChanged();

  // Accessors and setters for various properties.
  HWND owning_window() const { return owning_hwnd_; }
  void set_focus_on_creation(bool focus_on_creation) {
    focus_on_creation_ = focus_on_creation;
  }

  // Overridden from Window:
  virtual gfx::Rect GetBounds() const;
  virtual gfx::Rect GetNormalBounds() const;
  virtual void SetBounds(const gfx::Rect& bounds,
                         gfx::NativeWindow other_window);
  virtual void Show();
  virtual void HideWindow();
  virtual void PushForceHidden();
  virtual void PopForceHidden();
  virtual void Activate();
  virtual void Close();
  virtual void Maximize();
  virtual void Minimize();
  virtual void Restore();
  virtual bool IsActive() const;
  virtual bool IsVisible() const;
  virtual bool IsMaximized() const;
  virtual bool IsMinimized() const;
  virtual void SetFullscreen(bool fullscreen);
  virtual bool IsFullscreen() const;
  virtual void EnableClose(bool enable);
  virtual void DisableInactiveRendering();
  virtual void UpdateWindowTitle();
  virtual void UpdateWindowIcon();
  virtual void SetIsAlwaysOnTop(bool always_on_top);
  virtual NonClientFrameView* CreateFrameViewForWindow();
  virtual void UpdateFrameAfterFrameChange();
  virtual WindowDelegate* GetDelegate() const;
  virtual NonClientView* GetNonClientView() const;
  virtual ClientView* GetClientView() const;
  virtual gfx::NativeWindow GetNativeWindow() const;
  virtual bool ShouldUseNativeFrame() const;

 protected:
  friend Window;

  // Constructs the WindowWin. |window_delegate| cannot be NULL.
  explicit WindowWin(WindowDelegate* window_delegate);

  // Create the Window.
  // If parent is NULL, this WindowWin is top level on the desktop.
  // If |bounds| is empty, the view is queried for its preferred size and
  // centered on screen.
  virtual void Init(HWND parent, const gfx::Rect& bounds);

  // Sizes the window to the default size specified by its ClientView.
  virtual void SizeWindowToDefault();

  // Shows the system menu at the specified screen point.
  void RunSystemMenu(const gfx::Point& point);

  // Overridden from WidgetWin:
  virtual void OnActivate(UINT action, BOOL minimized, HWND window);
  virtual void OnActivateApp(BOOL active, DWORD thread_id);
  virtual LRESULT OnAppCommand(HWND window, short app_command, WORD device,
                               int keystate);
  virtual void OnCommand(UINT notification_code, int command_id, HWND window);
  virtual void OnDestroy();
  virtual LRESULT OnDwmCompositionChanged(UINT msg, WPARAM w_param,
                                          LPARAM l_param);
  virtual void OnFinalMessage(HWND window);
  virtual void OnGetMinMaxInfo(MINMAXINFO* minmax_info);
  virtual void OnInitMenu(HMENU menu);
  virtual void OnMouseLeave();
  virtual LRESULT OnNCActivate(BOOL active);
  virtual LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);
  virtual LRESULT OnNCHitTest(const CPoint& point);
  virtual void OnNCPaint(HRGN rgn);
  virtual void OnNCLButtonDown(UINT ht_component, const CPoint& point);
  virtual void OnNCRButtonDown(UINT ht_component, const CPoint& point);
  virtual void OnNCRButtonUp(UINT ht_component, const CPoint& point);
  virtual void OnRButtonUp(UINT ht_component, const CPoint& point);
  virtual LRESULT OnNCUAHDrawCaption(UINT msg, WPARAM w_param, LPARAM l_param);
  virtual LRESULT OnNCUAHDrawFrame(UINT msg, WPARAM w_param, LPARAM l_param);
  virtual LRESULT OnSetCursor(HWND window, UINT hittest_code, UINT message);
  virtual LRESULT OnSetIcon(UINT size_type, HICON new_icon);
  virtual LRESULT OnSetText(const wchar_t* text);
  virtual void OnSettingChange(UINT flags, const wchar_t* section);
  virtual void OnSize(UINT size_param, const CSize& new_size);
  virtual void OnSysCommand(UINT notification_code, CPoint click);
  virtual void OnWindowPosChanging(WINDOWPOS* window_pos);
  virtual Window* GetWindow() { return this; }
  virtual const Window* GetWindow() const { return this; }

  // Accessor for disable_inactive_rendering_.
  bool disable_inactive_rendering() const {
    return disable_inactive_rendering_;
  }

 private:
  // Information saved before going into fullscreen mode, used to restore the
  // window afterwards.
  struct SavedWindowInfo {
    bool maximized;
    LONG style;
    LONG ex_style;
    RECT window_rect;
  };

  // Set the window as modal (by disabling all the other windows).
  void BecomeModal();

  // Sets-up the focus manager with the view that should have focus when the
  // window is shown the first time.  If NULL is returned, the focus goes to the
  // button if there is one, otherwise the to the Cancel button.
  void SetInitialFocus();

  // Place and size the window when it is created. |create_bounds| are the
  // bounds used when the window was created.
  void SetInitialBounds(const gfx::Rect& create_bounds);

  // If necessary, enables all ancestors.
  void RestoreEnabledIfNecessary();

  // Calculate the appropriate window styles for this window.
  DWORD CalculateWindowStyle();
  DWORD CalculateWindowExStyle();

  // Asks the delegate if any to save the window's location and size.
  void SaveWindowPosition();

  // Lock or unlock the window from being able to redraw itself in response to
  // updates to its invalid region.
  class ScopedRedrawLock;
  void LockUpdates();
  void UnlockUpdates();

  // Stops ignoring SetWindowPos() requests (see below).
  void StopIgnoringPosChanges() { ignore_window_pos_changes_ = false; }

  // Resets the window region for the current window bounds if necessary.
  // If |force| is true, the window region is reset to NULL even for native
  // frame windows.
  void ResetWindowRegion(bool force);

  // Converts a non-client mouse down message to a regular ChromeViews event
  // and handle it. |point| is the mouse position of the message in screen
  // coords. |flags| are flags that would be passed with a WM_L/M/RBUTTON*
  // message and relate to things like which button was pressed. These are
  // combined with flags relating to the current key state.
  void ProcessNCMousePress(const CPoint& point, int flags);

  // Calls the default WM_NCACTIVATE handler with the specified activation
  // value, safely wrapping the call in a ScopedRedrawLock to prevent frame
  // flicker.
  LRESULT CallDefaultNCActivateHandler(BOOL active);

  // Returns the normal bounds of the window in screen coordinates and
  // whether the window is maximized. The arguments can be NULL.
  void GetWindowBoundsAndMaximizedState(gfx::Rect* bounds,
                                        bool* maximized) const;

  // Static resource initialization.
  static void InitClass();
  enum ResizeCursor {
    RC_NORMAL = 0, RC_VERTICAL, RC_HORIZONTAL, RC_NESW, RC_NWSE
  };
  static HCURSOR resize_cursors_[6];

  // Our window delegate (see Init method for documentation).
  WindowDelegate* window_delegate_;

  // The View that provides the non-client area of the window (title bar,
  // window controls, sizing borders etc). To use an implementation other than
  // the default, this class must be subclassed and this value set to the
  // desired implementation before calling |Init|.
  NonClientView* non_client_view_;

  // Whether we should SetFocus() on a newly created window after
  // Init(). Defaults to true.
  bool focus_on_creation_;

  // We need to save the parent window that spawned us, since GetParent()
  // returns NULL for dialogs.
  HWND owning_hwnd_;

  // The smallest size the window can be.
  CSize minimum_size_;

  // Whether or not the window is modal. This comes from the delegate and is
  // cached at Init time to avoid calling back to the delegate from the
  // destructor.
  bool is_modal_;

  // Whether all ancestors have been enabled. This is only used if is_modal_ is
  // true.
  bool restored_enabled_;

  // True if we're in fullscreen mode.
  bool fullscreen_;

  // Saved window information from before entering fullscreen mode.
  SavedWindowInfo saved_window_info_;

  // Set to true if the window is in the process of closing .
  bool window_closed_;

  // True when the window should be rendered as active, regardless of whether
  // or not it actually is.
  bool disable_inactive_rendering_;

  // True if this window is the active top level window.
  bool is_active_;

  // True if updates to this window are currently locked.
  bool lock_updates_;

  // The window styles of the window before updates were locked.
  DWORD saved_window_style_;

  // The saved maximized state for this window. See note in SetInitialBounds
  // that explains why we save this.
  bool saved_maximized_state_;

  // When true, this flag makes us discard incoming SetWindowPos() requests that
  // only change our position/size.  (We still allow changes to Z-order,
  // activation, etc.)
  bool ignore_window_pos_changes_;

  // The following factory is used to ignore SetWindowPos() calls for short time
  // periods.
  ScopedRunnableMethodFactory<WindowWin> ignore_pos_changes_factory_;

  // If this is greater than zero, we should prevent attempts to make the window
  // visible when we handle WM_WINDOWPOSCHANGING. Some calls like
  // ShowWindow(SW_RESTORE) make the window visible in addition to restoring it,
  // when all we want to do is restore it.
  int force_hidden_count_;

  // Set to true when the user presses the right mouse button on the caption
  // area. We need this so we can correctly show the context menu on mouse-up.
  bool is_right_mouse_pressed_on_caption_;

  // The last-seen monitor containing us, and its rect and work area.  These are
  // used to catch updates to the rect and work area and react accordingly.
  HMONITOR last_monitor_;
  gfx::Rect last_monitor_rect_, last_work_area_;

  DISALLOW_COPY_AND_ASSIGN(WindowWin);
};

}  // namespace views

#endif  // VIEWS_WINDOW_WINDOW_WIN_H_
