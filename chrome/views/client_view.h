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

#ifndef CHROME_VIEWS_CLIENT_VIEW_H_
#define CHROME_VIEWS_CLIENT_VIEW_H_

#include "chrome/views/view.h"

namespace ChromeViews {

class DialogClientView;
class Window;

///////////////////////////////////////////////////////////////////////////////
// ClientView
//
//  A ClientView is a View subclass that is used to occupy the "client area"
//  of a window. It provides basic information to the window that contains it
//  such as non-client hit testing information, sizing etc. Sub-classes of
//  ClientView are used to create more elaborate contents, e.g.
//  "DialogClientView".
class ClientView : public View {
 public:
  // Constructs a ClientView object for the specified window with the specified
  // contents. Since this object is created during the process of creating
  // |window|, |contents_view| must be valid if you want the initial size of
  // the window to be based on |contents_view|'s preferred size.
  ClientView(Window* window, View* contents_view);
  virtual ~ClientView() {}

  // Manual RTTI ftw.
  virtual DialogClientView* AsDialogClientView() { return NULL; }

  // Returns true to signal that the Window can be closed. Specialized
  // ClientView subclasses can override this default behavior to allow the
  // close to be blocked until the user corrects mistakes, accepts a warning
  // dialog, etc.
  virtual bool CanClose() const { return true; }

  // Tests to see if the specified point (in view coordinates) is within the 
  // bounds of this view. If so, it returns HTCLIENT in this default
  // implementation. If it is outside the bounds of this view, this must return
  // HTNOWHERE to tell the caller to do further processing to determine where
  // in the non-client area it is (if it is).
  // Subclasses of ClientView can extend this logic by overriding this method
  // to detect if regions within the client area count as parts of the "non-
  // client" area. A good example of this is the size box at the bottom right
  // corner of resizable dialog boxes.
  virtual int NonClientHitTest(const gfx::Point& point);

  // Overridden from View:
  virtual void GetPreferredSize(CSize* out);

 protected:
  // Overridden from View:
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual void Layout();

  // Accessors for private data members.
  Window* window() const { return window_; }
  void set_window(Window* window) { window_ = window; }
  View* contents_view() const { return contents_view_; }
  void set_contents_view(View* contents_view) {
    contents_view_ = contents_view;
  }

 private:
  // The Window that hosts this ClientView.
  Window* window_;

  // The View that this ClientView contains.
  View* contents_view_;
};

}  // namespace ChromeViews

#endif  // #ifndef CHROME_VIEWS_CLIENT_VIEW_H_