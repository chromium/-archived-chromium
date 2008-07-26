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

#ifndef CHROME_VIEWS_WINDOW_H__
#define CHROME_VIEWS_WINDOW_H__

#include "chrome/views/hwnd_view_container.h"

namespace gfx {
class Size;
class Path;
class Point;
};

class PrefService;

namespace ChromeViews {

class ClientView;
class Client;
class WindowDelegate;

////////////////////////////////////////////////////////////////////////////////
//
// Window
//
//  A Window is a HWNDViewContainer that has a caption and a border. The frame
//  is rendered by the operating system.
//
////////////////////////////////////////////////////////////////////////////////
class Window : public HWNDViewContainer {
 public:
  // TODO(beng): (Cleanup) move these into private section, effectively making
  //             this class "final" to all but designated friends within
  //             ChromeViews. Users in browser/ should always construct with
  //             CreateChromeWindow which will give the right version,
  //             depending on platform & configuration.
  Window();
  virtual ~Window();

  // Creates the appropriate Window class for a Chrome dialog or window. This
  // means a ChromeWindow or a standard Windows frame.
  static Window* CreateChromeWindow(HWND parent,
                                    const gfx::Rect& bounds,
                                    View* contents_view,
                                    WindowDelegate* window_delegate);

  // Create the Window.
  // If parent is NULL, this Window is top level on the desktop.
  // |contents_view| is a ChromeView that will be displayed in the client area
  // of the Window, as the sole child view of the RootView.
  // |window_delegate| is an object implementing WindowDelegate that can perform
  // controller-like tasks for this window, such as obtaining its preferred
  // placement and state from preferences (which override the default position
  // and size specified in |bounds|) and executing commands. Can be NULL.
  // If |bounds| is empty, the view is queried for its preferred size and
  // centered on screen.
  void Init(HWND parent,
            const gfx::Rect& bounds,
            View* contents_view,
            WindowDelegate* window_delegate);

  // Return the size of window (including non-client area) required to contain
  // a window of the specified client size.
  virtual gfx::Size CalculateWindowSizeForClientSize(
      const gfx::Size& client_size) const;

  // Return the maximum size possible size the window should be have if it is
  // to be positioned within the bounds of the current "work area" (screen or
  // parent window).
  gfx::Size CalculateMaximumSize() const;

  // Show the window.
  virtual void Show();

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
  virtual void Close();

  // Whether or not the window is maximized or minimized.
  bool IsMaximized() const;
  bool IsMinimized() const;

  // Toggles the enable state for the Close button (and the Close menu item in
  // the system menu).
  virtual void EnableClose(bool enable);

  WindowDelegate* window_delegate() const { return window_delegate_; }
  void set_window_delegate(WindowDelegate* delegate) {
    window_delegate_ = delegate;
  }

  // Set whether or not we should insert a client view. See comment below.
  void set_use_client_view(bool use_client_view) {
    use_client_view_ = use_client_view;
  }

  void set_focus_on_creation(bool focus_on_creation) {
    focus_on_creation_ = focus_on_creation;
  }

  // Updates the enabled state and label of the dialog buttons visible in this
  // window.
  void UpdateDialogButtons();

  // Called when the window should be canceled or accepted, if it is a dialog
  // box.
  void AcceptWindow();
  void CancelWindow();

  // Tell the window to update its title from the delegate.
  virtual void UpdateWindowTitle();

  // The parent of this window.
  HWND owning_window() const {
    return owning_hwnd_;
  }

  // Convenience methods for storing/retrieving window location information
  // to/from a PrefService using the specified |entry| name.
  // WindowDelegate instances can use these methods in their implementation of
  // SaveWindowPosition/RestoreWindowPosition to save windows' location to
  // preferences.
  static bool SaveWindowPositionToPrefService(PrefService* pref_service,
                                              const std::wstring& entry,
                                              const CRect& bounds,
                                              bool maximized,
                                              bool always_on_top);
  // Returns true if the window location was retrieved from the PrefService and
  // set in |bounds|, |maximized| and |always_on_top|.
  static bool RestoreWindowPositionFromPrefService(PrefService* pref_service,
                                                   const std::wstring& entry,
                                                   CRect* bounds,
                                                   bool* maximized,
                                                   bool* always_on_top);

  // Returns the preferred size of the contents view of this window based on
  // its localized size data. The width in cols is held in a localized string
  // resource identified by |col_resource_id|, the height in the same fashion.
  // TODO(beng): This should eventually live somewhere else, probably closer to
  //             ClientView.
  static gfx::Size GetLocalizedContentsSize(int col_resource_id,
                                            int row_resource_id);

 protected:
  virtual void SizeWindowToDefault();

  // Sets-up the focus manager with the view that should have focus when the
  // window is shown the first time.  If NULL is returned, the focus goes to the
  // button if there is one, otherwise the to the Cancel button.
  void SetInitialFocus();

  // Overridden from HWNDViewContainer:
  virtual void OnActivate(UINT action, BOOL minimized, HWND window);
  virtual void OnCommand(UINT notification_code, int command_id, HWND window);
  virtual void OnDestroy();
  virtual LRESULT OnEraseBkgnd(HDC dc);
  virtual LRESULT OnNCHitTest(const CPoint& point);
  virtual LRESULT OnSetCursor(HWND window, UINT hittest_code, UINT message);
  virtual void OnSize(UINT size_param, const CSize& new_size);
  virtual void OnSysCommand(UINT notification_code, CPoint click);

  // The client view object that contains the client area of the window,
  // including optional dialog buttons.
  ClientView* client_view_;

  // Our window delegate (see Init method for documentation).
  WindowDelegate* window_delegate_;

 private:
  // Set the window as modal (by disabling all the other windows).
  void BecomeModal();

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

  // Static resource initialization.
  static void InitClass();
  static HCURSOR nwse_cursor_;

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

  // Whether or not the client view should be inserted into the Window's view
  // hierarchy.
  // TODO(beng): (Cleanup) This is probably a short term measure until I figure
  //             out a way to make other Window subclasses (e.g.
  //             ConstrainedWindowImpl) and their users jive with the new
  //             dialog framework.
  bool use_client_view_;

  // True if the window was Accepted by the user using the OK button.
  bool accepted_;

  // Set to true if the window is in the process of closing .
  bool window_closed_;

  DISALLOW_EVIL_CONSTRUCTORS(Window);
};

}

#endif  // CHROME_VIEWS_WINDOW_H__
