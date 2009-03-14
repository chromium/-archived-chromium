// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_WINDOW_H__
#define CHROME_VIEWS_WINDOW_H__

#include "chrome/common/notification_registrar.h"
#include "chrome/views/client_view.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/widget_win.h"

namespace gfx {
class Point;
class Size;
};

namespace views {

class Client;
class NonClientView;
class WindowDelegate;

///////////////////////////////////////////////////////////////////////////////
//
// Window
//
//  A Window is a WidgetWIn that has a caption and a border. The frame is
//  rendered by the operating system.
//
///////////////////////////////////////////////////////////////////////////////
class Window : public WidgetWin,
               public NotificationObserver {
 public:
  virtual ~Window();

  // Creates the appropriate Window class for a Chrome dialog or window. This
  // means a ChromeWindow or a standard Windows frame.
  static Window* CreateChromeWindow(HWND parent,
                                    const gfx::Rect& bounds,
                                    WindowDelegate* window_delegate);

  // Return the maximum size possible size the window should be have if it is
  // to be positioned within the bounds of the current "work area" (screen or
  // parent window).
  gfx::Size CalculateMaximumSize() const;

  // Show the window.
  void Show();
  void Show(int show_state);

  // Retrieve the show state of the window. This is one of the SW_SHOW* flags
  // passed into Windows' ShowWindow method. For normal windows this defaults
  // to SW_SHOWNORMAL, however windows (e.g. the main window) can override this
  // method to provide different values (e.g. retrieve the user's specified
  // show state from the shortcut starutp info).
  virtual int GetShowState() const;

  // Activate the window, assuming it already exists and is visible.
  void Activate();

  // Sizes and/or places the window to the specified bounds, size or position.
  void SetBounds(const gfx::Rect& bounds);
  // As above, except the window is inserted after |other_hwnd| in the window
  // Z-order. If this window's HWND is not yet visible, other_hwnd's monitor
  // is used as the constraining rectangle, rather than this window's hwnd's
  // monitor.
  void SetBounds(const gfx::Rect& bounds, HWND other_hwnd);

  // Closes the window, ultimately destroying it.
  void Close();

  // Whether or not the window is maximized or minimized.
  bool IsMaximized() const;
  bool IsMinimized() const;

  // Toggles the enable state for the Close button (and the Close menu item in
  // the system menu).
  void EnableClose(bool enable);

  // Prevents the window from being rendered as deactivated the next time it is.
  // This state is reset automatically as soon as the window becomes actiated
  // again. There is no ability to control the state through this API as this
  // leads to sync problems.
  void DisableInactiveRendering();

  // Tell the window to update its title from the delegate.
  void UpdateWindowTitle();

  // Tell the window to update its icon from the delegate.
  void UpdateWindowIcon();

  // Executes the specified SC_command.
  void ExecuteSystemMenuCommand(int command);

  // Shortcut to access the determination of whether or not we're using a
  // native frame. This triggers different rendering modes in certain views and
  // should be used in preference to calling win_util::ShouldUseVistaFrame.
  bool UseNativeFrame() const { return non_client_view_->UseNativeFrame(); }

  // Returns the bounds of the window required to display the content area
  // at the specified bounds.
  gfx::Rect GetWindowBoundsForClientBounds(const gfx::Rect& client_bounds);

  // Creates an appropriate NonClientFrameView for this window.
  virtual NonClientFrameView* CreateFrameViewForWindow();

  // Updates the frame after an event caused it to be changed.
  virtual void UpdateFrameAfterFrameChange();

  // Accessors and setters for various properties.
  WindowDelegate* window_delegate() const { return window_delegate_; }
  HWND owning_window() const { return owning_hwnd_; }
  ClientView* client_view() const { return non_client_view_->client_view(); }
  bool is_active() const { return is_active_; }
  void set_focus_on_creation(bool focus_on_creation) {
    focus_on_creation_ = focus_on_creation;
  }
  void set_force_hidden(bool force_hidden) { force_hidden_ = force_hidden; }

  // Returns the preferred size of the contents view of this window based on
  // its localized size data. The width in cols is held in a localized string
  // resource identified by |col_resource_id|, the height in the same fashion.
  // TODO(beng): This should eventually live somewhere else, probably closer to
  //             ClientView.
  static int GetLocalizedContentsWidth(int col_resource_id);
  static int GetLocalizedContentsHeight(int row_resource_id);
  static gfx::Size GetLocalizedContentsSize(int col_resource_id,
                                            int row_resource_id);

  // NotificationObserver overrides:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  // Constructs the Window. |window_delegate| cannot be NULL.
  explicit Window(WindowDelegate* window_delegate);

  // Create the Window.
  // If parent is NULL, this Window is top level on the desktop.
  // If |bounds| is empty, the view is queried for its preferred size and
  // centered on screen.
  virtual void Init(HWND parent, const gfx::Rect& bounds);

  // Sizes the window to the default size specified by its ClientView.
  virtual void SizeWindowToDefault();

  // Returns true if the Window is considered to be an "app window" - i.e. any
  // window which when it is the last of its type closed causes the application
  // to exit.
  virtual bool IsAppWindow() const { return false; }

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
  virtual void OnInitMenu(HMENU menu);
  virtual void OnMouseLeave();
  virtual LRESULT OnNCActivate(BOOL active);
  virtual LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);
  virtual LRESULT OnNCHitTest(const CPoint& point);
  virtual void OnNCPaint(HRGN rgn);
  virtual void OnNCLButtonDown(UINT ht_component, const CPoint& point);
  virtual void OnNCRButtonDown(UINT ht_component, const CPoint& point);
  virtual LRESULT OnNCUAHDrawCaption(UINT msg, WPARAM w_param, LPARAM l_param);
  virtual LRESULT OnNCUAHDrawFrame(UINT msg, WPARAM w_param, LPARAM l_param);
  virtual LRESULT OnSetCursor(HWND window, UINT hittest_code, UINT message);
  virtual LRESULT OnSetIcon(UINT size_type, HICON new_icon);
  virtual LRESULT OnSetText(const wchar_t* text);
  virtual void OnSize(UINT size_param, const CSize& new_size);
  virtual void OnSysCommand(UINT notification_code, CPoint click);
  virtual void OnWindowPosChanging(WINDOWPOS* window_pos);
  virtual Window* AsWindow() { return this; }
  virtual const Window* AsWindow() const { return this; }

