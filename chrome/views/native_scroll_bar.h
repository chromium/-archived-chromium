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

#ifndef CHROME_VIEWS_NATIVE_SCROLLBAR_H__
#define CHROME_VIEWS_NATIVE_SCROLLBAR_H__

#include "chrome/views/scroll_bar.h"

namespace ChromeViews {

class HWNDView;
class ScrollBarContainer;

/////////////////////////////////////////////////////////////////////////////
//
// NativeScrollBar
//
// A View subclass that wraps a Native Windows scrollbar control.
//
// A scrollbar is either horizontal or vertical.
//
/////////////////////////////////////////////////////////////////////////////
class NativeScrollBar : public ScrollBar {
 public:

  // Create new scrollbar, either horizontal or vertical
  explicit NativeScrollBar(bool is_horiz);
  virtual ~NativeScrollBar();

  // Overridden for layout purpose
  virtual void Layout();
  virtual void GetPreferredSize(CSize* sz);
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);

  // Overridden for keyboard UI purpose
  virtual bool OnKeyPressed(const KeyEvent& event);
  virtual bool OnMouseWheel(const MouseWheelEvent& e);

  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

  // Overridden from ScrollBar
  virtual void Update(int viewport_size, int content_size, int current_pos);
  virtual int GetLayoutSize() const;
  virtual int GetPosition() const;

  // Return the system sizes
  static int GetHorizontalScrollBarHeight();
  static int GetVerticalScrollBarWidth();

 private:
  // The sb_view_ takes care of keeping sb_container in sync with the
  // view hierarchy
  HWNDView* sb_view_;

  // sb_container_ is a custom hwnd that we use to wrap the real
  // windows scrollbar. We need to do this to get the scroll events
  // without having to do anything special in the high level hwnd.
  ScrollBarContainer* sb_container_;
};

}
#endif
