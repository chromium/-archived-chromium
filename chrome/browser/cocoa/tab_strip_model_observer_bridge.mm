// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/tab_strip_model_observer_bridge.h"

TabStripModelObserverBridge::TabStripModelObserverBridge(TabStripModel* model,
                                                         id controller)
    : controller_(controller), model_(model) {
  DCHECK(model && controller);
  // Register to be a listener on the model so we can get updates and tell
  // |controller_| about them in the future.
  model_->AddObserver(this);
}

TabStripModelObserverBridge::~TabStripModelObserverBridge() {
  // Remove ourselves from receiving notifications.
  model_->RemoveObserver(this);
}

void TabStripModelObserverBridge::TabInsertedAt(TabContents* contents,
                                                int index,
                                                bool foreground) {
  if ([controller_ respondsToSelector:
          @selector(insertTabWithContents:atIndex:inForeground:)]) {
    [controller_ insertTabWithContents:contents
                               atIndex:index
                          inForeground:foreground];
  }
}

void TabStripModelObserverBridge::TabClosingAt(TabContents* contents,
                                               int index) {
  if ([controller_ respondsToSelector:
          @selector(tabClosingWithContents:atIndex:)]) {
    [controller_ tabClosingWithContents:contents atIndex:index];
  }
}

void TabStripModelObserverBridge::TabDetachedAt(TabContents* contents,
                                                int index) {
  if ([controller_ respondsToSelector:
          @selector(tabDetachedWithContents:atIndex:)]) {
    [controller_ tabDetachedWithContents:contents atIndex:index];
  }
}

void TabStripModelObserverBridge::TabSelectedAt(TabContents* old_contents,
                                                TabContents* new_contents,
                                                int index,
                                                bool user_gesture) {
  if ([controller_ respondsToSelector:
          @selector(selectTabWithContents:previousContents:atIndex:
                    userGesture:)]) {
    [controller_ selectTabWithContents:new_contents
                      previousContents:old_contents
                               atIndex:index
                           userGesture:user_gesture];
  }
}

void TabStripModelObserverBridge::TabMoved(TabContents* contents,
                                           int from_index,
                                           int to_index) {
  if ([controller_ respondsToSelector:
          @selector(tabMovedWithContents:fromIndex:toIndex:)]) {
    [controller_ tabMovedWithContents:contents
                            fromIndex:from_index
                              toIndex:to_index];
  }
}

void TabStripModelObserverBridge::TabChangedAt(TabContents* contents,
                                               int index,
                                               bool loading_only) {
  if ([controller_ respondsToSelector:
          @selector(tabChangedWithContents:atIndex:loadingOnly:)]) {
    [controller_ tabChangedWithContents:contents
                                atIndex:index
                            loadingOnly:loading_only ? YES : NO];
  }
}

void TabStripModelObserverBridge::TabStripEmpty() {
  if ([controller_ respondsToSelector:@selector(tabStripEmpty)])
    [controller_ tabStripEmpty];
}
