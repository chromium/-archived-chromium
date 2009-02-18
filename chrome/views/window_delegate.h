// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_WINDOW_DELEGATE_H_
#define CHROME_VIEWS_WINDOW_DELEGATE_H_

#include <string>

#include "base/scoped_ptr.h"

class SkBitmap;

namespace gfx {
class Rect;
}
// TODO(maruel):  Remove once gfx::Rect is used instead.
namespace WTL {
class CRect;
}
using WTL::CRect;

namespace views {

class ClientView;
class DialogDelegate;
class View;
class Window;

///////////////////////////////////////////////////////////////////////////////
//
// WindowDelegate
//
//  WindowDelegate is an interface implemented by objects that wish to show a
//  Window. The window that is displayed uses this interface to determine how
//  it should be displayed and notify the delegate object of certain events.
//
///////////////////////////////////////////////////////////////////////////////
class WindowDelegate {
 public:
  WindowDelegate();
  virtual ~WindowDelegate();

  virtual DialogDelegate* AsDialogDelegate() { return NULL; }

  // Returns true if the window can be resized.
  virtual bool CanResize() const {
    return false;
  }

  // Returns true if the window can be maximized.
  virtual bool CanMaximize() const {
    return false;
  }

  // Returns true if the window should be placed on top of all other windows on
  // the system, even when it is not active. If HasAlwaysOnTopMenu() returns
  // true, then this method is only used the first time the window is opened, it
  // is stored in the preferences for next runs.
  virtual bool IsAlwaysOnTop() const {
    return false;
  }

  // Returns whether an "always on top" menu should be added to the system menu
  // of the window.
  virtual bool HasAlwaysOnTopMenu() const {
    return false;
  }

  // Returns true if the dialog should be displayed modally to the window that
  // opened it. Only windows with WindowType == DIALOG can be modal.
  virtual bool IsModal() const {
    return false;
  }

  // Returns the text to be displayed in the window title.
  virtual std::wstring GetWindowTitle() const {
    return L"";
  }

  // Returns the view that should have the focus when the dialog is opened.  If
  // NULL no view is focused.
  virtual View* GetInitiallyFocusedView() { return NULL; }

  // Returns true if the window should show a title in the title bar.
  virtual bool ShouldShowWindowTitle() const {
    return true;
  }

  // Returns the icon to be displayed in the window.
  virtual SkBitmap GetWindowIcon();

  // Returns true if a window icon should be shown.
  virtual bool ShouldShowWindowIcon() const {
    return false;
  }

  // Execute a command in the window's controller. Returns true if the command
  // was handled, false if it was not.
  virtual bool ExecuteWindowsCommand(int command_id) { return false; }

  // Returns the window's name identifier. Used to identify this window for
  // state restoration.
  virtual std::wstring GetWindowName() const {
    return std::wstring();
  }

  // Saves the window's bounds, maximized and always-on-top states. By default
  // this uses the process' local state keyed by window name (See GetWindowName
  // above). This behavior can be overridden to provide additional
  // functionality.
  virtual void SaveWindowPlacement(const gfx::Rect& bounds,
                                   bool maximized,
                                   bool always_on_top);

  // Retreives the window's bounds, maximized and always-on-top states. By
  // default, this uses the process' local state keyed by window name (See
  // GetWindowName above). This behavior can be overridden to provide
  // additional functionality.
  virtual bool GetSavedWindowBounds(gfx::Rect* bounds) const;
  virtual bool GetSavedMaximizedState(bool* maximized) const;
  virtual bool GetSavedAlwaysOnTopState(bool* always_on_top) const;
  
  // Called when the window closes.
  virtual void WindowClosing() { }

  // Returns the View that is contained within this Window.
  virtual View* GetContentsView() {
    return NULL;
  }

  // Called by the Window to create the Client View used to host the contents
  // of the window.
  virtual ClientView* CreateClientView(Window* window);

  // An accessor to the Window this delegate is bound to.
  Window* window() const { return window_.get(); }

 protected:
  // Releases the Window* we maintain. This should be done by a delegate in its
  // WindowClosing handler if it intends to be re-cycled to be used on a
  // different Window.
  void ReleaseWindow();

 private:
  friend class Window;
  // This is a little unusual. We use a scoped_ptr here because it's
  // initialized to NULL automatically. We do this because we want to allow
  // people using this helper to not have to call a ctor on this object.
  // Instead we just release the owning ref this pointer has when we are
  // destroyed.
  scoped_ptr<Window> window_;
};

}  // namespace views

#endif  // CHROME_VIEWS_WINDOW_DELEGATE_H_

