// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_CONTAINER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_CONTAINER_H_

#include "base/scoped_ptr.h"
#include "views/view.h"

class TabContents;
class TabContentsContainer;

// A views::View subclass that contains a docked dev tools window associated
// with a TabContents.
class DevToolsContainer : public views::View {
 public:
  explicit DevToolsContainer();
  virtual ~DevToolsContainer();

  // Changes the TabContents for which this container is showing dev tools. Can
  // be NULL.
  void ChangeTabContents(TabContents* tab_contents);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

 private:
  TabContents* GetDevToolsContents(TabContents* tab_contents);

  TabContents* devtools_contents_;

  // The view that contains the dev tools for selected TabContents.
  TabContentsContainer* contents_container_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsContainer);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_CONTAINER_H_
