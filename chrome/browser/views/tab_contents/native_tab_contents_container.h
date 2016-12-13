// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_CONTAINER_H_

class RenderViewHost;
class TabContents;
class TabContentsContainer;
namespace views {
class View;
}

// An interface that the TabContentsContainer uses to talk to a platform-
// specific view that hosts the native handle of the TabContents' view.
class NativeTabContentsContainer {
 public:
  // Creates an appropriate native container for the current platform.
  static NativeTabContentsContainer* CreateNativeContainer(
      TabContentsContainer* container);

  // Attaches the new TabContents to the native container.
  virtual void AttachContents(TabContents* contents) = 0;

  // Detaches the old TabContents from the native container.
  virtual void DetachContents(TabContents* contents) = 0;

  // Tells the container to update less frequently during resizing operations
  // so performance is better.
  virtual void SetFastResize(bool fast_resize) = 0;

  // Tells the container that the RenderViewHost for the attached TabContents
  // has changed and it should update focus.
  virtual void RenderViewHostChanged(RenderViewHost* old_host,
                                     RenderViewHost* new_host) = 0;

  // Tells the container that |tab_contents| got the focus.
  virtual void TabContentsFocused(TabContents* tab_contents) = 0;

  // Retrieves the views::View that hosts the TabContents.
  virtual views::View* GetView() = 0;
};

#endif  // CHROME_BROWSER_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_CONTAINER_H_
