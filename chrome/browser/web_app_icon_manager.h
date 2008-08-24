// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

