// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_VIEWS_DELEGATE_H_
#define VIEWS_VIEWS_DELEGATE_H_

#include <string>
#if defined(OS_WIN)
#include <windows.h>
#endif

class Clipboard;
namespace gfx {
class Rect;
}

namespace views {

// ViewsDelegate is an interface implemented by an object using the views
// framework. It is used to obtain various high level application utilities
// and perform some actions such as window placement saving.
//
// The embedding app must set views_delegate to assign its ViewsDelegate
// implementation.
class ViewsDelegate {
 public:
  // Gets the clipboard.
  virtual Clipboard* GetClipboard() const = 0;

  // Saves the position, size and maximized state for the window with the
  // specified name.
  virtual void SaveWindowPlacement(const std::wstring& window_name,
                                   const gfx::Rect& bounds,
                                   bool maximized) = 0;

  // Retrieves the saved position and size for the window with the specified
  // name.
  virtual bool GetSavedWindowBounds(const std::wstring& window_name,
                                    gfx::Rect* bounds) const = 0;

  // Retrieves the saved maximized state for the window with the specified
  // name.
  virtual bool GetSavedMaximizedState(const std::wstring& window_name,
                                      bool* maximized) const = 0;

#if defined(OS_WIN)
  // Retrieves the default window icon to use for windows if none is specified.
  virtual HICON GetDefaultWindowIcon() const = 0;
#endif

  // The active ViewsDelegate used by the views system.
  static ViewsDelegate* views_delegate;
};

}  // namespace views

#endif  // #ifndef VIEWS_VIEWS_DELEGATE_H_
