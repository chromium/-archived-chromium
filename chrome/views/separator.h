// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_SEPARATOR_H__
#define CHROME_VIEWS_SEPARATOR_H__

#include "chrome/views/native_control.h"

namespace ChromeViews {

// The Separator class is a view that shows a line used to visually separate
// other views.  The current implementation is only horizontal.

class Separator : public NativeControl {
 public:
  Separator();
  virtual ~Separator();

  // NativeControl overrides:
  virtual HWND CreateNativeControl(HWND parent_container);
  virtual LRESULT OnNotify(int w_param, LPNMHDR l_param);

  // View overrides:
  virtual gfx::Size GetPreferredSize();

 private:

  DISALLOW_EVIL_CONSTRUCTORS(Separator);
};

}
#endif  // #define CHROME_VIEWS_SEPARATOR_H__
