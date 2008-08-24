// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for working with strings.

#ifndef CHROME_BROWSER_SHELL_DEBUGGER_CONTENTS_H__
#define CHROME_BROWSER_SHELL_DEBUGGER_CONTENTS_H__

#include "chrome/browser/dom_ui/dom_ui_host.h"

class DebuggerContents : public DOMUIHost {
 public:
  DebuggerContents(Profile* profile, SiteInstance* instance);

  static bool IsDebuggerUrl(const GURL& url);

 protected:
  // WebContents overrides:
  // We override updating history with a no-op so these pages
  // are not saved to history.
  virtual void UpdateHistoryForNavigation(const GURL& url,
      const ViewHostMsg_FrameNavigate_Params& params) { }

  // DOMUIHost implementation.
  virtual void AttachMessageHandlers();

  DISALLOW_EVIL_CONSTRUCTORS(DebuggerContents);
};

#endif  // CHROME_BROWSER_DEBUGGER_CONTENTS_H__

