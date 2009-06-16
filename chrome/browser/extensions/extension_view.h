// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_

#include "build/build_config.h"

#include "base/scoped_ptr.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/controls/native/native_view_host.h"

class Browser;
class Extension;
class ExtensionHost;
class ExtensionView;
class RenderViewHost;

// A class that represents the container that this view is in.
// (bottom shelf, side bar, etc.)
class ExtensionContainer {
 public:
  // Mouse event notifications from the view. (useful for hover UI).
  virtual void OnExtensionMouseEvent(ExtensionView* view) = 0;
  virtual void OnExtensionMouseLeave(ExtensionView* view) = 0;
};

// This handles the display portion of an ExtensionHost.
class ExtensionView : public views::NativeViewHost,
                      public NotificationObserver {
 public:
  ExtensionView(ExtensionHost* host, Browser* browser);
  ~ExtensionView();

  ExtensionHost* host() const { return host_; }
  Browser* browser() const { return browser_; }
  Extension* extension() const;
  RenderViewHost* render_view_host() const;

  // Notification from ExtensionHost.
  void DidContentsPreferredWidthChange(const int pref_width);
  void HandleMouseEvent();
  void HandleMouseLeave();

  // Set a custom background for the view. The background will be tiled.
  void SetBackground(const SkBitmap& background);

  // Sets the container for this view.
  void SetContainer(ExtensionContainer* container) { container_ = container; }

  // Overridden from views::NativeViewHost:
  virtual void SetVisible(bool is_visible);
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View *parent, views::View *child);

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend class ExtensionHost;

  // We wait to show the ExtensionView until several things have loaded.
  void ShowIfCompletelyLoaded();

  // The running extension instance that we're displaying.
  // Note that host_ owns view
  ExtensionHost* host_;

  // The browser window that this view is in.
  Browser* browser_;

  // True if we've been initialized.
  bool initialized_;

  // The background the view should have once it is initialized. This is set
  // when the view has a custom background, but hasn't been initialized yet.
  SkBitmap pending_background_;

  // What we should set the preferred width to once the ExtensionView has
  // loaded.
  int pending_preferred_width_;

  // The container this view is in (not necessarily its direct superview).
  // Note: the view does not own its container.
  ExtensionContainer* container_;

  // So that we can track browser window closing.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionView);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
