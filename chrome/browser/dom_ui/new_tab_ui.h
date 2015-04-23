// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_NEW_TAB_UI_H_
#define CHROME_BROWSER_DOM_UI_NEW_TAB_UI_H_

#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/common/notification_registrar.h"

class GURL;
class PrefService;
class Profile;

namespace {
  class NewTabHTMLSource;
}

// The TabContents used for the New Tab page.
class NewTabUI : public DOMUI,
                 public NotificationObserver {
 public:
  explicit NewTabUI(TabContents* manager);
  ~NewTabUI();

  static void RegisterUserPrefs(PrefService* prefs);

  // Whether we should use the old new tab page.
  static bool UseOldNewTabPage();

  // Whether we should enable the web resources backend service
  static bool EnableWebResources();

 private:
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  NotificationRegistrar registrar_;

  // The message id that should be displayed in this NewTabUIContents
  // instance's motd area.
  int motd_message_id_;

  // Whether the user is in incognito mode or not, used to determine
  // what HTML to load.
  bool incognito_;

  DISALLOW_COPY_AND_ASSIGN(NewTabUI);
};

#endif  // CHROME_BROWSER_DOM_UI_NEW_TAB_UI_H_
