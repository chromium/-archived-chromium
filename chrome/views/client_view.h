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

#ifndef CHROME_VIEWS_CLIENT_VIEW_H__
#define CHROME_VIEWS_CLIENT_VIEW_H__

#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/native_button.h"

namespace ChromeViews {

class Window;

///////////////////////////////////////////////////////////////////////////////
// ClientView
//
//  A ClientView is a view representing the "Client" area of a Window. This is
//  defined on Windows as being the portion of the window excluding the frame,
//  title bar and window controls.
//
//  This abstraction is used to provide both the native frame window class
//  (ChromeViews::Window) and custom frame window class
//  (ChromeViews::ChromeWindow) with a client view container and dialog button
//  helper. ChromeWindow is used for all dialogs and windows on Windows XP and
//  Windows Vista without Aero, and Window is used on Windows Vista with Aero,
//  and so this class allows both Window types to share the same dialog button
//  behavior.
//
///////////////////////////////////////////////////////////////////////////////
class ClientView : public View,
                   public NativeButton::Listener {
 public:
  ClientView(Window* window, View* contents_view);
  virtual ~ClientView();

  // Adds the dialog buttons required by the supplied WindowDelegate to the
  // view.
  void ShowDialogButtons();

  // Updates the enabled state and label of the buttons required by the
  // supplied WindowDelegate
  void UpdateDialogButtons();

  // Returns true if the specified point (in screen coordinates) is within the
  // resize box area of the window.
  bool PointIsInSizeBox(const gfx::Point& point);

  // Accessors in case the user wishes to adjust these buttons.
  NativeButton* ok_button() const { return ok_button_; }
  NativeButton* cancel_button() const { return cancel_button_; }

  // View overrides:
  virtual void Paint(ChromeCanvas* canvas);
  virtual void PaintChildren(ChromeCanvas* canvas);
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual void DidChangeBounds(const CRect& prev, const CRect& next);
  virtual void GetPreferredSize(CSize* out);
  virtual bool AcceleratorPressed(const Accelerator& accelerator);

  // NativeButton::Listener implementation:
  virtual void ButtonPressed(NativeButton* sender);

 private:
  // Paint the size box in the bottom right corner of the window if it is
  // resizable.
  void PaintSizeBox(ChromeCanvas* canvas);

  // Returns the width of the specified dialog button using the correct font.
  int GetButtonWidth(DialogDelegate::DialogButton button);

  // Position and size various sub-views.
  void LayoutDialogButtons();
  void LayoutContentsView();

  bool has_dialog_buttons() const { return ok_button_ || cancel_button_; }

  // The dialog buttons.
  NativeButton* ok_button_;
  NativeButton* cancel_button_;
  // The button-level extra view, NULL unless the dialog delegate supplies one.
  View* extra_view_;

  // The layout rect of the size box, when visible.
  gfx::Rect size_box_bounds_;

  // The Window that owns us.
  Window* owner_;

  // The contents of the client area, supplied by the caller.
  View* contents_view_;

  // Static resource initialization
  static void InitClass();
  static ChromeFont dialog_button_font_;

  DISALLOW_EVIL_CONSTRUCTORS(ClientView);
};

}

#endif  // #ifndef CHROME_VIEWS_CLIENT_VIEW_H__
