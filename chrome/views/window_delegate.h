// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_WINDOW_DELEGATE_H_
#define CHROME_VIEWS_WINDOW_DELEGATE_H_

#include <string>

#include "base/scoped_ptr.h"

class SkBitmap;

// TODO(maruel):  Remove once gfx::Rect is used instead.
namespace WTL {
class CRect;
}
using WTL::CRect;

namespace ChromeViews {

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
  virtual View* GetInitiallyFocusedView() const { return NULL; }

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

  // Execute a command in the window's controller.
  virtual void ExecuteWindowsCommand(int command_id) { }

  // Saves the specified bounds, maximized and always on top state as the
  // window's position to/ be restored the next time it is shown.
  virtual void SaveWindowPosition(const CRect& bounds,
                                  bool maximized,
                                  bool always_on_top) { }

  // returns true if there was a saved position, false if there was none and
  // the default should be used.
  virtual bool RestoreWindowPosition(CRect* bounds,
                                     bool* maximized,
                                     bool* always_on_top) {
    return false;
  }

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

 private:
  friend Window;
  // This is a little unusual. We use a scoped_ptr here because it's
  // initialized to NULL automatically. We do this because we want to allow
  // people using this helper to not have to call a ctor on this object.
  // Instead we just release the owning ref this pointer has when we are
  // destroyed.
  scoped_ptr<ChromeViews::Window> window_;
};

}  // namespace ChromeViews

#endif  // CHROME_VIEWS_WINDOW_DELEGATE_H_

