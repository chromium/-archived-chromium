// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_PLUGIN_HOST_H__
#define CHROME_BROWSER_CHROME_PLUGIN_HOST_H__

#include "chrome/common/chrome_plugin_api.h"

// Returns the table of browser functions for use from the browser process.
CPBrowserFuncs* GetCPBrowserFuncsForBrowser();

// Interface for generic data passed to plugin UI command handlers.
// Note: All functions are called on the plugin thread.
class CPCommandInterface {
 public:
  virtual ~CPCommandInterface() {}

  // Returns the actual pointer to pass to the plugin.
  virtual void *GetData() = 0;

  // Called when the command has been invoked.  The default action is deletion,
  // but some callers may want to use output or check the return value before
  // deleting.
  virtual void OnCommandInvoked(CPError retval) {
    delete this;
  }

  // Some commands have an asynchronous response.  This is called some time
  // after OnCommandInvoked.
  virtual void OnCommandResponse(CPError retval) { }
};

// Called when a builtin or plugin-registered UI command is invoked by a user
// gesture.  'data' is an optional parameter that allows command-specific data
// to be passed to the plugin.  Ownership of 'data' is transferred to the
// callee. CPBrowsingContext is some additional data the caller wishes to pass
// through to the receiver. OnCommandInvoked is called after the command has
// been invoked.
void CPHandleCommand(int command, CPCommandInterface* data,
                     CPBrowsingContext context);

#endif  // CHROME_BROWSER_CHROME_PLUGIN_HOST_H__
