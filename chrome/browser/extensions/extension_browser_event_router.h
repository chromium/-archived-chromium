// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_EVENT_ROUTER_H_

#include <vector>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/notification_observer.h"

// The ExtensionBrowserEventRouter listens to Browser window & tab events
// and routes them to listeners inside extension process renderers.
// ExtensionBrowserEventRouter listens to *all* events, but will only route
// events from windows/tabs within a profile to extension processes in the same
// profile.
class ExtensionBrowserEventRouter : public TabStripModelObserver,
                                    public NotificationObserver {
 public:
  // Get Browser-Global instance
  static ExtensionBrowserEventRouter* GetInstance();

  // Must be called once. Subsequent calls have no effect.
  void Init();

  // TabStripModelObserver
  void TabInsertedAt(TabContents* contents, int index, bool foreground);
  void TabClosingAt(TabContents* contents, int index);
  void TabDetachedAt(TabContents* contents, int index);
  void TabSelectedAt(TabContents* old_contents,
                     TabContents* new_contents,
                     int index,
                     bool user_gesture);
  void TabMoved(TabContents* contents, int from_index, int to_index);
  void TabChangedAt(TabContents* contents, int index, bool loading_only);
  void TabStripEmpty();

  // NotificationObserver
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);
 private:
  ExtensionBrowserEventRouter();
  friend struct DefaultSingletonTraits<ExtensionBrowserEventRouter>;

  bool initialized_;

  // Maintain set of known tab ids, so we can distinguish between tab creation
  // and tab insertion. Also used to not send tab-detached after tab-removed.
  std::set<int> tab_ids_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBrowserEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_EVENT_ROUTER_H_
