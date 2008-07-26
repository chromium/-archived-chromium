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
