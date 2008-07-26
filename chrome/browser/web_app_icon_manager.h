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

#ifndef CHROME_BROWSER_WEB_APP_ICON_MANAGER_H__
#define CHROME_BROWSER_WEB_APP_ICON_MANAGER_H__

#include <windows.h>

#include "base/scoped_ptr.h"
#include "chrome/browser/web_app.h"

class TabContents;

// WebAppIconManager is used by SimpleXPFrame/SimpleVistaFrame to manage the
// icons for the frame. If the current contents are a web app, then icon is
// set from the app, otherwise the icons are set to the default.
class WebAppIconManager : public WebApp::Observer {
 public:
  explicit WebAppIconManager(HWND parent);
  ~WebAppIconManager();

  // Sets the contents the WebApp should come from. If the contents has a web
  // app, the image comes from it, otherwise the icon for the HWND is set to
  // the default chrome icon.
  void SetContents(TabContents* contents);

  // Enables/disables icons. If true and this WebAppIconManager was previously
  // disabled, the icon is updated immediately.
  void SetUpdatesEnabled(bool enabled);

 private:
  // Invoked when the icons of the WebApp has changed. Invokes
  // UpdateIconsFromApp appropriately.
  virtual void WebAppImagesChanged(WebApp* web_app);

  // Updates the icons of the HWND, unless we're disabled in which case this
  // does nothing.
  void UpdateIconsFromApp();

  // HWND the icon is updated on.
  const HWND hwnd_;

  // Current app, may be null.
  scoped_refptr<WebApp> app_;

  // Icons. These are only valid if the app doesn't have an icon.
  HICON small_icon_;
  HICON big_icon_;

  // Are we enabled? If not, we won't update the icons of the HWND.
  bool enabled_;

  DISALLOW_EVIL_CONSTRUCTORS(WebAppIconManager);
};

#endif  // CHROME_BROWSER_WEB_APP_ICON_MANAGER_H__
