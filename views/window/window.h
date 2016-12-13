// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_WINDOW_WINDOW_H_
#define VIEWS_WINDOW_WINDOW_H_

#include "base/gfx/native_widget_types.h"

namespace gfx {
class Rect;
class Size;
}

namespace views {

class ClientView;
class NonClientFrameView;
class NonClientView;
class WindowDelegate;

// An interface implemented by an object that provides a top level window.
class Window {
 public:
  virtual ~Window() {}

  // Creates an instance of an object implementing this interface.
  static Window* CreateChromeWindow(gfx::NativeWindow parent,
                                    const gfx::Rect& bounds,
                                    WindowDelegate* window_delegate);

  // Returns the preferred size of the contents view of this window based on
  // its localized size data. The width in cols is held in a localized string
  // resource identified by |col_resource_id|, the height in the same fashion.
  // TODO(beng): This should eventually live somewhere else, probably closer to
  //             ClientView.
  static int GetLocalizedContentsWidth(int col_resource_id);
  static int GetLocalizedContentsHeight(int row_resource_id);
  static gfx::Size GetLocalizedContentsSize(int col_resource_id,
                                            int row_resource_id);

  // Closes all windows that aren't identified as "app windows" via
  // IsAppWindow. Called during application shutdown when the last "app window"
  // is closed.
  static void CloseAllSecondaryWindows();

  // Retrieves the window's bounds, including its frame.
  virtual gfx::Rect GetBounds() const = 0;

  // Retrieves the restored bounds for the window.
  virtual gfx::Rect GetNormalBounds() const = 0;

  // Sets the Window's bounds. The window is inserted after |other_window| in
  // the window Z-order. If this window is not yet visible, other_window's
  // monitor is used as the constraining rectangle, rather than this window's
  // monitor.
  virtual void SetBounds(const gfx::Rect& bounds,
                         gfx::NativeWindow other_window) = 0;

  // Makes the window visible.
  virtual void Show() = 0;

  // Hides the window. This does not delete the window, it just hides it. This
  // always hides the window, it is separate from the stack maintained by
  // Push/PopForceHidden.
  virtual void HideWindow() = 0;

  // Hides the window if it hasn't already been force-hidden. The force hidden
  // count is tracked, so calling multiple times is allowed, you just have to
  // be sure to call PopForceHidden the same number of times.
  virtual void PushForceHidden() = 0;

  // Decrements the force hidden count, showing the window if we have reached
  // the top of the stack. See PushForceHidden.
  virtual void PopForceHidden() = 0;

  // Activate the window, assuming it already exists and is visible.
  virtual void Activate() = 0;

  // Closes the window, ultimately destroying it. This isn't immediate (it
  // occurs after a return to the message loop. Implementors must also make sure
  // that invoking Close multiple times doesn't cause bad things to happen,
  // since it can happen.
  virtual void Close() = 0;

  // Maximizes/minimizes/restores the window.
  virtual void Maximize() = 0;
  virtual void Minimize() = 0;
  virtual void Restore() = 0;

  // Whether or not the window is currently active.
  virtual bool IsActive() const = 0;

  // Whether or not the window is currently visible.
  virtual bool IsVisible() const = 0;

  // Whether or not the window is maximized or minimized.
  virtual bool IsMaximized() const = 0;
  virtual bool IsMinimized() const = 0;

  // Accessors for fullscreen state.
  virtual void SetFullscreen(bool fullscreen) = 0;
  virtual bool IsFullscreen() const = 0;

  // Returns true if the Window is considered to be an "app window" - i.e.
  // any window which when it is the last of its type closed causes the
  // application to exit.
  virtual bool IsAppWindow() const { return false; }

  // Toggles the enable state for the Close button (and the Close menu item in
  // the system menu).
  virtual void EnableClose(bool enable) = 0;

  // Prevents the window from being rendered as deactivated the next time it is.
  // This state is reset automatically as soon as the window becomes activated
  // again. There is no ability to control the state through this API as this
  // leads to sync problems.
  virtual void DisableInactiveRendering() = 0;

  // Tell the window to update its title from the delegate.
  virtual void UpdateWindowTitle() = 0;

  // Tell the window to update its icon from the delegate.
  virtual void UpdateWindowIcon() = 0;

  // Sets whether or not the window is always-on-top.
  virtual void SetIsAlwaysOnTop(bool always_on_top) = 0;

  // Creates an appropriate NonClientFrameView for this window.
  virtual NonClientFrameView* CreateFrameViewForWindow() = 0;

  // Updates the frame after an event caused it to be changed.
  virtual void UpdateFrameAfterFrameChange() = 0;

  // Retrieves the Window's delegate.
  virtual WindowDelegate* GetDelegate() const = 0;

  // Retrieves the Window's non-client view.
  virtual NonClientView* GetNonClientView() const = 0;

  // Retrieves the Window's client view.
  virtual ClientView* GetClientView() const = 0;

  // Retrieves the Window's native window handle.
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Whether we should be using a native frame.
  virtual bool ShouldUseNativeFrame() const = 0;

  // Tell the window that something caused the frame type to change.
  virtual void FrameTypeChanged() = 0;
};

}  // namespace views

#endif  // #ifndef VIEWS_WINDOW_WINDOW_H_
