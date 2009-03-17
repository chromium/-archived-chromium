// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for working with strings.

#ifndef CHROME_BROWSER_DOM_UI_DEBUGGER_UI_H_
#define CHROME_BROWSER_DOM_UI_DEBUGGER_UI_H_

#include "chrome/browser/dom_ui/dom_ui.h"

class DebuggerUI : public DOMUI {
 public:
  DebuggerUI(WebContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(DebuggerUI);
};

#endif  // CHROME_BROWSER_DOM_UI_DEBUGGER_UI_H_
