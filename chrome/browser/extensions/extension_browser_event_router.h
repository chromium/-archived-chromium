// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_EVENT_ROUTER_H_

#include <map>
#include <vector>
#include <string>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/notification_observer.h"

// The ExtensionBrowserEventRouter listens to Browser window & tab events
// and routes them to listeners inside extension process renderers.
// ExtensionBrowserEventRouter listens to *all* events, but will only route
// events from windows/tabs within a profile to extension processes in the same
// profile.
class ExtensionBrowserEventRouter : public TabStripModelObserver,
                                    public BrowserList::Observer {
 public:
  // Get Browser-Global instance.
  static ExtensionBrowserEventRouter* GetInstance();

  // Must be called once. Subsequent calls have no effect.
  void Init();

  // BrowserList::Observer
  virtual void OnBrowserAdded(const Browser* browser);
  virtual void OnBrowserRemoving(const Browser* browser);
  virtual void OnBrowserSetLastActive(const Browser* browser);

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

  // PageActions.
  void PageActionExecuted(Profile *profile,
                          std::string page_action_id,
                          int tab_id,
                          std::string url);

  // NotificationObserver.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);
 private:
  // "Synthetic" event. Called from TabInsertedAt if new tab is detected.
  void TabCreatedAt(TabContents* contents, int index, bool foreground);
  ExtensionBrowserEventRouter();
  friend struct DefaultSingletonTraits<ExtensionBrowserEventRouter>;

  bool initialized_;

  // Maintain some information about known tabs, so we can:
  //
  //  - distinguish between tab creation and tab insertion
  //  - not send tab-detached after tab-removed
  //  - reduce the "noise" of TabChangedAt() when sending events to extensions
  class TabEntry {
   public:
    // Create a new tab entry whose initial state is TAB_COMPLETE.  This
    // constructor is required because TabEntry objects placed inside an
    // std::map<> by value.
    TabEntry();

    // Create a new tab entry whose initial state is derived from the given
    // tab contents.
    explicit TabEntry(const TabContents* contents);

    // Returns the current state of the tab.
    ExtensionTabUtil::TabStatus state() const { return state_; }

    // Update the state of the tab based on its TabContents.  Returns true if
    // the state changed, false otherwise.  Whether the state has changed or not
    // is used to determine if events needs to be sent to extensions during
    // processing of TabChangedAt().
    bool UpdateState(const TabContents* contents);

   private:
    // Tab state used for last notification to extensions.
    ExtensionTabUtil::TabStatus state_;
  };

  std::map<int, TabEntry> tab_entries_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBrowserEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_EVENT_ROUTER_H_
