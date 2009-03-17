// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_WINDOW_WIN_H__
#define CHROME_VIEWS_WINDOW_WIN_H__

#include "chrome/common/notification_registrar.h"
#include "chrome/views/client_view.h"
#include "chrome/views/non_client_view.h"
#include "chrome/views/widget/widget_win.h"
#include "chrome/views/window.h"

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
                  public Window,
                  public NotificationObserver {
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

  // Accessors and setters for various properties.
  HWND owning_window() const { return owning_hwnd_; }
  void set_focus_on_creation(bool focus_on_creation) {
    focus_on_creation_ = focus_on_creation;
  }
  void set_force_hidden(bool force_hidden) { force_hidden_ = force_hidden; }

  // Window overrides:
  virtual gfx::Rect GetBounds() const;
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void SetBounds(const gfx::Rect& bounds,
                         gfx::NativeWindow other_window);
  virtual void Show();
  virtual void Activate();
  virtual void Close();
  virtual void Maximize();
  virtual void Minimize();
  virtual void Restore();
  virtual bool IsActive() const;
  virtual bool IsVisible() const;
  virtual bool IsMaximized() const;
  virtual bool IsMinimized() const;
  virtual void EnableClose(bool enable);
  virtual void DisableInactiveRendering();
  virtual void UpdateWindowTitle();
  virtual void UpdateWindowIcon();
  virtual NonClientFrameView* CreateFrameViewForWindow();
  virtual void UpdateFrameAfterFrameChange();
  virtual WindowDelegate* GetDelegate() const;
  virtual NonClientView* GetNonClientView() const;
  virtual ClientView* GetClientView() const;
  virtual gfx::NativeWindow GetNativeWindow() const;

  // NotificationObserver overrides:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

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

  // Returns true if the WindowWin is considered to be an "app window" - i.e.
  // any window which when it is the last of its type closed causes the
  // application to exit.
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

  DISALLOW_COPY_AND_ASSIGN(WindowWin);
};

}  // namespace views

#endif  // CHROME_VIEWS_WINDOW_WIN_H__
