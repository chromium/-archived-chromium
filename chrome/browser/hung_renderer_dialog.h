// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HUNG_RENDERER_DIALOG_H_
#define CHROME_BROWSER_HUNG_RENDERER_DIALOG_H_

#include "base/basictypes.h"

class TabContents;

class HungRendererDialog {
 public:
  static void ShowForTabContents(TabContents* contents);
  static void HideForTabContents(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(HungRendererDialog);
};

#endif  // CHROME_BROWSER_HUNG_RENDERER_DIALOG_H_
