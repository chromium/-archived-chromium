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

#ifndef CHROME_VIEWS_WINDOW_DELEGATE_H__
#define CHROME_VIEWS_WINDOW_DELEGATE_H__

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>
#include <string>

#include "chrome/views/window.h"
#include "skia/include/SkBitmap.h"

namespace ChromeViews {

class DialogDelegate;
class View;

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
  virtual ~WindowDelegate() {
    window_.release();
  }

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
  virtual SkBitmap GetWindowIcon() {
    return SkBitmap();
  }

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

#endif  // #ifndef CHROME_VIEWS_WINDOW_DELEGATE_H__
