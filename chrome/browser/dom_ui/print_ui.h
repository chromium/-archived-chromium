// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_PRINT_UI_H_
#define CHROME_BROWSER_DOM_UI_PRINT_UI_H_

#include "chrome/browser/dom_ui/dom_ui.h"

class PrintUI : public DOMUI {
 public:
  explicit PrintUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintUI);
};

#endif  // CHROME_BROWSER_DOM_UI_PRINT_UI_H_