  // The View that provides the non-client area of the window (title bar,
  // window controls, sizing borders etc). To use an implementation other than
  // the default, this class must be subclassed and this value set to the
  // desired implementation before calling |Init|.
  NonClientView* non_client_view_;

  // Accessor for disable_inactive_rendering_.
  bool disable_inactive_rendering() const {
    return disable_inactive_rendering_;
  }

 private:
  // Set the window as modal (by disabling all the other windows).
  void BecomeModal();

  // Sets-up the focus manager with the view that should have focus when the
  // window is shown the first time.  If NULL is returned, the focus goes to the
  // button if there is one, otherwise the to the Cancel button.
  void SetInitialFocus();

  // Place and size the window when it is created. |create_bounds| are the
  // bounds used when the window was created.
  void SetInitialBounds(const gfx::Rect& create_bounds);

  // Restore saved always on stop state and add the always on top system menu
  // if needed.
  void InitAlwaysOnTopState();

  // Add an item for "Always on Top" to the System Menu.
  void AddAlwaysOnTopSystemMenuItem();

  // If necessary, enables all ancestors.
  void RestoreEnabledIfNecessary();

  // Update the window style to reflect the always on top state.
  void AlwaysOnTopChanged();

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

  // Static resource initialization.
  static void InitClass();
  enum ResizeCursor {
    RC_NORMAL = 0, RC_VERTICAL, RC_HORIZONTAL, RC_NESW, RC_NWSE
  };
  static HCURSOR resize_cursors_[6];

  // Our window delegate (see Init method for documentation).
  WindowDelegate* window_delegate_;

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

  // Whether the window is currently always on top.
  bool is_always_on_top_;

  // We need to own the text of the menu, the Windows API does not copy it.
  std::wstring always_on_top_menu_text_;

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

  // True if we should prevent attempts to make the window visible when we
  // handle WM_WINDOWPOSCHANGING. Some calls like ShowWindow(SW_RESTORE) make
  // the window visible in addition to restoring it, when all we want to do is
  // restore it.
  bool force_hidden_;

  // Hold onto notifications.
  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace views

#endif  // CHROME_VIEWS_WINDOW_H__
